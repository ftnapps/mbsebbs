/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: Fidonet mailer
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
 * Software Foundation, 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *****************************************************************************/

#include "../config.h"
#include "../lib/libs.h"
#include "../lib/structs.h"
#include "../lib/users.h"
#include "../lib/records.h"
#include "../lib/clcomm.h"
#include "../lib/common.h"
#include "callstat.h"



callstat *getstatus(faddr *addr)
{
    static callstat	cst;
    FILE		*fp;

    cst.trytime = 0L;
    cst.tryno   = 0;
    cst.trystat = 0;
	
    if ((fp = fopen(stsname(addr), "r"))) {
	fread(&cst, sizeof(callstat), 1, fp);
	fclose(fp);
    }

    return &cst;
}



void putstatus(faddr *addr, int incr, int sts)
{
    FILE	    *fp;
    callstat    *cst;
    int	    j;

    cst = getstatus(addr);
    if ((fp = fopen(stsname(addr), "w"))) {
	if (sts == 0) {
	    j = cst->tryno = 0;
	} else {
	    cst->tryno += incr;
	    srand(getpid());
	    Syslog('d', "putstatus %s, incr=%d, tryno=%d, status=%d", ascfnode(addr, 0xf), incr, cst->tryno, sts);
	    while (TRUE) {
		j = 1+(int) (1.0 * CFG.dialdelay * rand() / (RAND_MAX + 1.0));
		if ((j > (CFG.dialdelay / 10)) && (j > 9))
		    break;
	    }
	    Syslog('d', "Next call allowed over %d seconds", j);
	}

	cst->trystat = sts;
	cst->trytime = time(NULL) + j;

	fwrite(cst, sizeof(callstat), 1, fp);
	fclose(fp);
	if (cst->tryno >= 30)
	    WriteError("Node %s is marked undialable.", ascfnode(addr, 0x1f));
    } else {
	WriteError("$Cannot create status file for node %s", ascfnode(addr,0x1f));
    }
}


