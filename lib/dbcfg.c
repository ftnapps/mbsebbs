/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: Config Database.
 *
 *****************************************************************************
 * Copyright (C) 1997-2002
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

#include "libs.h"
#include "mbse.h"
#include "structs.h"
#include "users.h"
#include "records.h"
#include "dbcfg.h"




void InitConfig(void)
{
	if ((getenv("MBSE_ROOT")) == NULL) {
		printf("Could not get MBSE_ROOT environment variable\n");
		printf("Please set the environment variable ie:\n");
		printf("\"MBSE_ROOT=/opt/mbse;export MBSE_ROOT\"\n\n");
		exit(1);
	}
	LoadConfig();
}



void LoadConfig(void)
{
	FILE	*pDataFile;
	char	*FileName;

	FileName = calloc(PATH_MAX, sizeof(char));
	sprintf(FileName, "%s/etc/config.data", getenv("MBSE_ROOT"));
	if ((pDataFile = fopen(FileName, "r")) == NULL) {
		perror("\n\nFATAL ERROR:");
		printf(" Can't open %s\n", FileName);
		printf("Please run mbsetup to create configuration file.\n");
		printf("Or check that your MBSE_ROOT variable is set to the BBS path!\n\n");
		free(FileName);
#ifdef MEMWATCH
		mwTerm();
#endif
		exit(1);
	}

	free(FileName);
	fread(&CFG, sizeof(CFG), 1, pDataFile);
	fclose(pDataFile);
}



int IsOurAka(fidoaddr taka)
{
	int	i;

	for (i = 0; i < 40; i++) {
		if ((taka.zone == CFG.aka[i].zone) &&
		    (taka.net == CFG.aka[i].net) &&
		    (taka.node == CFG.aka[i].node) &&
		    (taka.point == CFG.aka[i].point) &&
		    (CFG.akavalid[i])) 
			return TRUE;
	}
	return FALSE;
}



