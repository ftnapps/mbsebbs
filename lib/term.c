/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: Terminal output routines.
 *
 *****************************************************************************
 * Copyright (C) 1997-2002
 *   
 * Michiel Broek		FIDO:	2:280/2802
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

#define	DB_USERS

#include "libs.h"
#include "structs.h"
#include "users.h"
#include "ansi.h"
#include "records.h"
#include "common.h"


int termmode;			/* 0 = tty, 1 = ANSI			   */



void TermInit(int mode)
{
	termmode = mode;
}



/*
 * Function will print about of enters specified
 */
void Enter(int num)
{
	int i;

	for(i = 0; i < num; i++)
		printf("\n");
}




void pout(int fg, int bg, char *Str)
{
	colour(fg, bg);
	printf(Str);
}



void poutCenter(int fg, int bg, char *Str)
{
	colour(fg, bg);
	Center(Str);
}



void poutCR(int fg, int bg, char *Str)
{
	colour(fg, bg);
	puts(Str);
}



/*
 * Changes ansi background and foreground color
 */
void colour(int fg, int bg)
{
	if (termmode == 1) {
  
		int att=0, fore=37, back=40;

		if (fg<0 || fg>31 || bg<0 || bg>7) {
			printf("ANSI: Illegal colour specified: %i, %i\n", fg, bg);
			return; 
		}

		printf("[");
		if ( fg > 15) {
			printf("5;");
			fg-=16;
		}
		if (fg > 7) {
			att=1;
			fg=fg-8;
		}

		if      (fg==0) fore=30;
		else if (fg==1) fore=34;
		else if (fg==2) fore=32;
		else if (fg==3) fore=36;
		else if (fg==4) fore=31;
		else if (fg==5) fore=35;
		else if (fg==6) fore=33;
		else            fore=37;

		if      (bg==1) back=44;
		else if (bg==2) back=42;
		else if (bg==3) back=46;
		else if (bg==4) back=41;
		else if (bg==5) back=45;
		else if (bg==6) back=43;
		else if (bg==7) back=47;
		else            back=40;
		
		printf("%d;%d;%dm", att, fore, back);
	}
}



void Center(char *string)
{
	int	Strlen;
	int	Maxlen = 70;
	int	i, x, z;
	char	*Str;

	Str = calloc(81, sizeof(char));
	Strlen = strlen(string);

	if(Strlen == Maxlen)
		printf("%s\n", string);
	else {
		x = Maxlen - Strlen;
		z = x / 2;
		for(i = 0; i < z; i++)
			strcat(Str, " ");
		strcat(Str, string);
		printf("%s\n", Str);
	}

	free(Str);
}



void clear()
{
	if (termmode == 1) {
		colour(LIGHTGRAY, BLACK);
		printf(ANSI_HOME);
		printf(ANSI_CLEAR);
	} else
		Enter(1); 
}



/*
 * Moves cursor to specified position
 */
void locate(int y, int x)
{
	if (termmode > 0) {
		if (exitinfo.iScreenLen != 0) {
			if (y > exitinfo.iScreenLen || x > 80) {
				printf("ANSI: Invalid screen coordinates: %i, %i\n", y, x);
				printf("ANSI: exitinfo.iScreenLen: %i\n", exitinfo.iScreenLen);
				return;
			}
		} else {
			if (y > 25 || x > 80) {
				printf("ANSI: Invalid screen coordinates: %i, %i\n", y, x);
				return; 
			}
		}
		printf("\x1B[%i;%iH", y, x);
	}
}



void fLine(int	Len)
{
	int	x;

	if (termmode == 0)
		for (x = 0; x < Len; x++)
			printf("-");

	if (termmode == 1)
		for (x = 0; x < Len; x++)
			printf("%c", 196);

	printf(" \n");
}




void  sLine()
{
	fLine(79);
}



/*
 * curses compatible functions
 */
void mvprintw(int y, int x, const char *format, ...)
{
	char		*outputstr;
	va_list		va_ptr;

	outputstr = calloc(2048, sizeof(char));

	va_start(va_ptr, format);
	vsprintf(outputstr, format, va_ptr);
	va_end(va_ptr);

	locate(y, x);
	printf(outputstr);
	free(outputstr);
}



