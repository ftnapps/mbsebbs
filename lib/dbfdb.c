/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: Files database functions
 *
 *****************************************************************************
 * Copyright (C) 1997-2004
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
#include "mbselib.h"
#include "users.h"
#include "mbsedb.h"




/*
 *  Open files database Area number. Do some checks and abort
 *  if they fail.
 */
struct _fdbarea *mbsedb_OpenFDB(long Area, int Timeout)
{
    char	    *temp;
    struct _fdbarea *fdb_area = NULL;
    int		    Tries = 0;
    FILE	    *fp;

    temp = calloc(PATH_MAX, sizeof(char));
    fdb_area = malloc(sizeof(struct _fdbarea));	    /* Will be freed by CloseFDB */

    sprintf(temp, "%s/var/fdb/file%ld.data", getenv("MBSE_ROOT"), Area);

    /*
     * Open the file database, if it's locked, just wait.
     */
    while (((fp = fopen(temp, "r+")) == NULL) && ((errno == EACCES) || (errno == EAGAIN))) {
	if (++Tries >= (Timeout * 4)) {
	    WriteError("Can't open file area %ld, timeout", Area);
	    free(temp);
	    return NULL;
	}
	msleep(250);
	Syslog('f', "Open file area %ld, try %d", Area, Tries);
    }
    if (fp == NULL) {
	if (errno == ENOENT) {
	    Syslog('+', "Create empty FDB for area %ld", Area);
	    fdbhdr.hdrsize = sizeof(fdbhdr);
	    fdbhdr.recsize = sizeof(fdb);
	    if ((fp = fopen(temp, "w+"))) {
		fwrite(&fdbhdr, sizeof(fdbhdr), 1, fp);
	    }
	}
    } else {
	fread(&fdbhdr, sizeof(fdbhdr), 1, fp);
    }

    /*
     * If still not open, it's fatal.
     */
    if (fp == NULL) {
	WriteError("$Can't open %s", temp);
	free(temp);
	return NULL;
    }

    /*
     * Fix attributes if needed
     */
    chmod(temp, 0660);
    free(temp);

    if ((fdbhdr.hdrsize != sizeof(fdbhdr)) || (fdbhdr.recsize != sizeof(fdb))) {
        WriteError("Files database header in area %d is corrupt (%d:%d)", Area, fdbhdr.hdrsize, fdbhdr.recsize);
	fclose(fdb_area->fp);
	return NULL;
    }

    fseek(fp, 0, SEEK_END);
    if ((ftell(fp) - fdbhdr.hdrsize) % fdbhdr.recsize) {
	WriteError("Files database area %ld is corrupt, unalligned records", Area);
	fclose(fp);
	return NULL;
    }

    /*
     * Point to the first record
     */
    fseek(fp, fdbhdr.hdrsize, SEEK_SET);
    fdb_area->fp = fp;
    fdb_area->locked = 0;
    fdb_area->area = Area;
    return fdb_area;
}



/*
 *  Close current open file area
 */
int mbsedb_CloseFDB(struct _fdbarea *fdb_area)
{
    fclose(fdb_area->fp);
    free(fdb_area);
    return TRUE;
}



/*
 *  Lock Files DataBase
 */
int mbsedb_LockFDB(struct _fdbarea *fdb_area, int Timeout)
{
    int		    rc, Tries = 0;
    struct flock    fl;
    
    fl.l_type	= F_WRLCK;
    fl.l_whence	= SEEK_SET;
    fl.l_start	= 0L;
    fl.l_len	= 1L;
    fl.l_pid	= getpid();

    while ((rc = fcntl(fileno(fdb_area->fp), F_SETLK, &fl)) && ((errno == EACCES) || (errno == EAGAIN))) {
	if (++Tries >= (Timeout * 4)) {
	    fcntl(fileno(fdb_area->fp), F_GETLK, &fl);
	    WriteError("FDB %ld is locked by pid %d", fdb_area->area, fl.l_pid);
	    return FALSE;
	}
	msleep(250);
	Syslog('f', "FDB lock attempt %d", Tries);
    }

    if (rc) {
	WriteError("$FDB %ld lock error", fdb_area->area);
	return FALSE;
    }

    fdb_area->locked = 1;
    return TRUE;
}



/*
 *  Unlock Files DataBase
 */
int mbsedb_UnlockFDB(struct _fdbarea *fdb_area)
{
    struct flock    fl;

    fl.l_type   = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0L;
    fl.l_len    = 1L;
    fl.l_pid    = getpid();

    if (fcntl(fileno(fdb_area->fp), F_SETLK, &fl)) {
	WriteError("$Can't unlock FDB area %ld", fdb_area->area);
    }

    fdb_area->locked = 0;
    return TRUE;
}



int mbsedb_InsertFDB(struct _fdbarea *fdb_area, struct FILE_record frec, int AddAlpha)
{
    char    *temp, *temp2;
    int	    i, Insert, Done = FALSE, Found = FALSE, rc;
    FILE    *fp;
    
    if (mbsedb_LockFDB(fdb_area, 30) == FALSE)
	return FALSE;

    fseek(fdb_area->fp, 0, SEEK_END);
    if (ftell(fdb_area->fp) == fdbhdr.hdrsize) {
	/*
	 * No records yet, simply append this first record.
	 */
	fwrite(&frec, fdbhdr.recsize, 1, fdb_area->fp);
	mbsedb_UnlockFDB(fdb_area);
	return TRUE;
    }

    /*
     * There are files, search the insert point.
     */
    temp = calloc(PATH_MAX, sizeof(char));
    temp2 = calloc(PATH_MAX, sizeof(char));
    sprintf(temp, "%s/var/fdb/file%ld.temp", getenv("MBSE_ROOT"), fdb_area->area);
    fseek(fdb_area->fp, fdbhdr.hdrsize, SEEK_SET);
    Insert = 0;
    do {
	if (fread(&fdb, fdbhdr.recsize, 1, fdb_area->fp) != 1)
	    Done = TRUE;
	if (!Done) {
	    if (strcmp(frec.LName, fdb.LName) == 0) {
		Found = TRUE;
		Insert++;
	    } else if (strcmp(frec.LName, fdb.LName) < 0)
		Found = TRUE;
	    else
		Insert++;
	}
    } while ((!Found) && (!Done));

    if (Found) {
	if ((fp = fopen(temp, "a+")) == NULL) {
	    WriteError("$Can't create %s", temp);
	    mbsedb_UnlockFDB(fdb_area);
	    free(temp);
	    return FALSE;
	}
	fwrite(&fdbhdr, sizeof(fdbhdr), 1, fp);

	fseek(fdb_area->fp, fdbhdr.hdrsize, SEEK_SET);
	/*
	 * Copy entries untill the insert point.
	 */
	for (i = 0; i < Insert; i++) {
	    fread(&fdb, fdbhdr.recsize, 1, fdb_area->fp);
	    /*
	     * If we see a magic that is the new magic, remove
	     * the old one.
	     */
	    if (strlen(frec.Magic) && (strcmp(fdb.Magic, frec.Magic) == 0)) {
		memset(&fdb.Magic, 0, sizeof(fdb.Magic));
	    }

	    /*
	     * Check if we are importing a file with the same
	     * name, if so, don't copy the original database
	     * record. The file is also overwritten.
	     */
	    if (strcmp(fdb.LName, frec.LName) != 0)
		fwrite(&fdb, fdbhdr.recsize, 1, fp);
	}

	if (AddAlpha) {
	    /*
	     * Insert new entry
	     */
	    fwrite(&frec, fdbhdr.recsize, 1, fp);
	}

	/*
	 * Append the rest of the entries
	 */
	while (fread(&fdb, fdbhdr.recsize, 1, fdb_area->fp) == 1) {
	    /*
	     * If we see a magic that is the new magic, remove
	     * the old one.
	     */
	    if (strlen(frec.Magic) && (strcmp(fdb.Magic, frec.Magic) == 0)) {
		memset(&fdb.Magic, 0, sizeof(fdb.Magic));
	    }

	    /*
	     * Check if we are importing a file with the same
	     * name, if so, don't copy the original database
	     * record. The file is also overwritten.
	     */
	    if (strcmp(fdb.LName, frec.LName) != 0)
		fwrite(&fdb, fdbhdr.recsize, 1, fp);
	}

	if (! AddAlpha) {
	    /*
	     * Append
	     */
	    fwrite(&frec, fdbhdr.recsize, 1, fp);
	}

	/*
	 * Now the trick, some might be waiting for a lock on the original file,
	 * we will give that a new name on disk. Then we move the temp in place.
	 * Finaly remove the old (still locked) original file.
	 */
	sprintf(temp2, "%s/var/fdb/file%ld.data", getenv("MBSE_ROOT"), fdb_area->area);
	sprintf(temp, "%s/var/fdb/file%ld.xxxx", getenv("MBSE_ROOT"), fdb_area->area);
	rc = rename(temp2, temp);
	sprintf(temp, "%s/var/fdb/file%ld.temp", getenv("MBSE_ROOT"), fdb_area->area);
	rc = rename(temp, temp2);
	sprintf(temp, "%s/var/fdb/file%ld.xxxx", getenv("MBSE_ROOT"), fdb_area->area);
	rc = unlink(temp);

	fdb_area->fp = fp;
	fdb_area->locked = 0;
    } else {
	/*
	 * Append new entry
	 */
	fseek(fdb_area->fp, 0, SEEK_END);
	fwrite(&frec, fdbhdr.recsize, 1, fdb_area->fp);
    }

    free(temp);
    free(temp2);

    return TRUE;
}



/*
 * Return -1 if error, else number of purged records
 */
int mbsedb_PackFDB(struct _fdbarea *fdb_area)
{
    char    *temp, *temp2;
    FILE    *fp;
    int	    count = 0;

    fseek(fdb_area->fp, 0, SEEK_END);
    if (ftell(fdb_area->fp) == fdbhdr.hdrsize) {
	return 0;
    }

    if (mbsedb_LockFDB(fdb_area, 30) == FALSE)
	return -1;

    /*
     * There are files, copy the remaining entries
     */
    temp = calloc(PATH_MAX, sizeof(char));
    temp2 = calloc(PATH_MAX, sizeof(char));
    sprintf(temp, "%s/var/fdb/file%ld.temp", getenv("MBSE_ROOT"), fdb_area->area);
    if ((fp = fopen(temp, "a+")) == NULL) {
	WriteError("$Can't create %s", temp);
	mbsedb_UnlockFDB(fdb_area);
	return -1;
    }
    fwrite(&fdbhdr, fdbhdr.hdrsize, 1, fp);
    fseek(fdb_area->fp, fdbhdr.hdrsize, SEEK_SET);

    while (fread(&fdb, fdbhdr.recsize, 1, fdb_area->fp) == 1) {
	if ((!fdb.Deleted) && (!fdb.Double) && (strcmp(fdb.Name, "") != 0))
	    fwrite(&fdb, fdbhdr.recsize, 1, fp);
	else
	    count++;
    }

    /*
     * Now the trick, some might be waiting for a lock on the original file,
     * we will give that a new name on disk. Then we move the temp in place.
     * Finaly remove the old (still locked) original file.
     */
    sprintf(temp2, "%s/var/fdb/file%ld.data", getenv("MBSE_ROOT"), fdb_area->area);
    sprintf(temp, "%s/var/fdb/file%ld.xxxx", getenv("MBSE_ROOT"), fdb_area->area);
    rename(temp2, temp);
    sprintf(temp, "%s/var/fdb/file%ld.temp", getenv("MBSE_ROOT"), fdb_area->area);
    rename(temp, temp2);
    sprintf(temp, "%s/var/fdb/file%ld.xxxx", getenv("MBSE_ROOT"), fdb_area->area);
    unlink(temp);

    fdb_area->fp = fp;
    fdb_area->locked = 0;

    free(temp);
    free(temp2);

    return count;
}


typedef struct _fdbs {
    struct _fdbs        *next;
    struct FILE_record  filrec;
} fdbs;



void fill_fdbs(struct FILE_record, fdbs **);
void fill_fdbs(struct FILE_record filrec, fdbs **fap)
{
    fdbs    *tmp;

    tmp = (fdbs *)malloc(sizeof(fdbs));
    tmp->next = *fap;
    tmp->filrec = filrec;
    *fap = tmp;
}



void tidy_fdbs(fdbs **);
void tidy_fdbs(fdbs **fap)
{
    fdbs    *tmp, *old;

    for (tmp = *fap; tmp; tmp = old) {
	old = tmp->next;
	free(tmp);
    }
    *fap = NULL;
}


int comp_fdbs(fdbs **, fdbs **);


void sort_fdbs(fdbs **);
void sort_fdbs(fdbs **fap)
{
    fdbs    *ta, **vector;
    size_t  n = 0, i;

    if (*fap == NULL)
	return;

    for (ta = *fap; ta; ta = ta->next)
	n++;

    vector = (fdbs **)malloc(n * sizeof(fdbs *));
    i = 0;
    for (ta = *fap; ta; ta = ta->next)
	vector[i++] = ta;

    qsort(vector, n, sizeof(fdbs *), (int(*)(const void*, const void *))comp_fdbs);
    (*fap) = vector[0];
    i = 1;

    for (ta = *fap; ta; ta = ta->next) {
	if (i < n)
	    ta->next = vector[i++];
	else
	    ta->next = NULL;
    }

    free(vector);
    return;
}



int comp_fdbs(fdbs **fap1, fdbs **fap2)
{
    return strcasecmp((*fap1)->filrec.LName, (*fap2)->filrec.LName);
}



/*
 * Sort a files database using the long filenames.
 */
int mbsedb_SortFDB(struct _fdbarea *fdb_area)
{
    fdbs    *fdx = NULL, *tmp;
    char    *temp, *temp2;
    FILE    *fp;
    int	    count = 0;

    fseek(fdb_area->fp, 0, SEEK_END);
    if (ftell(fdb_area->fp) <= (fdbhdr.hdrsize + fdbhdr.recsize)) {
	return 0;
    }

    fseek(fdb_area->fp, fdbhdr.hdrsize, SEEK_SET);

    while (fread(&fdb, fdbhdr.recsize, 1, fdb_area->fp) == 1) {
	fill_fdbs(fdb, &fdx);
    }

    sort_fdbs(&fdx);
    
    /*
     * Now the most timeconsuming part is done, lock the database and
     * write the new sorted version.
     */
    if (mbsedb_LockFDB(fdb_area, 30) == FALSE) {
	tidy_fdbs(&fdx);
	return -1;
    }
    
    temp  = calloc(PATH_MAX, sizeof(char));
    sprintf(temp, "%s/var/fdb/file%ld.temp", getenv("MBSE_ROOT"), fdb_area->area);
    if ((fp = fopen(temp, "a+")) == NULL) {
        WriteError("$Can't create %s", temp);
        mbsedb_UnlockFDB(fdb_area);
        tidy_fdbs(&fdx);
	free(temp);
        return -1;
    }
    fwrite(&fdbhdr, fdbhdr.hdrsize, 1, fp);

    /*
     * Write sorted files to temp database
     */
    for (tmp = fdx; tmp; tmp = tmp->next) {
        fwrite(&tmp->filrec, fdbhdr.recsize, 1, fp);
	count++;
    }
    tidy_fdbs(&fdx);

    /*
     * Now the trick, some might be waiting for a lock on the original file,
     * we will give that a new name on disk. Then we move the temp in place.
     * Finaly remove the old (still locked) original file.
     */
    temp2 = calloc(PATH_MAX, sizeof(char));
    sprintf(temp2, "%s/var/fdb/file%ld.data", getenv("MBSE_ROOT"), fdb_area->area);
    sprintf(temp, "%s/var/fdb/file%ld.xxxx", getenv("MBSE_ROOT"), fdb_area->area);
    rename(temp2, temp);
    sprintf(temp, "%s/var/fdb/file%ld.temp", getenv("MBSE_ROOT"), fdb_area->area);
    rename(temp, temp2);
    sprintf(temp, "%s/var/fdb/file%ld.xxxx", getenv("MBSE_ROOT"), fdb_area->area);
    unlink(temp);

    fdb_area->fp = fp;
    fdb_area->locked = 0;

    free(temp);
    free(temp2);

    return count;
}


