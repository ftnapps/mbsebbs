/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: Sysop Paging
 * Todo ..................: Implement new config settings.
 *
 *****************************************************************************
 * Copyright (C) 1997-2003
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
 *****************************************************************************/

#include "../config.h"
#include "../lib/libs.h"
#include "../lib/memwatch.h"
#include "../lib/mbse.h"
#include "../lib/structs.h"
#include "../lib/users.h"
#include "../lib/records.h"
#include "../lib/common.h"
#include "../lib/clcomm.h"
#include "dispfile.h"
#include "input.h"
#include "chat.h"
#include "page.h"
#include "timeout.h"
#include "mail.h"
#include "language.h"


extern    pid_t           mypid;


/*
 * Function to Page Sysop
 */
void Page_Sysop(char *String)
{
    int		i;
    char	*Reason, temp[81];
    static char buf[128];

    Reason = calloc(81, sizeof(char));

    clear();
    colour(LIGHTRED, BLACK);
    /* MBSE BBS Chat */
    Center((char *) Language(151));

    if (CFG.iAskReason) {
	locate(6, 0);
	colour(BLUE, BLACK);
	printf("%c", 213);
	for (i = 0; i < 78; i++) 
	    printf("%c", 205);
	printf("%c\n", 184);

	colour(LIGHTGRAY, BLACK);
	for (i = 0; i < 78; i++) 
	    printf("%c", 250);
	printf("\n");

	colour(BLUE, BLACK);
	printf("%c", 212);
	for (i = 0; i < 78; i++) 
	    printf("%c", 205);
	printf("%c\n", 190);

	locate(7, 2);

	colour(LIGHTGRAY, BLACK);
	fflush(stdout);
	GetPageStr(temp, 76);

	colour(BLUE, BLACK);
	printf("%c", 212);
	for (i = 0; i < 78; i++) 
	    printf("%c", 205);
	printf("%c\n", 190);

	if ((strcmp(temp, "")) == 0)
	    return;

	Syslog('+', "Chat Reason: %s", temp);
	strcpy(Reason, temp);
    } else {
	sprintf(Reason, "User want's to chat");
    }


    CFG.iMaxPageTimes--;

    if (CFG.iMaxPageTimes <= 0) {
	if (!DisplayFile((char *)"maxpage")) {
	    /* If i is FALSE display hard coded message */
	    Enter(1);
	    /* You have paged the Sysop the maximum times allowed. */
	    pout(WHITE, BLACK, (char *) Language(154));
	    Enter(2);
	}

	Syslog('!', "Attempted to page Sysop, above maximum page limit.");
	Pause();
	free(Reason);
	return;
    }
	
    locate(14, ((80 - strlen(String) ) / 2 - 2));
    pout(WHITE, BLACK, (char *)"[");
    pout(LIGHTGRAY, BLACK, String);
    pout(WHITE, BLACK, (char *)"]");

    locate(16, ((80 - CFG.iPageLength) / 2 - 2));
    pout(WHITE, BLACK, (char *)"[");
    colour(BLUE, BLACK);
    for (i = 0; i < CFG.iPageLength; i++)
	printf("%c", 176);
    pout(WHITE, BLACK, (char *)"]");

    locate(16, ((80 - CFG.iPageLength) / 2 - 2) + 1);

    sprintf(buf, "CPAG:2,%d,%s;", mypid, Reason);
    if (socket_send(buf)) {
	Syslog('+', "Failed to send message to mbtask");
	free(Reason);
	return;
    }
    strcpy(buf, socket_receive());

    /*
     * Check if sysop is busy
     */
    if (strcmp(buf, "100:1,1;") == 0) {
	/* The SysOp is currently speaking to somebody else */
	pout(LIGHTMAGENTA, BLACK, (char *) Language(152));
	/* Try paging him again in a few minutes ... */
	pout(LIGHTGREEN, BLACK, (char *) Language(153));
	Enter(2);
	Syslog('+', "SysOp was busy chatting with another user");
	Pause();
	free(Reason);
	return;
    }

    /*
     * Check if sysop is not available
     */
    if (strcmp(buf, "100:1,2;") == 0) {
	Syslog('+', "Sysop is not available for chat");
    }

    /*
     * Check for other errors
     */
    if (strcmp(buf, "100:1,3;") == 0) {
	colour(LIGHTRED, BLACK);
	printf("Internal system error, the sysop is informed");
	Enter(2);
	Syslog('!', "Got error on page sysop command");
	Pause();
	free(Reason);
	return;
    }

    if (strcmp(buf, "100:0;") == 0) {
	/*
	 * Page accpeted, wait until sysop responds
	 */
	colour(LIGHTBLUE, BLACK);
	for (i = 0; i < CFG.iPageLength; i++) {
	    printf("%c", 219);
	    fflush(stdout);
	    sleep(1);

	    sprintf(buf, "CISC:1,%d", mypid);
	    if (socket_send(buf) == 0) {
		strcpy(buf, socket_receive());
		if (strcmp(buf, "100:1,1;") == 0) {
		    /*
		     * First cancel page request
		     */
		    sprintf(buf, "CCAN:1,%d", mypid);
		    socket_send(buf);
		    socket_receive();
		    Syslog('+', "Sysop responded to paging request");
		    Chat(exitinfo.Name, (char *)"#sysop");
		    free(Reason);
		    return;
		}
	    }
	}

	/*
	 * Cancel page request
	 */
	sprintf(buf, "CCAN:1,%d", mypid);
	socket_send(buf);
	strcpy(buf, socket_receive());
    }

    PageReason();
    printf("\n\n\n");
    Pause();
    if (strlen(Reason))
	SysopComment(Reason);
    else
	SysopComment((char *)"Failed chat");

    free(Reason);
    Pause();
}



/* 
 * Function gets string from user for Pager Function
 */
void GetPageStr(char *sStr, int iMaxlen)
{
    unsigned char   ch = 0; 
    int		    iPos = 0;

    if ((ttyfd = open ("/dev/tty", O_RDWR)) < 0) {
	perror("open 6");
	return;
    }
    Setraw();

    strcpy(sStr, "");

    alarm_on();
    while (ch != 13) {
	ch = Readkey();

	if (((ch == 8) || (ch == KEY_DEL) || (ch == 127)) && (iPos > 0)) {
	    printf("\b%c\b", 250);
	    fflush(stdout);
	    sStr[--iPos]='\0';
	}

	if (ch > 31 && ch < 127) {
	    if (iPos <= iMaxlen) {
		iPos++;
		sprintf(sStr, "%s%c", sStr, ch);
		printf("%c", ch);
		fflush(stdout);
	    } else
		ch=13;
	}
    }

    Unsetraw();
    close(ttyfd);
    printf("\n");
}



/*
 * Function gets page reason from a file in the txtfiles directory
 * randomly generates a number and prints the string to the screen
 */
void PageReason()
{
    FILE    *Page;
    int	    iLoop = FALSE, id, i, j = 0;
    int	    Lines = 0, Count = 0, iFoundString = FALSE;
    char    *String;
    char    *temp;

    temp = calloc(PATH_MAX, sizeof(char));
    String = calloc(81, sizeof(char));

    sprintf(temp, "%s/page.asc", CFG.bbs_txtfiles);
    if ((Page = fopen(temp, "r")) != NULL) {

	while (( fgets(String, 80 ,Page)) != NULL)
	    Lines++;

	rewind(Page);

	Count = Lines;
	id = getpid();
	do {
	    i = rand();
	    j = i % id;
	    if ((j <= Count) && (j != 0))
		iLoop = 1;
	} while (!iLoop);

	Lines = 0;

	while (( fgets(String,80,Page)) != NULL) {
	    if (Lines == j) {
		Striplf(String);
		locate(18, ((78 - strlen(String) ) / 2));
		pout(15, 0, (char *)"[");
		pout(9, 0, String);
		pout(15, 0, (char *)"]");
		iFoundString = TRUE;
	    }

	    Lines++; /* Increment Lines until correct line is found */
	}
    } /* End of Else */

    if (!iFoundString) {
	/* Sysop currently is not available ... please leave a comment */
	sprintf(String, "%s", (char *) Language(155));
	locate(18, ((78 - strlen(String) ) / 2));
	pout(15, 0, (char *)"[");
	pout(9, 0, String);
	pout(15, 0, (char *)"]");
    }

    free(temp);
    free(String);
}

