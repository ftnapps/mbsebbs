/*****************************************************************************
 *
 * File ..................: mbfido/addpkt.c
 * Purpose ...............: Add mail to .pkt
 * Last modification date : 31-Jul-2001
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
 *****************************************************************************/

#include "../lib/libs.h"
#include "../lib/structs.h"
#include "../lib/records.h"
#include "../lib/clcomm.h"
#include "../lib/common.h"
#include "../lib/dbnode.h"
#include "../lib/dbmsgs.h"
#include "pack.h"
#include "addpkt.h"



static char *months[]={(char *)"Jan",(char *)"Feb",(char *)"Mar",
		       (char *)"Apr",(char *)"May",(char *)"Jun",
		       (char *)"Jul",(char *)"Aug",(char *)"Sep",
		       (char *)"Oct",(char *)"Nov",(char *)"Dec"};

extern int do_unprot;


FILE *CreatePkt(char *, fidoaddr, fidoaddr, char *);
FILE *CreatePkt(char *Queue, fidoaddr Orig, fidoaddr Dest, char *Extension)
{
	FILE		*qp;
	unsigned char	buffer[0x3a];
	time_t		Now;
	int		i;
	struct tm	*Tm;
	char		str[81];

	if ((qp = fopen(Queue, "a")) == NULL) {
		WriteError("$Can't create Queue %s", Queue);
		return NULL;
	}

	/*
	 * Write .PKT header, see FSC-0039 rev. 4
	 */
	memset(&buffer, 0, sizeof(buffer));
	time(&Now);
	Tm = localtime(&Now);
	if (Tm->tm_sec > 59)
		Tm->tm_sec = 59;

	buffer[0x00] = (Orig.node & 0x00ff);
	buffer[0x01] = (Orig.node & 0xff00) >> 8;
	buffer[0x02] = (Dest.node & 0x00ff);
	buffer[0x03] = (Dest.node & 0xff00) >> 8;
	buffer[0x04] = ((Tm->tm_year + 1900) & 0x00ff);
	buffer[0x05] = ((Tm->tm_year + 1900) & 0xff00) >> 8;
	buffer[0x06] = Tm->tm_mon;
	buffer[0x08] = Tm->tm_mday;
	buffer[0x0a] = Tm->tm_hour;
	buffer[0x0c] = Tm->tm_min;
	buffer[0x0e] = Tm->tm_sec;
	buffer[0x12] = 2;
	buffer[0x14] = (Orig.net & 0x00ff);
	buffer[0x15] = (Orig.net & 0xff00) >> 8;
	buffer[0x16] = (Dest.net & 0x00ff);
	buffer[0x17] = (Dest.net & 0xff00) >> 8;
	buffer[0x18] = 0xfe;

	memset(&str, 0, 8);		/* Packet password	*/
	if (SearchNode(Dest)) {
		if (strlen(nodes.Epasswd)) {
			sprintf(str, "%s", nodes.Epasswd);
		}
	}

	for (i = 0; i < 8; i++)
		buffer[0x1a + i] = str[i];

	buffer[0x22] = (Orig.zone & 0x00ff);
	buffer[0x23] = (Orig.zone & 0xff00) >> 8;
	buffer[0x24] = (Dest.zone & 0x00ff);
	buffer[0x25] = (Dest.zone & 0xff00) >> 8;
	buffer[0x29] = 1;
	buffer[0x2c] = 1;
	buffer[0x2e] = buffer[0x22];
	buffer[0x2f] = buffer[0x23];
	buffer[0x30] = buffer[0x24];
	buffer[0x31] = buffer[0x25];
	buffer[0x32] = (Orig.point & 0x00ff);
	buffer[0x33] = (Orig.point & 0xff00) >> 8;
	buffer[0x34] = (Dest.point & 0x00ff);
	buffer[0x35] = (Dest.point & 0xff00) >> 8;
	buffer[0x36] = 'm';
	buffer[0x37] = 'b';
	buffer[0x38] = 's';
	buffer[0x39] = 'e';
	fwrite(buffer, 1, 0x3a, qp);

	fsync(fileno(qp));
	return qp;
}



/*
 *  Open a .pkt file on the queue, create a fresh one if needed.
 *  If CFG.maxpktsize is set then it will add the .pkt to the
 *  ARCmail archive when possible.
 */
FILE *OpenPkt(fidoaddr Orig, fidoaddr Dest, char *Extension)
{
	char	Queue[128], qname[128];
	FILE	*qp;

	sprintf(Queue, "%s/tmp/%d.%d.%d.%d.%s", getenv("MBSE_ROOT"), 
		Dest.zone, Dest.net, Dest.node, Dest.point, Extension);

	if (file_exist(Queue, R_OK))
		qp = CreatePkt(Queue, Orig, Dest, Extension);
	else {
		if ((qp = fopen(Queue, "a")) == NULL) {
			WriteError("$Can't reopen Queue %s", Queue);
			return NULL;
		}

		if (CFG.maxpktsize && (ftell(qp) >= (CFG.maxpktsize * 1024)) &&
		    (strncmp(Extension, "qqq", 3) == 0)) {
			/*
			 * It's a pkt that's meant to be send archived and it's
			 * bigger then maxpktsize. Try to add this pkt to the
			 * outbound archive for this node.
			 */
			sprintf(qname, "%s/tmp", getenv("MBSE_ROOT"));
			chdir(qname);
			sprintf(qname, "%d.%d.%d.%d.qqq", Dest.zone, Dest.net, Dest.node, Dest.point);
			fsync(fileno(qp));
			fclose(qp);
			if (pack_queue(qname) == TRUE) {
				/*
				 * If the pack succeeded create a fresh packet.
				 */
				qp = CreatePkt(Queue, Orig, Dest, Extension);
			} else {
				/*
				 * If the pack failed the existing queue is
				 * reopened and we continue adding to that
				 * existing packet. This is the case when the
				 * node is locked.
				 */
				Syslog('s', "pack_queue failed");
				qp = fopen(Queue, "a");
			}

			/*
			 * Go back to the original inbound directory.
			 */
			if (do_unprot) 
				chdir(CFG.inbound);
			else
				chdir(CFG.pinbound);
		}
	}

	return qp;
}



int AddMsgHdr(FILE *fp, faddr *f, faddr *t, int flags, int cost, time_t date, char *tname, char *fname, char *subj)
{
	unsigned char	buffer[0x0e];
	struct tm	*Tm;

	if ((tname == NULL) || (strlen(tname) > 36) ||
	    (fname == NULL) || (strlen(fname) > 36) ||
	    (subj  == NULL) || (strlen(subj) > 72)) {
		WriteError("AddMsgHdr() error in To name, From name or Subject field");
		return 1;
	}

	buffer[0x00] = 2;
	buffer[0x01] = 0;
	buffer[0x02] = (f->node & 0x00ff);
	buffer[0x03] = (f->node & 0xff00) >> 8;
	buffer[0x04] = (t->node & 0x00ff);
	buffer[0x05] = (t->node & 0xff00) >> 8;
	buffer[0x06] = (f->net & 0x00ff);
	buffer[0x07] = (f->net & 0xff00) >> 8;
	buffer[0x08] = (t->net & 0x00ff);
	buffer[0x09] = (t->net & 0xff00) >> 8;
	buffer[0x0a] = (flags & 0x00ff);
	buffer[0x0b] = (flags & 0xff00) >> 8;
	buffer[0x0c] = (cost & 0x00ff);
	buffer[0x0d] = (cost & 0xff00) >> 8;
	fwrite(buffer, 1, sizeof(buffer), fp);

	if (date == (time_t)0) {
		date = time(NULL);
		Tm = localtime(&date);
	} else
		Tm = gmtime(&date);

	/*
	 * According to the manpage the tm_sec value is in the range 0..61
	 * to allow leap seconds. FTN networks don't allow this, so if this
	 * happens we reset the leap seconds.
	 */
	if (Tm->tm_sec > 59)
		Tm->tm_sec = 59;

	fprintf(fp, "%02d %-3.3s %02d  %02d:%02d:%02d%c",
		Tm->tm_mday % 100, months[Tm->tm_mon], Tm->tm_year % 100,
		Tm->tm_hour % 100, Tm->tm_min % 100, Tm->tm_sec % 100, '\0');

	fprintf(fp, "%s%c", tname, '\0');
	fprintf(fp, "%s%c", fname, '\0');
	fprintf(fp, "%s%c", subj, '\0');
	fsync(fileno(fp));
	return 0;
}

