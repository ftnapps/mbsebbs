/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: New User login under Unix, creates both
 *			    BBS and unix accounts.
 *
 *****************************************************************************
 * Copyright (C) 1997-2004
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
 * Software Foundation, 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *****************************************************************************/

#include "../config.h"
#include "../lib/mbselib.h"
#include "../lib/mbse.h"
#include "../lib/users.h"
#include "funcs.h"
#include "input.h"
#include "newuser.h"
#include "language.h"
#include "timeout.h"
#include "change.h"
#include "dispfile.h"


/*
 * Internal prototypes
 */
char *NameGen(char *);			    /* Get and test for unix login              */
char *NameCreate(char *, char *, char *);   /* Create users login in passwd file	*/
int  BadNames(char *);			    /* Check for Unwanted user names		*/
int  TelephoneScan(char *, char *);	    /* Scans for Duplicate User Phone Numbers   */


/*
 * Variables
 */
extern	int	do_quiet;		    /* No logging to the screen			*/
extern	pid_t	mypid;			    /* Pid of this program			*/
char		UnixName[9];		    /* Unix Name				*/
extern	char	*ieHandle;		    /* Users Handle				*/
extern  time_t  t_start;		    /* Program starttime			*/
int		do_mailout = FALSE;	    /* Just for linking				*/
int		chat_with_sysop = FALSE;    /* Just for linking				*/



/*
 * The main newuser registration function
 */
int newuser()
{
    FILE	    *pUsrConfig;
    int		    i, x, Found, iLang, recno = 0, Count = 0, badname;
    unsigned long   crc;
    char	    temp[PATH_MAX], *FullName, *temp1, *temp2, *Phone1, *Phone2;
    long	    offset;
    struct userrec  us;

    IsDoing("New user login");
    Syslog('+', "Newuser registration");
    clear();
    DisplayFile((char *)"newuser");
    if ((iLang = Chg_Language(TRUE)) == 0)
	Fast_Bye(MBERR_INIT_ERROR);

    Enter(1);
    /* MBSE BBS - NEW USER REGISTRATION */
    language(CYAN, BLACK, 37);
    Enter(2);

    memset(&usrconfig, 0, sizeof(usrconfig));
    memset(&exitinfo, 0, sizeof(exitinfo));

    temp1    = calloc(81, sizeof(char));
    temp2    = calloc(81, sizeof(char));
    Phone1   = calloc(81, sizeof(char));
    Phone2   = calloc(81, sizeof(char));
    FullName = calloc(81, sizeof(char));

    usrconfig.iLanguage = iLang;
    usrconfig.MsgEditor = FSEDIT;

    do {

	/* Please enter your First and Last name: */
	language(CYAN, BLACK, 0);
	fflush(stdout);
	alarm_on();
	Getname(temp, 35);
	strcpy(FullName, tlcap(temp));
	Syslog('+', "Name entered: %s", FullName);
			
	/*
	 * Secret escape name
	 */
	if ((strcasecmp(temp, "off")) == 0) {
	    Syslog('+', "Quick \"off\" logout");
	    Fast_Bye(MBERR_OK);
	}

	Count++;
	if (Count >= CFG.iCRLoginCount) {
	    Enter(1);
	    /* Disconnecting user ... */
	    language(CFG.HiliteF, CFG.HiliteB, 2);
	    Enter(2);
	    Syslog('!', "Exceeded maximum login attempts");
	    Fast_Bye(MBERR_OK);
	}

	/*
	 * Check name, duplicate names, unwanted names, single names, they all get
	 * the same errormessage.
	 */
	badname = (BadNames(temp) || CheckName(temp) || ((strchr(temp, ' ') == NULL) && !CFG.iOneName));
	if (badname) {
	    /* That login name already exists, please choose another one. */
	    language(LIGHTRED, BLACK, 386);
	    Enter(1);
	}

    } while (badname);

    strcpy(FullName, tlcap(temp));

    while (TRUE) {
	Enter(1);
	/* Please enter new password   : */
	language(LIGHTCYAN, BLACK, 39);
	fflush(stdout);
	alarm_on();
	Getpass(temp1);
	if ((x = strlen(temp1)) >= CFG.password_length) {
	    Enter(1);
	    /* Please enter password again : */
	    language(LIGHTCYAN, BLACK, 40);
	    fflush(stdout);
	    alarm_on();
	    Getpass(temp2);
	    if ((i = strcmp(temp1,temp2)) != 0) {
		Enter(2);
		/* Your passwords do not match! Try again. */
		language(LIGHTRED, BLACK, 41);
		Enter(1);
	    } else {
		crc = StringCRC32(tu(temp1));
		break;
	    }
	} else {
	    Enter(2);
	    /* Your password must contain at least */
	    language(LIGHTRED, BLACK, 42);
	    printf("%d ", CFG.password_length);
	    /* characters! Try again. */
	    language(WHITE, BLACK, 43);
	    Enter(1);
	}
    }

    memset(&usrconfig.Password, 0, sizeof(usrconfig.Password));
    sprintf(usrconfig.Password, "%s", temp2);
    alarm_on();
    sprintf(UnixName, "%s", (char *) NameCreate(NameGen(FullName), FullName, temp2));
    UserCity(mypid, UnixName, (char *)"Unknown");

    strcpy(usrconfig.sUserName, FullName);
    strcpy(usrconfig.Name, UnixName);
    Time_Now = time(NULL);
    l_date = localtime(&Time_Now);
    ltime = time(NULL);

    if (CFG.iAnsi) {
	Enter(2);
	/* Do you want ANSI and graphics mode [Y/n]: */
	language(LIGHTGRAY, BLACK, 44);

	alarm_on();
 	i = toupper(getchar());

	if (i == Keystroke(44, 0) || i == '\n')
	    usrconfig.GraphMode = TRUE;
	else
	    usrconfig.GraphMode = FALSE;
    } else {
	usrconfig.GraphMode = TRUE; /* Default set it to ANSI */
	Enter(1);
    }
    exitinfo.GraphMode = usrconfig.GraphMode;
    TermInit(exitinfo.GraphMode, 80, 24);

    if (CFG.iVoicePhone) {
	while (1) {
	    Enter(1);
	    /* Please enter you Voice Number */
	    language(LIGHTGREEN, BLACK, 45);
	    Enter(1);

	    pout(LIGHTGREEN, BLACK, (char *)": ");
	    colour(CFG.InputColourF, CFG.InputColourB);
	    fflush(stdout);
	    alarm_on();
	    GetPhone(temp, 16);

	    if (strlen(temp) < 6) {
		Enter(1);
		/* Please enter a proper phone number */
		language(LIGHTRED, BLACK, 47);
		Enter(1);
	    } else {
		strcpy(usrconfig.sVoicePhone, temp);
		strcpy(Phone1, temp);
		break;
	    }
	}
    } /* End of first if statement */

    if (CFG.iDataPhone) {
	while (TRUE) {
	    Enter(1);
	    /* Please enter you Data Number */
	    language(LIGHTGREEN, BLACK, 48);
	    Enter(1);

	    pout(LIGHTGREEN, BLACK, (char *)": ");
	    colour(CFG.InputColourF, CFG.InputColourB);
	    alarm_on();
	    GetPhone(temp, 16);

	    /*
	     * If no dataphone, copy voicephone.
	     */
	    if (strcmp(temp, "") == 0) {
		strcpy(usrconfig.sDataPhone, usrconfig.sVoicePhone);
		break;
	    }

	    if (strlen(temp) < 6) {
		Enter(1);
		/* Please enter a proper phone number */
		language(LIGHTRED, BLACK, 47);
		Enter(1);
	    } else {
		strcpy(usrconfig.sDataPhone, temp);
		strcpy(Phone2, temp);
		break;
	    }
	}
    } /* End of if Statement */

    if (!CFG.iDataPhone)
	printf("\n");

    if (CFG.iLocation) {
	while (TRUE) {
	    Enter(1);
	    /* Enter your location */
	    language(YELLOW, BLACK, 49);
	    colour(CFG.InputColourF, CFG.InputColourB);
	    alarm_on();
	    if (CFG.iCapLocation) { /* Cap Location is turned on, Capitalise first letter */
		fflush(stdout);
		GetnameNE(temp, 24);
	    } else
		GetstrC(temp, 80);
	
	    if (strlen(temp) < CFG.CityLen) {
		Enter(1);
		/* Please enter a longer location */
		language(LIGHTRED, BLACK, 50);
		Enter(1);
		printf("%s%d)", (char *) Language(74), CFG.CityLen);
		Enter(1);
	    } else {
		strcpy(usrconfig.sLocation, temp);
		UserCity(mypid, UnixName, temp);
		break;
	    }
	}
    }

    if (CFG.AskAddress) {
	while (TRUE) {
	    Enter(1);
	    /* Your address, maximum 3 lines (only visible for the sysop): */
	    language(LIGHTMAGENTA, BLACK, 474);
	    Enter(1);
	    for (i = 0; i < 3; i++) {
		colour(YELLOW, BLACK);
		printf("%d: ", i+1);
		colour(CFG.InputColourF, CFG.InputColourB);
		fflush(stdout);
		alarm_on();
		GetstrC(usrconfig.address[i], 40);
	    }
	    if (strlen(usrconfig.address[0]) || strlen(usrconfig.address[1]) || strlen(usrconfig.address[2]))
		break;
	    Enter(1);
	    /* You need to enter your address here */
	    language(LIGHTRED, BLACK, 475);
	    Enter(1);
	}
    }

    if (CFG.iHandle) {
	do {
	    Enter(1);
	    /* Enter a handle (Enter to Quit): */
	    language(LIGHTRED, BLACK, 412);
	    colour(CFG.InputColourF, CFG.InputColourB);
	    fflush(stdout);
	    alarm_on();
	    Getname(temp, 34);

	    badname = (strlen(temp) && (BadNames(temp) || CheckName(temp) || CheckUnixNames(temp)));
	    if (badname) {
		Syslog('+', "User tried \"%s\" as Handle", MBSE_SS(temp));
		/* That login name already exists, please choose another one. */
		language(LIGHTRED, BLACK, 386);
		Enter(1);
	    } else {
		if(strcmp(temp, "") == 0)
		    strcpy(usrconfig.sHandle, "");
		else
		    strcpy(usrconfig.sHandle, temp);
	    }
	} while (badname);
    }

    /*
     * Note, the users database always contains the english sex
     */
    if (CFG.iSex) {
	while (TRUE) {
	    Enter(1);
	    /* What is your sex? (M)ale or (F)emale: */
	    language(LIGHTBLUE, BLACK, 51);
	    colour(CFG.InputColourF, CFG.InputColourB);
	    fflush(stdout);
	    alarm_on();
	    i = toupper(Getone());

	    if (i == Keystroke(51, 0)) {
		/* Male */
		sprintf(usrconfig.sSex, "Male");
		pout(CFG.InputColourF, CFG.InputColourB, (char *) Language(52));
		Enter(1);
		break;
	    } else if (i == Keystroke(51, 1)) {
		/* Female */
		sprintf(usrconfig.sSex, "Female");
		pout(CFG.InputColourF, CFG.InputColourB, (char *) Language(53));
		Enter(1);
		break;
	    } else {
		Enter(2);
		/* Please answer M or F */
		language(LIGHTRED, BLACK, 54);
		Enter(1);
	    }
	}
    } else /* End of if Statement */
	sprintf(usrconfig.sSex, "Unknown"); /* If set off, set to Unknown */

    if (CFG.iDOB) {
	while (TRUE) {
	    Enter(1);
	    /* Please enter your Date of Birth DD-MM-YYYY: */
	    pout(CYAN, BLACK, (char *) Language(56));
	    colour(CFG.InputColourF, CFG.InputColourB);
	    fflush(stdout);
	    alarm_on();
	    GetDate(temp, 10);

	    sprintf(temp1, "%c%c%c%c", temp[6], temp[7], temp[8], temp[9]);
	    sprintf(temp2, "%02d", l_date->tm_year);
	    iLang = atoi(temp2) + 1900;
	    sprintf(temp2, "%04d", iLang);

	    if ((strcmp(temp1,temp2)) == 0) {
		Enter(1);
		/* Sorry you entered this year by mistake. */
		pout(LIGHTRED, BLACK, (char *) Language(57));
		Enter(1);
	    } else {
		if((strlen(temp)) != 10) {
		    Enter(1);
		    /* Please enter the correct date format */
		    pout(LIGHTRED, BLACK, (char *) Language(58));
		    Enter(1);
		} else {
		    strcpy(usrconfig.sDateOfBirth,temp);
		    break;
		}
	    }
	}
    }

    usrconfig.tFirstLoginDate = ltime; /* Set first login date to current date */
    usrconfig.tLastLoginDate = (time_t)0; /* To force setting new limits */
    strcpy(usrconfig.sExpiryDate,"00-00-0000");
    usrconfig.ExpirySec = CFG.newuser_access;
    usrconfig.Security = CFG.newuser_access;
    usrconfig.Email = CFG.GiveEmail;

    if (CFG.iHotkeys) {
	while (TRUE) {
	    Enter(1);
	    /* Would you like hot-keyed menus [Y/n]: */
	    pout(LIGHTRED, BLACK, (char *) Language(62));
	    colour(CFG.InputColourF, CFG.InputColourB);
	    alarm_on();
	    GetstrC(temp, 8);
	
	    if ((toupper(temp[0]) == Keystroke(62, 0)) || (strcmp(temp,"") == 0)) {
		usrconfig.HotKeys = TRUE;
		break;
	    }
	    if (toupper(temp[0]) == Keystroke(62, 1)) {
		usrconfig.HotKeys = FALSE;
		break;
	    } else {
		/* Please answer Y or N */
		pout(WHITE, BLACK, (char *) Language(63));
	    }
	}
    } else
	usrconfig.HotKeys = TRUE; /* Default set it to Hotkeys */

    usrconfig.iTimeLeft    = 20;  /* Set Timeleft in users file to 20 */

    if (CFG.AskScreenlen) {
	Enter(1);
	/* Please enter your Screen Length [24]: */
	pout(LIGHTMAGENTA, BLACK, (char *) Language(64));
	colour(CFG.InputColourF, CFG.InputColourB);
	fflush(stdout);
	alarm_on();
	Getnum(temp, 3);

	if(strlen(temp) == 0)
	    usrconfig.iScreenLen = 24;
	else
	    usrconfig.iScreenLen = atoi(temp);
    } else {
	usrconfig.iScreenLen = 24;
    }
    TermInit(usrconfig.GraphMode, 80, usrconfig.iScreenLen);
    alarm_on();

    usrconfig.tLastPwdChange  = ltime; /* Days Since Last Password Change */
    usrconfig.iLastFileArea   = 1;
    usrconfig.iLastMsgArea    = 1;

    sprintf(usrconfig.sProtocol, "%s", (char *) Language(65));
    usrconfig.DoNotDisturb = FALSE;

    switch (CFG.AskNewmail) {
	case NO:    usrconfig.MailScan = FALSE;
		    break;
	case YES:   usrconfig.MailScan = TRUE;
		    break;
	default:    while (TRUE) {
			Enter(1);
			/* Check for new mail at login [Y/n]: */
			pout(LIGHTRED, BLACK, (char *) Language(26));
			colour(CFG.InputColourF, CFG.InputColourB);
			alarm_on();
			GetstrC(temp, 8);

			if ((toupper(temp[0]) == Keystroke(26, 0)) || (strcmp(temp,"") == 0)) {
			    usrconfig.MailScan = TRUE;
			    break;
			}
			if (toupper(temp[0]) == Keystroke(26, 1)) {
			    usrconfig.MailScan = FALSE;
			    break;
			}
			/* Please answer Y or N */
			pout(WHITE, BLACK, (char *) Language(63));
		    }
		    break;
    }

    switch (CFG.AskNewfiles) {
	case NO:    usrconfig.ieFILE = FALSE;
		    break;
	case YES:   usrconfig.ieFILE = TRUE;
		    break;
	default:    while (TRUE) {
			Enter(1);
			/* Check for new files at login [Y/n]: */
			pout(LIGHTRED, BLACK, (char *) Language(27));
			colour(CFG.InputColourF, CFG.InputColourB);
			alarm_on();
			GetstrC(temp, 8);

			if ((toupper(temp[0]) == Keystroke(27, 0)) || (strcmp(temp,"") == 0)) {
			    usrconfig.ieFILE = TRUE;
			    break;
			}
			if (toupper(temp[0]) == Keystroke(27, 1)) {
			    usrconfig.ieFILE = FALSE;
			    break;
			}
			/* Please answer Y or N */
			pout(WHITE, BLACK, (char *) Language(63));
		    }
		    break;
    }

    usrconfig.ieNEWS       = TRUE;
    usrconfig.Cls          = TRUE;
    usrconfig.More         = TRUE;
    usrconfig.ieASCII8     = TRUE;

    /*
     * Search a free slot in the users datafile
     */
    sprintf(temp, "%s/etc/users.data", getenv("MBSE_ROOT"));
    if ((pUsrConfig = fopen(temp, "r+")) == NULL) {
	WriteError("Can't open file: %s", temp);
	ExitClient(MBERR_GENERAL);
    }

    fread(&usrconfighdr, sizeof(usrconfighdr), 1, pUsrConfig);
    offset = ftell(pUsrConfig);
    Found = FALSE;

    while ((fread(&us, usrconfighdr.recsize, 1, pUsrConfig) == 1) && (!Found)) {
	if (us.sUserName[0] == '\0') {
	    Found = TRUE;
	} else {
	    offset = ftell(pUsrConfig);
	    recno++;
	}
    }
	
    if (Found)
	fseek(pUsrConfig, offset, SEEK_SET);
    else
	fseek(pUsrConfig, 0, SEEK_END);

    fwrite(&usrconfig, sizeof(usrconfig), 1, pUsrConfig);
    fclose(pUsrConfig);

    Enter(2);
    /* Your user account has been created: */
    pout(YELLOW, BLACK, (char *) Language(67));
    Enter(2);

    /* Login Name : */
    pout(LIGHTBLUE, BLACK, (char *) Language(68));
    colour(CYAN, BLACK);
    printf("%s (%s)\n", UnixName, FullName);
    /* Password   : */
    pout(LIGHTBLUE, BLACK, (char *) Language(69));
    pout(CYAN, BLACK, (char *)"<");
    /* not displayed */
    pout(WHITE, BLACK, (char *) Language(70));
    pout(CYAN, BLACK, (char *)">\n\n");
    fflush(stdout);
    fflush(stdin);
 
    if (CFG.iVoicePhone && TelephoneScan(Phone1, FullName))
	Syslog('!', "Duplicate phone numbers found");

    if (CFG.iDataPhone && TelephoneScan(Phone2, FullName))
	Syslog('!', "Duplicate phone numbers found");
 
    free(temp1);
    free(temp2);
    free(Phone1);
    free(Phone2);

    DisplayFile((char *)"registered");

    Syslog('+', "Completed new-user procedure");
    /* New user registration completed. */
    pout(LIGHTGREEN, BLACK, (char *) Language(71));
    Enter(1);
    /* You need to login again with the name: */
    pout(LIGHTRED, BLACK, (char *) Language(5));
    pout(YELLOW, BLACK, UnixName);
    Enter(2);
    alarm_on();
    Pause();
    alarm_off();
    printf("\n");
    return 0;
}



void Fast_Bye(int onsig)
{
    char    *temp;
    time_t	t_end;

    t_end = time(NULL);
    Syslog(' ', "MBNEWUSR finished in %s", t_elapsed(t_start, t_end));
    socket_shutdown(mypid);
	
    temp = calloc(PATH_MAX, sizeof(char));
    sprintf(temp, "%s/tmp/mbnewusr%d", getenv("MBSE_ROOT"), getpid());
    unlink(temp);
    free(temp);

    colour(LIGHTGRAY, BLACK);
    fflush(stdout);
    fflush(stdin);
    sleep(3);

    Free_Language();
    free(pTTY);
    exit(MBERR_OK);
}



/*
 * This function is the same as Fast_Bye(), it's here
 * to link the other modules properly.
 */
void Good_Bye(int onsig)
{
    Fast_Bye(onsig);
}



/*
 * Function will ask user to create a unix login
 * Name cannot be longer than 8 characters
 */
char *NameGen(char *FidoName)
{
    static char    *sUserName;

    sUserName = calloc(10, sizeof(char));
    Syslog('+', "NameGen(%s)", FidoName);

    while ((strcmp(sUserName, "") == 0) || (CheckUnixNames(sUserName)) || (strlen(sUserName) < 3)) {
	colour(LIGHTRED, BLACK);
	printf("\n%s\n\n", (char *) Language(381));
	colour(WHITE, BLACK);
	/* Please enter a login name (Maximum 8 characters) */
	printf("\n%s\n", (char *) Language(383));
	/* ie. John Doe, login = jdoe */
	printf("%s\n", (char *) Language(384));
	colour(LIGHTGREEN, BLACK);
	/* login > */
	printf("%s", (char *) Language(385));
	fflush(stdout);
	fflush(stdin);
	GetstrU(sUserName, 7);

	if (CheckUnixNames(sUserName)) {
	    /* That login name already exists, please choose another one. */
	    colour(LIGHTRED, BLACK);
	    printf("\n%s\n", (char *) Language(386));
	    Syslog('+', "Users tried to use \"%s\" as Unix name", MBSE_SS(sUserName));
	}
    }
    return sUserName;
}



/*
 * Function will create the users name in the passwd file
 */
char *NameCreate(char *Name, char *Comment, char *Password)
{
    char    *progname, *args[16], *gidstr;
    int	    err;

    progname = calloc(PATH_MAX, sizeof(char));
    gidstr = calloc(10, sizeof(char));
    memset(args, 0, sizeof(args));

    /*
     * Call mbuseradd, this is a special setuid root program to create
     * unix acounts and home directories.
     */
    sprintf(progname, "%s/bin/mbuseradd", getenv("MBSE_ROOT"));
    sprintf(gidstr, "%d", getgid());
    fflush(stdout);
    fflush(stdin);
    args[0] = progname;
    args[1] = gidstr;
    args[2] = Name;
    args[3] = Comment;
    args[4] = CFG.bbs_usersdir;
    args[5] = NULL;
    
    if ((err = execute(args, (char *)"/dev/null", (char *)"/dev/null", (char *)"/dev/null"))) {
        WriteError("Failed to create unix account");
        free(progname);
	free(gidstr);
        ExitClient(MBERR_GENERAL);
    }
    free(gidstr);

    sprintf(progname, "%s/bin/mbpasswd", getenv("MBSE_ROOT"));
    memset(args, 0, sizeof(args));
    args[0] = progname;
    args[1] = (char *)"-f";
    args[2] = Name;
    args[3] = Password;
    args[4] = NULL;
    fflush(stdout);
    fflush(stdin);

    if ((err = execute(args, (char *)"/dev/null", (char *)"/dev/null", (char *)"/dev/null"))) {
        WriteError("Failed to set unix password");
        free(progname);
        ExitClient(MBERR_GENERAL);
    }

    colour(YELLOW, BLACK);
    /* Your "Unix Account" is created, you may use it the next time you call */
    printf("\n%s\n", (char *) Language(382));
    Syslog('+', "Created Unix account %s for %s", Name, Comment);

    free(progname);
    return Name;
}



/*
 * Function will check for unwanted user names
 */
int BadNames(char *Username)
{
    FILE    *fp;
    int	    iFoundName = FALSE;
    char    *temp, *String, *User;

    temp   = calloc(PATH_MAX, sizeof(char));
    String = calloc(81, sizeof(char));
    User   = calloc(81, sizeof(char));

    strcpy(User, tl(Username));

    sprintf(temp, "%s/etc/badnames.ctl", getenv("MBSE_ROOT"));
    if ((fp = fopen(temp, "r")) != NULL) {
        while ((fgets(String, 80, fp)) != NULL) {
            strcpy(String, tl(String));
            Striplf(String);
            if ((strstr(User, String)) != NULL) {
                printf("\nSorry that name is not acceptable on this system\n");
		Syslog('+', "User tried username \"%s\", found in %s", Username, temp);
                iFoundName = TRUE;
                break;
            }
        }
        fclose(fp);
    }

    free(temp);
    free(String);
    free(User);
    return iFoundName;
}



/*
 * Function will Scan Users Database for existing phone numbers. If
 * found, it will write a log entry to the logfile. The user WILL NOT
 * be notified about the same numbers
 */
int TelephoneScan(char *Number, char *Name)
{
    FILE    *fp;
    int     Status = FALSE;
    char    *temp;
    struct  userhdr uhdr;
    struct  userrec u;

    temp  = calloc(PATH_MAX, sizeof(char));

    sprintf(temp, "%s/etc/users.data", getenv("MBSE_ROOT"));
    if ((fp = fopen(temp,"rb")) != NULL) {
        fread(&uhdr, sizeof(uhdr), 1, fp);

        while (fread(&u, uhdr.recsize, 1, fp) == 1) {
            if (strcasecmp(u.sUserName, Name) != 0)
                if ((strlen(u.sVoicePhone) && (strcmp(u.sVoicePhone, Number) == 0)) ||
                    (strlen(u.sDataPhone) &&  (strcmp(u.sDataPhone, Number) == 0))) {
                    Status = TRUE;
                    Syslog('b', "Dupe phones ref: \"%s\" voice: \"%s\" data: \"%s\"", Number, u.sVoicePhone, u.sDataPhone);
                    Syslog('+', "Uses the same telephone number as %s", u.sUserName);
                }
        }
        fclose(fp);
    }

    free(temp);
    return Status;
}


