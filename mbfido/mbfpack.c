/*****************************************************************************
 *
 * $Id$
 * Purpose: File Database Maintenance - Pack filebase
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
#include "../lib/users.h"
#include "../lib/mbsedb.h"
#include "mbfutil.h"
#include "mbfpack.h"



extern int	do_quiet;		/* Suppress screen output	*/
extern int	do_index;		/* Reindex filebases		*/


/*
 *  Removes records who are marked for deletion. If there is still a file
 *  on disk, it will be removed too.
 */
void PackFileBase(void)
{
    FILE    *pAreas;
    int	    i, iAreas, iAreasNew = 0, rc, iTotal = 0, iRemoved = 0;
    char    *sAreas, fn[PATH_MAX];
#ifdef	USE_EXPERIMENT
    struct _fdbarea *fdb_area = NULL;
#else
    FILE    *pFile, *fp;
    char    *fAreas, *fTmp;

    fAreas = calloc(PATH_MAX, sizeof(char));
    fTmp   = calloc(PATH_MAX, sizeof(char));
#endif

    sAreas = calloc(PATH_MAX, sizeof(char));

    IsDoing("Pack filebase");
    if (!do_quiet) {
	colour(3, 0);
	printf("Packing file database...\n");
    }

    sprintf(sAreas, "%s/etc/fareas.data", getenv("MBSE_ROOT"));

    if ((pAreas = fopen (sAreas, "r")) == NULL) {
	WriteError("Can't open %s", sAreas);
	die(MBERR_INIT_ERROR);
    }

    fread(&areahdr, sizeof(areahdr), 1, pAreas);
    fseek(pAreas, 0, SEEK_END);
    iAreas = (ftell(pAreas) - areahdr.hdrsize) / areahdr.recsize;

    for (i = 1; i <= iAreas; i++) {

	fseek(pAreas, ((i-1) * areahdr.recsize) + areahdr.hdrsize, SEEK_SET);
	fread(&area, areahdr.recsize, 1, pAreas);

	if (area.Available && !area.CDrom) {

	    if (enoughspace(CFG.freespace) == 0)
		die(MBERR_DISK_FULL);

	    if (!do_quiet) {
		printf("\r%4d => %-44s", i, area.Name);
		fflush(stdout);
	    }
	    Marker();

#ifdef	USE_EXPERIMENT
	    if ((fdb_area = mbsedb_OpenFDB(i, 30)) == NULL)
		die(MBERR_GENERAL);
#else
	    sprintf(fAreas, "%s/fdb/file%d.data", getenv("MBSE_ROOT"), i);
	    sprintf(fTmp,   "%s/fdb/file%d.temp", getenv("MBSE_ROOT"), i);

	    if ((pFile = fopen(fAreas, "r")) == NULL) {
		Syslog('!', "Creating new %s", fAreas);
		if ((pFile = fopen(fAreas, "a+")) == NULL) {
		    WriteError("$Can't create %s", fAreas);
		    die(MBERR_GENERAL);
		}
		fdbhdr.hdrsize = sizeof(fdbhdr);
		fdbhdr.recsize = sizeof(fdb);
		fwrite(&fdbhdr, sizeof(fdbhdr), 1, pFile);
	    } else {
		fread(&fdbhdr, sizeof(fdbhdr), 1, pFile);
	    } 

	    if ((fp = fopen(fTmp, "a+")) == NULL) {
		WriteError("$Can't create %s", fTmp);
		die(MBERR_GENERAL);
	    }
	    fwrite(&fdbhdr, fdbhdr.hdrsize, 1, fp);
#endif

#ifdef	USE_EXPERIMENT
	    while (fread(&fdb, fdbhdr.recsize, 1, fdb_area->fp) == 1) {
#else
	    while (fread(&fdb, fdbhdr.recsize, 1, pFile) == 1) {
#endif
		iTotal++;

		if ((!fdb.Deleted) && (!fdb.Double) && (strcmp(fdb.Name, "") != 0)) {
#ifndef	USE_EXPERIMENT
		    fwrite(&fdb, fdbhdr.recsize, 1, fp);
#endif
		} else {
		    iRemoved++;
		    if (fdb.Double) {
			Syslog('+', "Removed double record file \"%s\" from area %d", fdb.LName, i);
		    } else {
			Syslog('+', "Removed file \"%s\" from area %d", fdb.LName, i);
			sprintf(fn, "%s/%s", area.Path, fdb.LName);
			rc = unlink(fn);
			if (rc && (errno != ENOENT))
			    Syslog('+', "Unlink %s failed, result %d", fn, rc);
			sprintf(fn, "%s/%s", area.Path, fdb.Name);
			rc = unlink(fn);
			if (rc && (errno != ENOENT))
			    Syslog('+', "Unlink %s failed, result %d", fn, rc);
			/*
			 * If a dotted version (thumbnail) exists, remove it silently
			 */
			sprintf(fn, "%s/.%s", area.Path, fdb.Name);
			unlink(fn);
		    }
		    do_index = TRUE;
		}
	    }

#ifdef	USE_EXPERIMENT
	    mbsedb_PackFDB(fdb_area);
	    mbsedb_CloseFDB(fdb_area);
#else
	    fclose(fp);
	    fclose(pFile);

	    if ((rename(fTmp, fAreas)) == 0) {
		unlink(fTmp);
		chmod(fAreas, 00660);
	    }
#endif
	    iAreasNew++;

	} /* if area.Available */
    }

    fclose(pAreas);
    Syslog('+', "Pack  Areas [%5d] Files [%5d] Removed [%5d]", iAreasNew, iTotal, iRemoved);

    if (!do_quiet) {
	printf("\r                                                              \r");
	fflush(stdout);
    }

#ifndef	USE_EXPERIMENT
    free(fTmp);
    free(fAreas);
#endif
    free(sAreas);
}



