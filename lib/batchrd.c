/*****************************************************************************
 *
 * File ..................: common/batchrd.c
 * Purpose ...............: Batch reading
 * Last modification date : 28-Aug-2000
 *
 *****************************************************************************
 * Copyright (C) 1997-2000
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
#include "libs.h"
#include "structs.h"
#include "clcomm.h"
#include "common.h"


static long	counter = 0L;
static int	batchmode = -1;
int		usetmp = 0;

char *bgets(char *buf, int count, FILE *fp)
{
	if (usetmp) {
		return fgets(buf,count,fp);
	}

	if ((batchmode == 1) && (counter > 0L) && (counter < (long)(count-1))) 
		count=(int)(counter+1L);
	if (fgets(buf,count,fp) == NULL) 
		return NULL;

	switch (batchmode) {
	case -1: if (!strncmp(buf,"#! rnews ",9) || !strncmp(buf,"#!rnews ",8)) {
			batchmode=1;
			sscanf(buf+8,"%ld",&counter);
			Syslog('m', "first chunk of input batch: %ld",counter);
			if (counter < (long)(count-1)) 
				count=(int)(counter+1L);
			if (fgets(buf,count,fp) == NULL) 
				return NULL;
			else {
				counter -= strlen(buf);
				return(buf);
			}
		} else {
			batchmode=0;
			return buf;
		}

	case 0:	return buf;

	case 1:	if (counter <= 0L) {
			while (strncmp(buf,"#! rnews ",9) && strncmp(buf,"#!rnews ",8)) {
				Syslog('+', "batch out of sync: %s",buf);
				if (fgets(buf,count,fp) == NULL) 
					return NULL;
			}
			sscanf(buf+8,"%ld",&counter);
			Syslog('m', "next chunk of input batch: %ld",counter);
			return NULL;
		} else {
			counter -= (long)strlen(buf);
			Syslog('m', "bread \"%s\", %ld left of this chunk", buf,counter);
			return buf;
		}
	}
	return buf;
}


