/*****************************************************************************
 *
 * File ..................: bbs/mail.c
 * Purpose ...............: Message reading and writing.
 * Last modification date : 17-Sep-2001
 * Todo ..................: Implement message groups.
 *
 *****************************************************************************
 * Copyright (C) 1997-2001
 *   
 * Michiel Broek		FIDO:		2:280/2802
 * Beekmansbos 10
 * 1971 BV IJmuiden
 * the Netherlands
 *
 * This file is part of MBSE BBS.
 *
 * This BBS is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * MBSE BBS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with MBSE BBS; see the file COPYING.  If not, write to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************
 *
 * JAM(mbp) - Copyright 1993 Joaquim Homrighausen, Andrew Milner,
 *			     Mats Birch, Mats Wallin.
 *			     ALL RIGHTS RESERVED.
 *
 *****************************************************************************/

#include "../lib/libs.h"
#include "../lib/mbse.h"
#include "../lib/structs.h"
#include "../lib/records.h"
#include "../lib/common.h"
#include "../lib/msgtext.h"
#include "../lib/clcomm.h"
#include "../lib/msg.h"
#include "mail.h"
#include "funcs.h"
#include "funcs4.h"
#include "language.h"
#include "misc.h"
#include "timeout.h"
#include "oneline.h"
#include "exitinfo.h"
#include "lineedit.h"
#include "fsedit.h"
#include "filesub.h"
#include "msgutil.h"
#include "pop3.h"
#include "email.h"



/*
 * Global variables
 */
unsigned long	LastNum;		/* Last read message number	    */
int		Kludges = FALSE;	/* Show kludges or not		    */
int		Line = 1;		/* Line counter in editor	    */
char		*Message[TEXTBUFSIZE +1];/* Message compose text buffer	    */
FILE		*qf;			/* Quote file			    */
extern int	do_mailout;


/*
 *  Internal prototypes
 */

void	ShowMsgHdr(void);		/* Show message header		    */
int	Read_a_Msg(unsigned long Num, int);/* Read a message		    */
int	Export_a_Msg(unsigned long Num);/* Export message to homedir	    */
int	ReadPanel(void);		/* Read panel bar		    */
int	Save_Msg(int, faddr *);		/* Save a message		    */
void	Reply_Msg(int);			/* Reply to message		    */
void	Delete_MsgNum(unsigned long);	/* Delete specified message	    */
int	CheckUser(char *);		/* Check if user exists		    */
int	IsMe(char *);			/* Test if this is my userrecord    */


/****************************************************************************/


/* 
 * More prompt, returns 1 if user decides not to look any further.
 */
int LC(int Lines)
{
	int	z;

	iLineCount += Lines;
 
	if (iLineCount >= exitinfo.iScreenLen && iLineCount < 1000) {
		iLineCount = 1;

		pout(CFG.MoreF, CFG.MoreB, (char *) Language(61));
		fflush(stdout);
		alarm_on();
		z = toupper(Getone());

		if (z == Keystroke(61, 1)) {
			printf("\n");
			return(1);
		}

		if (z == Keystroke(61, 2))
			iLineCount = 50000;

		Blanker(strlen(Language(61)));
	}

	return(0);
}



/*
 * Check if posting is allowed
 */
int Post_Allowed(void);
int Post_Allowed(void)
{
	if (msgs.MsgKinds == RONLY) {
		/* Message area is Readonly */
		pout(12, 0, (char *) Language(437));
		fflush(stdout);
		sleep(3);
		return FALSE;
	}
	return TRUE;
}



/*
 * Check if netmail may be send crash or immediate.
 */
int Crash_Option(faddr *);
int Crash_Option(faddr *Dest)
{
	node		*Nlent;
	int		rc = 0;
	unsigned short	point;

	if (exitinfo.Security.level < CFG.iCrashLevel)
		return 0;

	point = Dest->point;
	Dest->point = 0;

	if (((Nlent = getnlent(Dest)) != NULL) && (Nlent->addr.zone)) {
		if (Nlent->oflags & OL_CM) {
			/* Crash [y/N]: */
			pout(3, 0, (char *)Language(461));
			colour(CFG.MsgInputColourF, CFG.MsgInputColourB);
			fflush(stdout);
			alarm_on();
			if (toupper(Getone()) == Keystroke(461, 0)) {
				rc = 1;
				printf("%c", Keystroke(461, 0));
			} else
				printf("%c", Keystroke(461, 1));
		} else {
			/* Warning: node is not CM, send Immediate [y/N]: */
			pout(3, 0, (char *)Language(462));
			colour(CFG.MsgInputColourF, CFG.MsgInputColourB);
			fflush(stdout);
			alarm_on();
			if (toupper(Getone()) == Keystroke(462, 0)) {
				rc = 2;
				printf("%c", Keystroke(462, 0));
			} else
				printf("%c", Keystroke(462, 1));
		}
		fflush(stdout);
	}

	Dest->point = point;
	return rc;
}



/*
 * Ask if message must be private, only allowed in areas which allow
 * both public and private. Private areas are forced to private.
 */
int IsPrivate(void);
int IsPrivate(void)
{
	int	rc = FALSE;

        if (msgs.MsgKinds == BOTH) {
                Enter(1);
                /* Private [y/N]: */
                pout(3, 0, (char *) Language(163));
                fflush(stdout);
                alarm_on();
                if (toupper(Getone()) == Keystroke(163, 0)) {
                        rc = TRUE;
			printf("%c", Keystroke(163, 0));
		} else {
			printf("%c", Keystroke(163, 1));
		}
		fflush(stdout);
        }

        /*
         * Allways set the private flag in Private areas.
         */
        if (msgs.MsgKinds == PRIVATE)
                rc = TRUE;

	return rc;
}



void Check_Attach(void);
void Check_Attach(void)
{
	char		*Attach, *dospath;
	struct stat	sb;

	/*
	 * This is a dangerous option! Every file on the system to which the
	 * bbs has read access and is in the range of paths translatable by
	 * Unix to DOS can be attached to the netmail.
	 */
	if ((msgs.Type == NETMAIL) && (exitinfo.Security.level >= CFG.iAttachLevel)) {

		Attach = calloc(PATH_MAX, sizeof(char));
		while (TRUE) {
			Enter(1);
			/* Attach file [y/N]: */
			pout(3, 0, (char *)Language(463));
			fflush(stdout);
			alarm_on();
			if (toupper(Getone()) == Keystroke(463, 0)) {

				printf("%c", Keystroke(463, 0));
				Enter(1);
				/* Please enter filename: */
				pout(14, 0, (char *)Language(245));
				colour(CFG.MsgInputColourF, CFG.MsgInputColourB);
				fflush(stdout);
				alarm_on();
				sprintf(Attach, "%s/", CFG.uxpath);
				printf("%s", Attach);
				fflush(stdout);
				GetstrP(Attach, 71, strlen(Attach));
				if (strcmp(Attach, "") == 0)
					break;

				if ((stat(Attach, &sb) == 0) && (S_ISREG(sb.st_mode))) {
					dospath = xstrcpy(Unix2Dos(Attach));
					if (strncasecmp(Attach, CFG.uxpath, strlen(CFG.uxpath)) == 0) {
						Syslog('+', "FileAttach \"%s\"", Attach);
						if (strlen(CFG.dospath))
							strcpy(Msg.Subject, dospath);
						else
							sprintf(Msg.Subject, "%s", Attach);
						Msg.FileAttach = TRUE;
						Enter(1);
						colour(11, 0);
						/* File */ /* will be attached */
						printf("%s %s %s", (char *)Language(464), Msg.Subject, Language(465));
						Enter(1);
						fflush(stdout);
						sleep(2);
						break;
					} else {
						Enter(1);
						colour(10, 0);
						/* File not within */
						printf("%s \"%s\"", Language(466), CFG.uxpath);
						Enter(1);
						Pause();
					}
				} else {
					Enter(1);
					/* File does not exist, please try again ... */
					pout(10, 0, (char *)Language(296));
					Enter(1);
					Pause();
				}
			} else {
				break;
			} /* if attach */
		} /* while true */
		free(Attach);
	}
}



/*
 * Comment to sysop
 */
void SysopComment(char *Cmt)
{
	unsigned long	tmp;
	char		*temp;
	FILE		*fp;

	tmp = iMsgAreaNumber;

	/*
	 *  Make sure that the .quote file is empty.
	 */
	temp = calloc(PATH_MAX, sizeof(char));
	sprintf(temp, "%s/%s/.quote", CFG.bbs_usersdir, exitinfo.Name);
	if ((fp = fopen(temp, "w")) != NULL)
		fclose(fp);
	free(temp);

	SetMsgArea(CFG.iSysopArea -1);
	sprintf(Msg.From, "%s", CFG.sysop_name);
	sprintf(Msg.Subject, "%s", Cmt);
	Reply_Msg(FALSE);

	SetMsgArea(tmp);
}



/*
 * Edit a message. Call the users preffered editor.
 */
int Edit_Msg()
{
	if (exitinfo.FsMsged)
		return Fs_Edit();
	else
		return Line_Edit();
}



/*
 *  Post a message, called from the menu or ReadPanel().
 */
void Post_Msg()
{
	int		i, x;
	char		*FidoNode;
	faddr		*Dest = NULL;
	node		*Nlent;
	unsigned short	point;

	Line = 1;
	WhosDoingWhat(READ_POST);
	SetMsgArea(iMsgAreaNumber);

	clear();
	if (!Post_Allowed())
		return;
	
	for (i = 0; i < (TEXTBUFSIZE + 1); i++)
		Message[i] = (char *) calloc(81, sizeof(char));
	Line = 1;

	Msg_New();

	colour(9, 0);
	/* Posting message in area: */
	printf("\n%s\"%s\"\n", (char *) Language(156), sMsgAreaDesc);
	pout(14, 0, (char *) Language(157));

	if (msgs.Type == NEWS) {
		if (CFG.EmailMode == E_NOISP) {
			/*
			 * If not connected to the internet, use Fido style addressing.
			 */
			Dest = fido2faddr(CFG.EmailFidoAka);
			strcpy(Msg.From, exitinfo.sUserName);
			tlcap(Msg.From);
		} else {
			sprintf(Msg.From, "%s@%s (%s)", exitinfo.Name, CFG.sysdomain, exitinfo.sUserName);
		}
	} else {
		strcpy(Msg.From, exitinfo.sUserName);
		tlcap(Msg.From);
	}
	colour(CFG.MsgInputColourF, CFG.MsgInputColourB);
	printf("%s", Msg.From);
	Syslog('b', "Setting From: %s", Msg.From);

	if (msgs.Type != NEWS) {
		while (TRUE) {
			Enter(1);
			/* To     : */
			pout(14, 0, (char *) Language(158));
	
			colour(CFG.MsgInputColourF, CFG.MsgInputColourB);
			Getname(Msg.To, 35);
	
			if ((strcmp(Msg.To, "")) == 0) {
				for(i = 0; i < (TEXTBUFSIZE + 1); i++)
					free(Message[i]);
				return;
			}

			if ((strcasecmp(Msg.To, "sysop")) == 0)
				strcpy(Msg.To, CFG.sysop_name);

			/*
			 * Localmail and Echomail may be addressed to All
			 */
			if ((msgs.Type == LOCALMAIL) || (msgs.Type == ECHOMAIL)) {
				if (strcasecmp(Msg.To, "all") == 0) 
					x = TRUE;
				else {
					/*
					 * Local users must exist in the userbase.
					 */
					if (msgs.Type == LOCALMAIL) {
						/* Verifying user ... */
						pout(3, 0, (char *) Language(159));
						x = CheckUser(Msg.To);
					} else
						x = TRUE;
				}
			} else if (msgs.Type == NETMAIL) {
				x = FALSE;
				pout(14, 0, (char *)"Address  : ");
				FidoNode = calloc(61, sizeof(char));
				colour(CFG.MsgInputColourF, CFG.MsgInputColourB);
				GetstrC(FidoNode, 60);

				if ((Dest = parsefnode(FidoNode)) != NULL) {
					point = Dest->point;
					Dest->point = 0;
					if (((Nlent = getnlent(Dest)) != NULL) && (Nlent->addr.zone)) {
						colour(14, 0);
						if (point)
							printf("Boss     : ");
						else
							printf("Node     : ");
						Dest->point = point;
						colour(CFG.MsgInputColourF, CFG.MsgInputColourB);
						printf("%s in %s", Nlent->name, Nlent->location);
						colour(14, 0);
						printf(" Is this correct Y/N: ");
						colour(CFG.MsgInputColourF, CFG.MsgInputColourB);
						fflush(stdout);
						alarm_on();

						if (toupper(Getone()) == 'Y') {
							Enter(1);
							sprintf(Msg.ToAddress, "%s", ascfnode(Dest, 0x1f));
							x = TRUE;
							switch (Crash_Option(Dest)) {
								case 1:	Msg.Crash = TRUE;
									break;
								case 2:	Msg.Immediate = TRUE;
									break;
							}
						}
					} else {
						Dest->point = point;
						printf("\r");
						pout(3, 0, (char *) Language(241));
						fflush(stdout);
						alarm_on();
						if (toupper(Getone()) == Keystroke(241, 0)) {
							x = TRUE;
							Syslog('+', "Node %s not found, forced continue", FidoNode);
						}
					}
				} else {
					Syslog('m', "Can't parse address %s", FidoNode);
				}
				free(FidoNode);
			} else {
				x = FALSE;
			}

			if(!x) {
				printf("\r");
				/* User not found. Try again, or (Enter) to quit */
				pout(3, 0, (char *) Language(160));
			} else
				break;
		}
	} else {
		/*
		 * Newsmode, automatic addressing to All.
		 */
		strcpy(Msg.To, "All");
	}

	Enter(1);
	/* Subject  :  */
	pout(14, 0, (char *) Language(161));
	colour(CFG.MsgInputColourF, CFG.MsgInputColourB);
	fflush(stdout);
	alarm_on();
	GetstrP(Msg.Subject, 65, 0);
	tlf(Msg.Subject);

	if((strcmp(Msg.Subject, "")) == 0) {
		Enter(1);
		/* Abort Message [y/N] ?: */
		pout(3, 0, (char *) Language(162));
		fflush(stdout);
		alarm_on();

		if (toupper(Getone()) == Keystroke(162, 0)) {
			for(i = 0; i < (TEXTBUFSIZE + 1); i++)
				free(Message[i]);
			return;
		}
	}

	/*
	 * If not addressed to "all" and the area is Private/Public we
	 * ask the user for the private flag.
	 */
	if ((strcasecmp(Msg.To, "all")) != 0)
		Msg.Private = IsPrivate();

	Check_Attach();

	if (Edit_Msg())
		Save_Msg(FALSE, Dest);

	for (i = 0; i < (TEXTBUFSIZE + 1); i++)
		free(Message[i]);
}



/*
 *  Save the message to disk.
 */
int Save_Msg(int IsReply, faddr *Dest)
{
	int		i;
	char		*temp;
	FILE		*fp;

	if (Line < 2)
		return TRUE;

	if (!Open_Msgbase(msgs.Base, 'w'))
		return FALSE;

	Msg.Arrived = time(NULL) - (gmt_offset((time_t)0) * 60);
	Msg.Written = Msg.Arrived;
	Msg.Local = TRUE;
	temp = calloc(PATH_MAX, sizeof(char));

	if (strlen(Msg.ReplyTo) && (msgs.Type == NETMAIL)) {
		/*
		 *  Send message to internet gateway.
		 */
		Syslog('m', "UUCP message to %s", Msg.ReplyAddr);
		sprintf(Msg.To, "UUCP");
		Add_Headkludges(Dest, IsReply);
		sprintf(temp, "To: %s", Msg.ReplyAddr);
		MsgText_Add2(temp);
		MsgText_Add2((char *)"");
	} else {
		Add_Headkludges(Dest, IsReply);
	}

	/*
	 * Add message text
	 */
	for (i = 1; i <= Line; i++) {
		MsgText_Add2(Message[i]);
	}

	Add_Footkludges(TRUE);

	/*
	 * Save if to disk
	 */
	Msg_AddMsg();
	Msg_UnLock();

	ReadExitinfo();
	exitinfo.iPosted++;
	WriteExitinfo();

	do_mailout = TRUE;

	Syslog('+', "Msg (%ld) to \"%s\", \"%s\", in %ld", Msg.Id, Msg.To, Msg.Subject, iMsgAreaNumber + 1);

	colour(CFG.HiliteF, CFG.HiliteB);
	/* Saving message to disk */
	printf("\n%s(%ld)\n\n", (char *) Language(202), Msg.Id);
	fflush(stdout);
	sleep(2);

	msgs.LastPosted = time(NULL);
	msgs.Posted.total++;
	msgs.Posted.tweek++;
	msgs.Posted.tdow[Diw]++;
	msgs.Posted.month[Miy]++;

	sprintf(temp, "%s/etc/mareas.data", getenv("MBSE_ROOT"));
	
	if ((fp = fopen(temp, "r+")) != NULL) {
		fseek(fp, msgshdr.hdrsize + (iMsgAreaNumber * (msgshdr.recsize + msgshdr.syssize)), SEEK_SET);
		fwrite(&msgs, msgshdr.recsize, 1, fp);
		fclose(fp);
	}

	/*
	 * Add quick mailscan info
	 */
	if (msgs.Type != LOCALMAIL) {
		sprintf(temp, "%s/tmp/%smail.jam", getenv("MBSE_ROOT"), (msgs.Type == ECHOMAIL)? "echo" : "net");
		if ((fp = fopen(temp, "a")) != NULL) {
			fprintf(fp, "%s %lu\n", msgs.Base, Msg.Id);
			fclose(fp);
		}
	}
	free(temp);
	Msg_Close();

	SetMsgArea(iMsgAreaNumber);
	return TRUE;
}



/* 
 * Show message header screen top for reading messages.
 */
void ShowMsgHdr()
{
	static char	Buf1[35], Buf2[35], Buf3[81];
	struct tm	*tm;
	time_t		now;

	Buf1[0] = '\0';
	Buf2[0] = '\0';
	Buf3[0] = '\0';

  	clear();
	colour(1,7);
	printf("   %-70s", sMsgAreaDesc);

	colour(4,7);
	printf("#%-5lu\n", Msg.Id);

	/* Date     : */
	pout(14, 0, (char *) Language(206));
	colour(10, 0);
	/* Use intermediate variable to prevent SIGBUS on Sparc's */
	now = Msg.Written;
	tm = gmtime(&now);
	printf("%02d-%02d-%d %02d:%02d:%02d", tm->tm_mday, tm->tm_mon+1, 
		tm->tm_year+1900, tm->tm_hour, tm->tm_min, tm->tm_sec);
	colour(12, 0);
	if (Msg.Local)		printf(" Local");
	if (Msg.Intransit)	printf(" Transit");
	if (Msg.Private)	printf(" Priv.");
	if (Msg.Received)	printf(" Rcvd");
	if (Msg.Sent)		printf(" Sent");
	if (Msg.KillSent)	printf(" KillSent");
	if (Msg.ArchiveSent)	printf(" ArchiveSent");
	if (Msg.Hold)		printf(" Hold");
	if (Msg.Crash)		printf(" Crash");
	if (Msg.Immediate)	printf(" Imm.");
	if (Msg.Direct)		printf(" Dir");
	if (Msg.Gate)		printf(" Gate");
	if (Msg.FileRequest)	printf(" Freq");
	if (Msg.FileAttach)	printf(" File");
	if (Msg.TruncFile)	printf(" TruncFile");
	if (Msg.KillFile)	printf(" KillFile");
	if (Msg.ReceiptRequest)	printf(" RRQ");
	if (Msg.ConfirmRequest)	printf(" CRQ");
	if (Msg.Orphan)		printf(" Orphan");
	if (Msg.Encrypt)	printf(" Crypt");
	if (Msg.Compressed)	printf(" Comp");
	if (Msg.Escaped)	printf(" 7bit");
	if (Msg.ForcePU)	printf(" FPU");
	if (Msg.Localmail)	printf(" Localmail");
	if (Msg.Netmail)	printf(" Netmail");
	if (Msg.Echomail)	printf(" Echomail");
	if (Msg.News)		printf(" News");
	if (Msg.Email)		printf(" E-mail");
	if (Msg.Nodisplay)	printf(" Nodisp");
	if (Msg.Locked)		printf(" LCK");
	if (Msg.Deleted)	printf(" Del");
	printf("\n");

	/* From    : */
	pout(14,0, (char *) Language(209));
	colour(10, 0);
	printf("%s ", Msg.From);
	if (iMsgAreaType != LOCALMAIL) {
		colour(11, 0);
		printf("(%s)", Msg.FromAddress);
	}
	printf("\n");

	/* To      : */
	pout(14,0, (char *) Language(208));
	colour(10, 0);
	printf("%s ", Msg.To);
	if (iMsgAreaType == NETMAIL) {
		colour(11, 0);
		printf("(%s)", Msg.ToAddress);
	}
	printf("\n");

	/* Subject : */
	pout(14,0, (char *) Language(210));
	colour(10, 0);
	printf("%s\n", Msg.Subject);

	colour(CFG.HiliteF, CFG.HiliteB);
	colour(14, 1);
	if (Msg.Reply)
		sprintf(Buf1, "\"+\" %s %lu", (char *)Language(211), Msg.Reply);
	if (Msg.Original)
		sprintf(Buf2, "   \"-\" %s %lu", (char *)Language(212), Msg.Original);
	sprintf(Buf3, "%s%s ", Buf1, Buf2);
	colour(14, 1);
	printf("%78s  \n", Buf3);
}



/*
 * Export a message to file in the users home directory.
 */
int Export_a_Msg(unsigned long Num)
{
	char            *p;
	int             ShowMsg = TRUE;

	LastNum = Num;
	iLineCount = 7;
	WhosDoingWhat(READ_POST);
	Syslog('+', "Export msg %d in area #%d (%s)", Num, iMsgAreaNumber + 1, sMsgAreaDesc);

	/*
	 * The area data is already set, so we can do the next things
	 */
	if (MsgBase.Total == 0) {
		colour(15, 0);
		/* There are no messages in this area */
		printf("\n%s\n\n", (char *) Language(205));
		sleep(3);
		return FALSE;
	}

	if (!Msg_Open(sMsgAreaBase)) {
		WriteError("Error open JAM base %s", sMsgAreaBase);
		return FALSE;
	}

	if (!Msg_ReadHeader(Num)) {
		perror("");
		colour(15, 0);
		printf("\n%s\n\n", (char *)Language(77));
		Msg_Close();
		sleep(3);
		return FALSE;
	}

	if (Msg.Private) {
		ShowMsg = FALSE;
		if ((strcasecmp(exitinfo.sUserName, Msg.From) == 0) || (strcasecmp(exitinfo.sUserName, Msg.To) == 0))
			ShowMsg = TRUE;
		if (exitinfo.Security.level >= msgs.SYSec.level)
			ShowMsg = TRUE;
	}

	if (!ShowMsg) {
		printf("\n%s\n\n", (char *) Language(82));
		Msg_Close();
		sleep(3);
		return FALSE;
	}

	/*
	 * Export the message text to the file in the users home/wrk directory.
	 * Create the filename as <areanum>_<msgnum>.msg The message is
	 * written in M$DOS <cr/lf> format.
	 */
	p = calloc(128, sizeof(char));
	sprintf(p, "%s/%s/wrk/%d_%lu.msg", CFG.bbs_usersdir, exitinfo.Name, iMsgAreaNumber + 1, Num);
	if ((qf = fopen(p, "w")) != NULL) {
		free(p);
		p = NULL;
		if (Msg_Read(Num, 80)) {
			if ((p = (char *)MsgText_First()) != NULL)
				do {
					if ((p[0] == '\001') || (!strncmp(p, "SEEN-BY:", 8)) || (!strncmp(p, "AREA:", 5))) {
						if (Kludges) {
							if (p[0] == '\001') {
								p[0] = 'a';
								fprintf(qf, "^%s\r\n", p);
							} else
								fprintf(qf, "%s\r\n", p);
						}
					} else
						fprintf(qf, "%s\r\n", p);
				} while ((p = (char *)MsgText_Next()) != NULL);
		}
		fclose(qf);
	} else {
		WriteError("$Can't open %s", p);
	}
	Msg_Close();

	/*
	 * Report the result.
	 */
	colour(CFG.TextColourF, CFG.TextColourB);
	printf("\n\n%s", (char *) Language(46));
	colour(CFG.HiliteF, CFG.HiliteB);
	printf("%d_%lu.msg\n\n", iMsgAreaNumber + 1, Num);
	Pause();
	return TRUE;
}



/*
 * Read a message on screen. Update the lastread pointers,
 * except while scanning and reading new mail at logon.
 */
int Read_a_Msg(unsigned long Num, int UpdateLR)
{
	char		*p = NULL, *fn;
	int		ShowMsg = TRUE;
	lastread	LR;

	LastNum = Num;
	iLineCount = 7;
	WhosDoingWhat(READ_POST);

	/*
	 * The area data is already set, so we can do the next things
	 */
	if (MsgBase.Total == 0) {
		colour(15, 0);
		/* There are no messages in this area */
		printf("\n%s\n\n", (char *) Language(205));
		sleep(3);
		return FALSE;
	}

	if (!Msg_Open(sMsgAreaBase)) {
		WriteError("Error open JAM base %s", sMsgAreaBase);
		return FALSE;
	}

	if (!Msg_ReadHeader(Num)) {
		perror("");
		colour(15, 0);
		printf("\n%s\n\n", (char *)Language(77));
		Msg_Close();
		sleep(3);
		return FALSE;
	}
	if (Msg.Private) {
		ShowMsg = FALSE;
		if ((strcasecmp(exitinfo.sUserName, Msg.From) == 0) || (strcasecmp(exitinfo.sUserName, Msg.To) == 0))
			ShowMsg = TRUE;
		if (exitinfo.Security.level >= msgs.SYSec.level)
			ShowMsg = TRUE;
	} 
	if (!ShowMsg) {
		printf("\n%s\n\n", (char *) Language(82));
		Msg_Close();
		sleep(3);
		return FALSE;
	}
	ShowMsgHdr();

	/*
	 * Fill Quote file in case the user wants to reply. Note that line
	 * wrapping is set lower then normal message read, to create room
	 * for the Quote> strings at the start of each line.
	 */
	fn = calloc(128, sizeof(char));
	sprintf(fn, "%s/%s/.quote", CFG.bbs_usersdir, exitinfo.Name);
	if ((qf = fopen(fn, "w")) != NULL) {
		if (Msg_Read(Num, 75)) {
			if ((p = (char *)MsgText_First()) != NULL)
				do {
					if ((p[0] == '\001') || (!strncmp(p, "SEEN-BY:", 8)) || (!strncmp(p, "AREA:", 5))) {
						if (Kludges) {
							if (p[0] == '\001') {
								p[0] = 'a';
								fprintf(qf, "^%s\n", p);
							} else
								fprintf(qf, "%s\n", p);
						}
					} else
						fprintf(qf, "%s\n", p);
				} while ((p = (char *)MsgText_Next()) != NULL);
		}
		fclose(qf);
	} else {
		WriteError("$Can't open %s", p);
	}
	free(fn);

	/*
	 * Show message text
	 */
	colour(CFG.TextColourF, CFG.TextColourB);
	if (Msg_Read(Num, 78)) {
		if ((p = (char *)MsgText_First()) != NULL) {
			do {
				if ((p[0] == '\001') || (!strncmp(p, "SEEN-BY:", 8)) || (!strncmp(p, "AREA:", 5))) {
					if (Kludges) {
						colour(7, 0);
						printf("%s\n", p);
						if (CheckLine(CFG.TextColourF, CFG.TextColourB, FALSE))
							break;
					}
				} else {
					colour(CFG.TextColourF, CFG.TextColourB);
					if (strchr(p, '>') != NULL)
						if ((strlen(p) - strlen(strchr(p, '>'))) < 10)
							colour(CFG.HiliteF, CFG.HiliteB);
					printf("%s\n", p);
					if (CheckLine(CFG.TextColourF, CFG.TextColourB, FALSE))
						break;
				}
			} while ((p = (char *)MsgText_Next()) != NULL);
		}
	}

	/*
	 * Set the Received status on this message if it's for the user.
	 */
	if ((!Msg.Received) && (strlen(Msg.To) > 0) &&
	    ((strcasecmp(Msg.To, exitinfo.sUserName) == 0) || (strcasecmp(exitinfo.sHandle, Msg.To) == 0))) {
		Syslog('m', "Marking message received");
		Msg.Received = TRUE;
		Msg.Read = time(NULL) - (gmt_offset((time_t)0) * 60);
		if (Msg_Lock(30L)) {
			Msg_WriteHeader(Num);
			Msg_UnLock();
		}
	}

	/*
	 * Update lastread pointer if needed. Netmail boards are always updated.
	 */
	if (Msg_Lock(30L) && (UpdateLR || msgs.Type == NETMAIL)) {
		LR.UserID = grecno;
		p = xstrcpy(exitinfo.sUserName);
		if (Msg_GetLastRead(&LR) == TRUE) {
			LR.LastReadMsg = Num;
			if (Num > LR.HighReadMsg)
				LR.HighReadMsg = Num;
			if (LR.HighReadMsg > MsgBase.Highest)
				LR.HighReadMsg = MsgBase.Highest;
			LR.UserCRC = StringCRC32(tl(p));
			if (!Msg_SetLastRead(LR))
				WriteError("Error update lastread");
		} else {
			/*
			 * Append new lastread pointer
			 */
			LR.UserCRC = StringCRC32(tl(p));
			LR.UserID  = grecno;
			LR.LastReadMsg = Num;
			LR.HighReadMsg = Num;
			if (!Msg_NewLastRead(LR))
				WriteError("Can't append lastread");
		}
		free(p);
		Msg_UnLock();
	}

	Msg_Close();
	return TRUE;
}



/*
 * Read Messages, called from menu
 */
void Read_Msgs()
{
	char		*temp;
	unsigned long	Start;
	lastread	LR;

	colour(CFG.TextColourF, CFG.TextColourB);
	/* Message area \"%s\" contains %lu messages. */
	printf("\n%s\"%s\" %s%lu %s", (char *) Language(221), sMsgAreaDesc, (char *) Language(222), MsgBase.Total, (char *) Language(223));

	/*
	 * Check for lastread pointer, suggest lastread number for start.
	 */
	Start = MsgBase.Lowest;
	if (Msg_Open(sMsgAreaBase)) {
		LR.UserID = grecno;
		if (Msg_GetLastRead(&LR))
			Start = LR.HighReadMsg + 1;
		else
			Start = 1;
		Msg_Close();
		/*
		 * If we already have read the last message, the pointer is
		 * higher then HighMsgNum, we set it at HighMsgNum to prevent
		 * errors and read that message again.
		 */
		if (Start > MsgBase.Highest)
			Start = MsgBase.Highest;
	}

	colour(15, 0);
	/* Please enter a message between */
	printf("\n%s(%lu - %lu)", (char *) Language(224), MsgBase.Lowest, MsgBase.Highest);
	/* Message number [ */
	printf("\n%s%lu]: ", (char *) Language(225), Start);

	temp = calloc(81, sizeof(char));
	colour(CFG.InputColourF, CFG.InputColourB);
	GetstrC(temp, 80);
	if ((strcmp(temp, "")) != 0)
		Start = atoi(temp);
	free(temp);

	if (!Read_a_Msg(Start, TRUE))
		return;

	if (MsgBase.Total == 0)
		return;

	while(ReadPanel()) {}
}



/*
 * The panel bar under the messages while reading
 */
int ReadPanel()
{
	int input;

	WhosDoingWhat(READ_POST);

	colour(15, 4);
 	if (msgs.UsrDelete || exitinfo.Security.level >= CFG.sysop_access) {
		/* (A)gain, (N)ext, (L)ast, (R)eply, (E)nter, (D)elete, (Q)uit, e(X)port */
		printf("%s", (char *) Language(214));
	} else {
		/* (A)gain, (N)ext, (L)ast, (R)eply, (E)nter, (Q)uit, e(X)port */
		printf("%s", (char *) Language(215));
	}
	if (exitinfo.Security.level >= CFG.sysop_access)
		printf(", (!)");

	printf(": ");

	fflush(stdout);
	alarm_on();
	input = toupper(Getone());

	if (input == '!') {
		if (exitinfo.Security.level >= CFG.sysop_access) {
			if (Kludges)
				Kludges = FALSE;
			else
				Kludges = TRUE;
		}
		Read_a_Msg(LastNum, TRUE);
	} else if (input == Keystroke(214, 0)) { /* (A)gain */
		Read_a_Msg(LastNum, TRUE);

	} else if (input == Keystroke(214, 4)) { /* (P)ost */
		Post_Msg();
		Read_a_Msg(LastNum, TRUE);

	} else if (input == Keystroke(214, 2)) { /* (L)ast */
		if (LastNum  > MsgBase.Lowest)
			LastNum--;
		Read_a_Msg(LastNum, TRUE);

	} else if (input == Keystroke(214, 3)) { /* (R)eply */
		Reply_Msg(TRUE);
		Read_a_Msg(LastNum, TRUE);

	} else if (input == Keystroke(214, 5)) { /* (Q)uit */
		/* Quit */
		printf("%s\n", (char *) Language(189));
		return FALSE;

	} else if (input == Keystroke(214, 7)) { /* e(X)port */
		Export_a_Msg(LastNum);
		Read_a_Msg(LastNum, TRUE);

	} else if (input == '+') {
		if (Msg.Reply) 
			LastNum = Msg.Reply;
		Read_a_Msg(LastNum, TRUE);

	} else if (input == '-') {
		if (Msg.Original) 
			LastNum = Msg.Original;
		Read_a_Msg(LastNum, TRUE);

	} else if (input == Keystroke(214, 6)) { /* (D)elete */
		Delete_MsgNum(LastNum);
		if (LastNum < MsgBase.Highest) {
			LastNum++;
			Read_a_Msg(LastNum, TRUE);
		} else {
			return FALSE;
		}
	} else {
		/* Next */
		pout(15, 0, (char *) Language(216));
		if (LastNum < MsgBase.Highest)
			LastNum++;
		else
			return FALSE;
		Read_a_Msg(LastNum, TRUE);
	}
	return TRUE;
}



/*
 *  Reply message, in Msg.From and Msg.Subject must be the
 *  name to reply to and the subject. IsReply is true if the
 *  message is a real reply, and false for forced messages such
 *  as "message to sysop"
 */
void Reply_Msg(int IsReply)
{
	int	i, j, x;
	char	to[65];
	char	from[65];
	char	subj[72];
	char	msgid[81];
	char	replyto[81];
	char	replyaddr[81];
	char	*tmp, *buf;
	char	qin[6];
	faddr	*Dest = NULL;

	if (!Post_Allowed())
		return;

	sprintf(from, "%s", Msg.To);
	sprintf(to, "%s", Msg.From);
	sprintf(replyto, "%s", Msg.ReplyTo);
	sprintf(replyaddr, "%s", Msg.ReplyAddr);
	Dest = parsefnode(Msg.FromAddress);
	Syslog('m', "Parsed from address %s", ascfnode(Dest, 0x1f));

	if (strncasecmp(Msg.Subject, "Re:", 3) && strncasecmp(Msg.Subject, "Re^2:", 5) && IsReply) {
		sprintf(subj, "Re: %s", Msg.Subject);
	} else {
		sprintf(subj, "%s", Msg.Subject);
	}
	Syslog('m', "Reply msg to %s, subject %s", to, subj);
	Syslog('m', "Msgid was %s", Msg.Msgid);
	sprintf(msgid, "%s", Msg.Msgid);

	x = 0;
	WhosDoingWhat(READ_POST);
	clear();
	colour(1,7);
	printf("   %-71s", sMsgAreaDesc);
	colour(4,7);
	printf("#%-5lu", MsgBase.Highest + 1);

	colour(CFG.HiliteF, CFG.HiliteB);
	sLine();

	for (i = 0; i < (TEXTBUFSIZE + 1); i++)
		Message[i] = (char *) calloc(81, sizeof(char));
	Msg_New();

	sprintf(Msg.Replyid, "%s", msgid);
	sprintf(Msg.ReplyTo, "%s", replyto);
	sprintf(Msg.ReplyAddr, "%s", replyaddr);

	/* From     : */
	sprintf(Msg.From, "%s", exitinfo.sUserName);
	pout(14, 0, (char *) Language(209));
	pout(CFG.MsgInputColourF, CFG.MsgInputColourB, Msg.From);
	Enter(1);

	/* To       : */
	sprintf(Msg.To, "%s", to);
	pout(14, 0, (char *) Language(208));
	pout(CFG.MsgInputColourF, CFG.MsgInputColourB, Msg.To);
	Enter(1);

	/* Enter to keep Subject. */
	pout(12, 0, (char *) Language(219));
	Enter(1);
	/* Subject  : */
	pout(14, 0, (char *) Language(210));
	sprintf(Msg.Subject, "%s", subj);
	pout(CFG.MsgInputColourF, CFG.MsgInputColourB, Msg.Subject);

	x = strlen(subj);
	fflush(stdout);
	colour(CFG.MsgInputColourF, CFG.MsgInputColourB);
	GetstrP(subj, 50, x);
	fflush(stdout);

	if (strlen(subj))
		strcpy(Msg.Subject, subj);
	tlf(Msg.Subject);

	Msg.Private = IsPrivate();
	Enter(1);

	/*
	 * If netmail reply and enough security level, allow crashmail.
	 */
	if (msgs.Type == NETMAIL) {
		switch (Crash_Option(Dest)) {
			case 1:	Msg.Crash = TRUE;
				break;
			case 2:	Msg.Immediate = TRUE;
				break;
		}
	}

	Check_Attach();

	/*
	 *  Quote original message now, format the original users
	 *  initials into qin. No quoting if this is a message to Sysop.
	 */
	Line = 1;
	if (IsReply) {
		sprintf(Message[1], "%s wrote to %s:", to, from);
		memset(&qin, 0, sizeof(qin));
		x = TRUE;
		j = 0;
		for (i = 0; i < strlen(to); i++) {
			if (x) {
				qin[j] = to[i];
				j++;
				x = FALSE;
			}
			if (to[i] == ' ' || to[i] == '.')
				x = TRUE;
			if (j == 6)
				break;
		}
		Line = 2;

		tmp = calloc(128, sizeof(char));
		buf = calloc(128, sizeof(char));

		sprintf(tmp, "%s/%s/.quote", CFG.bbs_usersdir, exitinfo.Name);
		if ((qf = fopen(tmp, "r")) != NULL) {
			while ((fgets(buf, 128, qf)) != NULL) {
				Striplf(buf);
				sprintf(Message[Line], "%s> %s", (char *)qin, buf);
				Line++;
				if (Line == TEXTBUFSIZE)
					break;
			}
			fclose(qf);
		} else
			WriteError("$Can't read %s", tmp);

		free(buf);
		free(tmp);
	}

	if (Edit_Msg())
		Save_Msg(IsReply, Dest);

	for (i = 0; i < (TEXTBUFSIZE + 1); i++)
		free(Message[i]);
}



int IsMe(char *Name)
{
	char	*p, *q;
	int	i, rc = FALSE;

	if (strlen(Name) == 0)
		return FALSE;

	if (strcasecmp(Name, exitinfo.sUserName) == 0)
		rc = TRUE;

	if (strcasecmp(Name, exitinfo.sHandle) == 0)
		rc = TRUE;

	q = xstrcpy(Name);
	if (strstr(q, (char *)"@")) {
		p = strtok(q, "@");
		for (i = 0; i < strlen(p); i++)
			if (p[i] == '_')
				p[i] = ' ';
		if (strcasecmp(p, exitinfo.sUserName) == 0)
			rc = TRUE;
		if (strcasecmp(p, exitinfo.sHandle) == 0)
			rc = TRUE;
		if (strcasecmp(p, exitinfo.Name) == 0)
			rc = TRUE; 
	}
	free(q);
	return rc ;  	
}



void QuickScan_Msgs()
{
	int	FoundMsg  = FALSE;
	long	i;

	iLineCount = 2;
	WhosDoingWhat(READ_POST);

	if (MsgBase.Total == 0) {
		Enter(1);
		/* There are no messages in this area. */
		pout(15, 0, (char *) Language(205));
		Enter(3);
		sleep(3);
		return;
	}

  	clear(); 
	/* #    From                  To                       Subject */
	poutCR(14, 1, (char *) Language(220));

	if (Msg_Open(sMsgAreaBase)) {
		for (i = MsgBase.Lowest; i <= MsgBase.Highest; i++) {
			if (Msg_ReadHeader(i)) {
				
				colour(15, 0);
				printf("%-6lu", Msg.Id);
				if (IsMe(Msg.From))
					colour(11, 0);
				else
					colour(3, 0);
				printf("%s ", padleft(Msg.From, 20, ' '));

				if (IsMe(Msg.To))
					colour(10, 0);
				else
					colour(2, 0);
				printf("%s ", padleft(Msg.To, 20, ' '));
				colour(5, 0);
				printf("%s", padleft(Msg.Subject, 31, ' '));
				printf("\n");
				FoundMsg = TRUE;
				if (LC(1))
					break;
			}
		}
		Msg_Close();
	}

	if(!FoundMsg) {
		Enter(1);
		/* There are no messages in this area. */
		pout(10, 0, (char *) Language(205));
		Enter(2);
		sleep(3);
	}

	iLineCount = 2;
	Pause();
}



/*
 *  Called from the menu
 */
void Delete_Msg()
{
	return;
} 



/*
 * Check linecounter for reading messages.
 */
int CheckLine(int FG, int BG, int Email)
{
	int	i, x, z;

	x = strlen(Language(61));
	iLineCount++;

	if ((iLineCount >= (exitinfo.iScreenLen -1)) && (iLineCount < 1000)) {
		iLineCount = 7;

		DoNop();
		pout(CFG.MoreF, CFG.MoreB, (char *) Language(61));

		fflush(stdout);
		alarm_on();
		z = tolower(Getone());

		for (i = 0; i < x; i++)
			putchar('\b');
		for (i = 0; i < x; i++)
			putchar(' ');
		for (i = 0; i < x; i++)
			putchar('\b');
		fflush(stdout);
	
		switch(z) {

		case 'n':
			printf("\n");
			return TRUE;
			break;
		case '=':
			iLineCount = 1000;
		}
		if (Email)
			ShowEmailHdr();
		else
			ShowMsgHdr();
		colour(FG, BG);
	}
	return FALSE;
}



/*
 * Select message area from the list.
 */
void MsgArea_List(char *Option)
{
	FILE	*pAreas;
	int     iAreaCount = 6, Recno = 0; 
	int     iOldArea = 0, iAreaNum = 0;
	int     iGotArea = FALSE; /* Flag to check if user typed in area */
	long    offset;
	char    *temp;

	temp         = calloc(PATH_MAX, sizeof(char));

	sprintf(temp,"%s/etc/mareas.data", getenv("MBSE_ROOT"));

	/*
	 * Save old area, incase he picks a invalid area
	 */
	iOldArea = iMsgAreaNumber;

	if(( pAreas = fopen(temp, "rb")) == NULL) {
		WriteError("Can't open msg areas file: %s", temp);
		free(temp);
		fclose(pAreas);
		return;
	}
	
	/* 
	 * Count how many records there are
	 */
	fread(&msgshdr, sizeof(msgshdr), 1, pAreas);
	fseek(pAreas, 0, SEEK_END);
	iAreaNum = (ftell(pAreas) - msgshdr.hdrsize) / (msgshdr.recsize + msgshdr.syssize);

	/*
	 * If there are menu options, select area direct.
	 */
	if (strlen(Option) != 0) {
		
		if (strcmp(Option, "M+") == 0) 
			while(TRUE) {
				iMsgAreaNumber++;
				if (iMsgAreaNumber >= iAreaNum)
					iMsgAreaNumber = 0;

				offset = msgshdr.hdrsize + (iMsgAreaNumber * (msgshdr.recsize + msgshdr.syssize));
				if(fseek(pAreas, offset, 0) != 0) {
					printf("Can't move pointer there.");
				}
								
				fread(&msgs, msgshdr.recsize, 1, pAreas);
				if ((Access(exitinfo.Security, msgs.RDSec)) && (msgs.Active) && (strlen(msgs.Password) == 0))
					break;
			}
		
		if (strcmp(Option, "M-") == 0) 
			while(TRUE) {
				iMsgAreaNumber--;
				if (iMsgAreaNumber < 0)
					iMsgAreaNumber = iAreaNum -1;

				offset = msgshdr.hdrsize + (iMsgAreaNumber * (msgshdr.recsize + msgshdr.syssize));
				if(fseek(pAreas, offset, 0) != 0) {
					printf("Can't move pointer there.");
				}
				
				fread(&msgs, msgshdr.recsize, 1, pAreas);
				if ((Access(exitinfo.Security, msgs.RDSec)) && (msgs.Active) && (strlen(msgs.Password) == 0))
					break;
			}
		SetMsgArea(iMsgAreaNumber);
		Syslog('+', "Msg area %lu %s", iMsgAreaNumber, sMsgAreaDesc);
		free(temp);
		fclose(pAreas);
		return;
	}

	clear();
	Enter(1);
	pout(CFG.HiliteF, CFG.HiliteB, (char *) Language(231));
	Enter(2);

	fseek(pAreas, msgshdr.hdrsize, 0);
	
	while (fread(&msgs, msgshdr.recsize, 1, pAreas) == 1) {
		/*
		 * Skip the echomail systems
		 */
		fseek(pAreas, msgshdr.syssize, SEEK_CUR);
		if ((Access(exitinfo.Security, msgs.RDSec)) && (msgs.Active)) {
			msgs.Name[31] = '\0';

			colour(15,0);
			printf("%5d", Recno + 1);

			colour(9,0);
			printf(" %c ", 46);

			colour(3,0);
			printf("%-31s", msgs.Name);

			iAreaCount++;

			if ((iAreaCount % 2) == 0)
				printf("\n");
			else
				printf(" ");
		}

		Recno++;

		if((iAreaCount / 2) == exitinfo.iScreenLen) {
			/* More (Y/n/=/Area #): */
			pout(CFG.MoreF, CFG.MoreB, (char *) Language(207));
			/*
			 * Ask user for Area or enter to continue
			 */
			colour(CFG.InputColourF, CFG.InputColourB);
			fflush(stdout);
			GetstrC(temp, 7);

			if (toupper(temp[0]) == Keystroke(207, 1))
				break;

			if ((strcmp(temp, "")) != 0) {
				iGotArea = TRUE;
				break;
			}

			iAreaCount = 2;
		}
	}

	/*
	 * If user type in area above during area listing
	 * don't ask for it again
	 */
	if (!iGotArea) {
		Enter(1);
		pout(CFG.HiliteF, CFG.HiliteB, (char *) Language(232));
		colour(CFG.InputColourF, CFG.InputColourB);
		GetstrC(temp, 80);
	}

	/*
	 * Check if user pressed ENTER
	 */
	if ((strcmp(temp, "")) == 0) {
		fclose(pAreas);
		return;
	}
	iMsgAreaNumber = atoi(temp);
	iMsgAreaNumber--;

	/*
	 * Do a check in case user presses Zero
	 */
	if (iMsgAreaNumber == -1)
		iMsgAreaNumber = 0;

	offset = msgshdr.hdrsize + (iMsgAreaNumber * (msgshdr.recsize + msgshdr.syssize));
	if(fseek(pAreas, offset, 0) != 0) {
		printf("Can't move pointer there.");
	} 
	fread(&msgs, msgshdr.recsize, 1, pAreas);

	/*
	 * Do a check if area is greater or less number than allowed,
	 * security acces (level, flags and age) is oke, and the area
	 * is active.
	 */
	if (iMsgAreaNumber > iAreaNum || iMsgAreaNumber < 0 || (Access(exitinfo.Security, msgs.RDSec) == FALSE) || (!msgs.Active)) {
		Enter(1);
		/*
		 * Invalid area specified - Please try again ...
		 */
		pout(12, 0, (char *) Language(233));
		Enter(2);
		Pause();
		fclose(pAreas);
		iMsgAreaNumber = iOldArea;
		SetMsgArea(iMsgAreaNumber);
		free(temp);
		return;
	}

	SetMsgArea(iMsgAreaNumber);
	Syslog('+', "Msg area %lu %s", iMsgAreaNumber, sMsgAreaDesc);

	/*
	 * Check if msg area has a password, if it does ask user for it
	 */
	if((strlen(msgs.Password)) > 2) {
		Enter(2);
		/* Please enter Area Password: */
		pout(15, 0, (char *) Language(233));
		fflush(stdout);
		colour(CFG.InputColourF, CFG.InputColourB);
		GetstrC(temp, 20);

		if((strcmp(temp, msgs.Password)) != 0) {
			Enter(1);
			pout(15, 0, (char *) Language(234));
			Syslog('!', "Incorrect Message Area # %d password given: %s", iMsgAreaNumber, temp);
			SetMsgArea(iOldArea);
		} else {
			Enter(1);
			pout(15, 0, (char *) Language(235));
			Enter(2);
		}
		Pause();
	}

	free(temp);
	fclose(pAreas);
}



/*
 *  Function deletes a specified message.
 */
void Delete_MsgNum(unsigned long MsgNum)
{
	int	Result = FALSE;

	pout(12, 0, (char *) Language(230));

	if (Msg_Open(sMsgAreaBase)) {
		if (Msg_Lock(15L)) {
			Result = Msg_Delete(MsgNum);
			Msg_UnLock();
		}
		Msg_Close();
	}

	if (Result)
		Syslog('+', "Deleted msg #%lu in Area #%d (%s)", MsgNum, iMsgAreaNumber, sMsgAreaDesc);
	else
		WriteError("ERROR delete msg #%lu in Area #%d (%s)", MsgNum, iMsgAreaNumber, sMsgAreaDesc);
}



/*
 * This Function checks to see if the user exists in the user database
 * and returns a int TRUE or FALSE
 */
int CheckUser(char *To)
{
	FILE		*pUsrConfig;
	int		Found = FALSE;
	char		*temp;
	long		offset;
	unsigned long	Crc;

	temp = calloc(PATH_MAX, sizeof(char));
	sprintf(temp, "%s/etc/users.data", getenv("MBSE_ROOT"));
	if ((pUsrConfig = fopen(temp,"rb")) == NULL) {
		perror("");
		WriteError("Can't open file %s for reading", temp);
		Pause();
		free(temp);
		return FALSE;
	}
	free(temp);
	fread(&usrconfighdr, sizeof(usrconfighdr), 1, pUsrConfig);
	Crc = StringCRC32(tl(To));

	while (fread(&usrconfig, usrconfighdr.recsize, 1, pUsrConfig) == 1) {
		if (StringCRC32(tl(usrconfig.sUserName)) == Crc) {
			Found = TRUE;
			break;
		}
	}

	if (!Found)
		Syslog('!', "User attempted to mail unknown user: %s", tlcap(To));

	/*
	 * Restore users record
	 */
	offset = usrconfighdr.hdrsize + (grecno * usrconfighdr.recsize);
	if (fseek(pUsrConfig, offset, 0) != 0)
		printf("Can't move pointer there.");
	else
		fread(&usrconfig, usrconfighdr.recsize, 1, pUsrConfig);
	fclose(pUsrConfig);

	return Found;
}



/*
 * Check for new mail
 */
void CheckMail()
{
	FILE		*pMsgArea, *Tmp;
	int		x, Found = 0; 
	int		Color, Count = 0, Reading;
	int		OldMsgArea;
	char		*temp;
	char		*sFileName;
	unsigned long	i, Start;
	typedef	struct _Mailrec {
		long		Area;
		unsigned long	Msg;
	} _Mail;
	_Mail		Mail;
	lastread	LR;

	OldMsgArea = iMsgAreaNumber;
	iMsgAreaNumber = 0;
	Syslog('+', "Start checking for new mail");

	clear();
	/* Checking your mail box ... */
	language(10, 0, 150);
	Enter(2);
	Color = 9;
	fflush(stdout);

	/*
	 * Open temporary file
	 */
	if ((Tmp = tmpfile()) == NULL) {
		WriteError("$unable to open temporary file");
		return;
	}

	/*
	 * First check the e-mail mailbox
	 */
	temp = calloc(PATH_MAX, sizeof(char));
	if (exitinfo.Email && strlen(exitinfo.Password)) {
		check_popmail(exitinfo.Name, exitinfo.Password);
		colour(Color, 0);
		printf("\re-mail  Private e-mail mailbox");
		fflush(stdout);
		Color++;
		Count = 0;
		sprintf(temp, "%s/%s/mailbox", CFG.bbs_usersdir, exitinfo.Name);
		SetEmailArea((char *)"mailbox");
		if (Msg_Open(temp)) {
			/*
			 * Check lastread pointer, if none found start
			 * at the begin of the messagebase.
			 */
			LR.UserID = grecno;
			if (Msg_GetLastRead(&LR))
				Start = LR.HighReadMsg + 1;
			else
				Start = EmailBase.Lowest;

			for (i = Start; i <= EmailBase.Highest; i++) {
				if (Msg_ReadHeader(i)) {
					/*
					 * Only check the received status of the email. The mail
					 * may not be direct addressed to this user (aliases database)
					 * but if it is in his mailbox it is always for the user.
					 */
					if (!Msg.Received) {
						/*
						 * Store area and message number in temporary file.
						 */
						Mail.Area = -1; /* Means e-mail mailbox */
						Mail.Msg  = Msg.Id + EmailBase.Lowest -1;
						fwrite(&Mail, sizeof(Mail), 1, Tmp);
						Count++;
						Found++;
					}
				}
			}
			Msg_Close();
		}
		if (Count) {
			colour(CFG.TextColourF, CFG.TextColourB);
			/* messages in */
			printf("\n\n%d %s private e-mail mailbox\n\n", Count, (char *)Language(213));
			Syslog('m', "  %d messages in private e-mail mailbox", Count);
		}
	}

	/*
	 * Open the message base configuration
	 */
	sFileName = calloc(PATH_MAX, sizeof(char));
	sprintf(sFileName,"%s/etc/mareas.data", getenv("MBSE_ROOT"));
	if((pMsgArea = fopen(sFileName, "r+")) == NULL) {
		WriteError("$Can't open: %s", sFileName);
		free(temp);
		free(sFileName);
		return;
	}
	fread(&msgshdr, sizeof(msgshdr), 1, pMsgArea);

	/*
	 * Check all normal areas one by one
	 */
	while (fread(&msgs, msgshdr.recsize, 1, pMsgArea) == 1) {
		fseek(pMsgArea, msgshdr.syssize, SEEK_CUR);
		if ((msgs.Active) && (exitinfo.Security.level >= msgs.RDSec.level)) {
			SetMsgArea(iMsgAreaNumber);
			sprintf(temp, "%d", iMsgAreaNumber + 1);
			colour(Color, 0);
			if (Color < 15)
				Color++;
			else
				Color = 9;
			printf("\r%6s  %-40s", temp, sMsgAreaDesc);
			fflush(stdout);
			Count = 0;
			Nopper();

			if (Msg_Open(sMsgAreaBase)) {
				/*
				 * Check lastread pointer, if none found start
				 * at the begin of the messagebase.
				 */
				LR.UserID = grecno;
				if (Msg_GetLastRead(&LR))
					Start = LR.HighReadMsg + 1;
				else
					Start = MsgBase.Lowest;

				for (i = Start; i <= MsgBase.Highest; i++) {
					if (Msg_ReadHeader(i)) {
						if ((!Msg.Received) && (IsMe(Msg.To))) {
							/*
							 * Store area and message number
							 * in temporary file.
							 */
							Mail.Area = iMsgAreaNumber;
							Mail.Msg  = Msg.Id + MsgBase.Lowest -1;
							fwrite(&Mail, sizeof(Mail), 1, Tmp);
							Count++;
							Found++;
						}
					}
				}
				Msg_Close();
			}
			if (Count) {
				colour(CFG.TextColourF, CFG.TextColourB);
				/* messages in */
				printf("\n\n%d %s %s\n\n", Count, (char *)Language(213), sMsgAreaDesc);
				Syslog('m', "  %d messages in %s", Count, sMsgAreaDesc);
			}
		}
		iMsgAreaNumber++;
	}

	fclose(pMsgArea);
	putchar('\r');
	for (i = 0; i < 48; i++)
		putchar(' ');
	putchar('\r');

	if (Found) {
		colour(14, 0);
		/* You have messages, read your mail now? [Y/n]: */
		printf("\n%s%d %s", (char *) Language(142), Found, (char *) Language(143));
		colour(CFG.InputColourF, CFG.InputColourB);
		fflush(stdout);
		fflush(stdin);
		alarm_on();

		if (toupper(Getone()) != Keystroke(143,1)) {
			rewind(Tmp);
			Reading = TRUE;

			while ((Reading) && (fread(&Mail, sizeof(Mail), 1, Tmp) == 1)) {
				if (Mail.Area == -1) {
					/*
					 * Private e-mail
					 */
					Read_a_Email(Mail.Msg);
				} else {
					SetMsgArea(Mail.Area);
					Read_a_Msg(Mail.Msg, FALSE);
				}
				/* (R)eply, (N)ext, (Q)uit */
				pout(CFG.CRColourF, CFG.CRColourB, (char *)Language(218));
				fflush(stdout);
				fflush(stdin);
				alarm_on();
				x = toupper(Getone());

				if (x == Keystroke(218, 0)) {
					Syslog('m', "  Reply!");
					if (Mail.Area == -1) {
					} else {
						Reply_Msg(TRUE);
					}
				}
				if (x == Keystroke(218, 2)) {
					Syslog('m', "  Quit check for new mail");
					iMsgAreaNumber = OldMsgArea;
					fclose(Tmp);
					SetMsgArea(OldMsgArea);
					printf("\n\n");
					free(temp);
					free(sFileName);
					return;
				}
			}
		}
	} else {
		language(12, 0, 144);
		Enter(1);
		sleep(3);
	} /* if (Found) */

	iMsgAreaNumber = OldMsgArea;
	fclose(Tmp);
	SetMsgArea(OldMsgArea);
	printf("\n\n");
	free(temp);
	free(sFileName);
}



/*
 * Status of all mail areas.
 */
void MailStatus()
{
	FILE		*pMsgArea;
	int		Count = 0;
	int		OldMsgArea;
	char		temp[81];
	char		*sFileName;
	unsigned long	i;

	sFileName = calloc(PATH_MAX, sizeof(char));
	OldMsgArea = iMsgAreaNumber;
	iMsgAreaNumber = 0;
	clear();
	colour(14, 1);
	/* Area Type Description                                   Messages Personal */
	printf("%-79s", (char *)Language(226));
	Enter(1);
	iLineCount = 2;
	fflush(stdout);

	if (exitinfo.Email) {
		sprintf(temp, "%s", sMailbox);
		for (i = 0; i < 3; i++) {
			switch (i) {
				case 0:	SetEmailArea((char *)"mailbox");
					break;
				case 1:	SetEmailArea((char *)"archive");
					break;
				case 2:	SetEmailArea((char *)"trash");
					break;
			}
			colour(12, 0);
			printf("      Email");
			colour(11, 0);
			printf(" %-40s", Language(467 + i));
			colour(14, 0);
			if (EmailBase.Highest)
				printf(" %8lu", EmailBase.Highest - EmailBase.Lowest + 1);
			else
				printf("        0");
			colour(9, 0);
			if (EmailBase.Highest)
				printf(" %8lu\n", EmailBase.Highest - EmailBase.Lowest + 1);
			else
				printf("        0\n");
		}
		iLineCount = 5;
		SetEmailArea(temp);
	}

	/*
	 * Open the message base configuration
	 */
	sprintf(sFileName,"%s/etc/mareas.data", getenv("MBSE_ROOT"));
	if((pMsgArea = fopen(sFileName, "r+")) == NULL) {
		WriteError("Can't open file: %s", sFileName);
		free(sFileName);
		return;
	}
	fread(&msgshdr, sizeof(msgshdr), 1, pMsgArea);

	/*
	 * Check all areas one by one
	 */
	while (fread(&msgs, msgshdr.recsize, 1, pMsgArea) == 1) {
		fseek(pMsgArea, msgshdr.syssize, SEEK_CUR);
		if ((msgs.Active) && (exitinfo.Security.level >= msgs.RDSec.level)) {
			SetMsgArea(iMsgAreaNumber);
			sprintf(temp, "%d", iMsgAreaNumber + 1);
			colour(15, 0);
			printf("%5s", temp);
			colour(12, 0);
			switch(msgs.Type) {
				case LOCALMAIL:
					printf(" Local");
					break;

				case NETMAIL:
					printf(" Net  ");
					break;

				case ECHOMAIL:
					printf(" Echo ");
					break;

				case NEWS:
					printf(" News ");
					break;
			}
			colour(11, 0);
			printf(" %-40s", sMsgAreaDesc);
			Count = 0;

			if (Msg_Open(sMsgAreaBase)) {
				for (i = MsgBase.Lowest; i <= MsgBase.Highest; i++) {
					if (Msg_ReadHeader(i)) {
						if (IsMe(Msg.To) || IsMe(Msg.From))
							Count++;
					}
				}
				Msg_Close();
			} else
				WriteError("Error open JAM %s", sMsgAreaBase);
			colour(14, 0);
			if (MsgBase.Highest)
				printf(" %8lu", MsgBase.Highest - MsgBase.Lowest + 1);
			else
				printf("        0");
			colour(9, 0);
			printf(" %8d\n", Count);
			if (LC(1))
				break;
		}
		iMsgAreaNumber++;
	}

	fclose(pMsgArea);
	SetMsgArea(OldMsgArea);
	free(sFileName);
	Pause();
}



/*
 * Set message area number, set global area description and JAM path
 */
void SetMsgArea(unsigned long AreaNum)
{
	FILE	*pMsgArea;
	long	offset;
	char	*sFileName;

	sFileName = calloc(PATH_MAX, sizeof(char));
	sprintf(sFileName,"%s/etc/mareas.data", getenv("MBSE_ROOT"));
	memset(&msgs, 0, sizeof(msgs));

	if((pMsgArea = fopen(sFileName, "r")) == NULL) {
		WriteError("$Can't open file: %s", sFileName);
		free(sFileName);
		return;
	}

	fread(&msgshdr, sizeof(msgshdr), 1, pMsgArea);
	offset = msgshdr.hdrsize + (AreaNum * (msgshdr.recsize + msgshdr.syssize));
	if(fseek(pMsgArea, offset, 0) != 0) {
		WriteError("$Can't move pointer in %s",sFileName);
		free(sFileName);
		return;
	}

	fread(&msgs, msgshdr.recsize, 1, pMsgArea);
	strcpy(sMsgAreaDesc, msgs.Name);
	strcpy(sMsgAreaBase, msgs.Base);
	iMsgAreaNumber = AreaNum;
	iMsgAreaType = msgs.Type;

	fclose(pMsgArea);

	/*
	 * Get information from the message base
	 */

	if (Msg_Open(sMsgAreaBase)) {

		MsgBase.Lowest  = Msg_Lowest();
		MsgBase.Highest = Msg_Highest();
		MsgBase.Total   = Msg_Number();
		Msg_Close();
	} else
		WriteError("Error open JAM %s", sMsgAreaBase);
	free(sFileName);
}



