/*****************************************************************************
 *
 * File ..................: setup/m_archive.c
 * Purpose ...............: Setup Archive structure.
 * Last modification date : 19-Oct-2001
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
 * MB BBS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with MB BBS; see the file COPYING.  If not, write to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************/

#include "../lib/libs.h"
#include "../lib/structs.h"
#include "../lib/records.h"
#include "../lib/common.h"
#include "../lib/clcomm.h"
#include "screen.h"
#include "mutil.h"
#include "ledit.h"
#include "stlist.h"
#include "m_global.h"
#include "m_archive.h"



int	ArchUpdated = 0;


/*
 * Count nr of archiver records in the database.
 * Creates the database if it doesn't exist.
 */
int CountArchive(void)
{
	FILE	*fil;
	char	ffile[PATH_MAX];
	int	count;

	sprintf(ffile, "%s/etc/archiver.data", getenv("MBSE_ROOT"));
	if ((fil = fopen(ffile, "r")) == NULL) {
		if ((fil = fopen(ffile, "a+")) != NULL) {
			Syslog('+', "Created new %s", ffile);
			archiverhdr.hdrsize = sizeof(archiverhdr);
			archiverhdr.recsize = sizeof(archiver);
			fwrite(&archiverhdr, sizeof(archiverhdr), 1, fil);
			/*
			 *  Create default records
			 */
			memset(&archiver, 0, sizeof(archiver));
			sprintf(archiver.comment, "ARC Version 5.21");
			sprintf(archiver.name,    "ARC");
			archiver.available = TRUE;
			sprintf(archiver.marc,    "/usr/bin/arc anw");
			sprintf(archiver.tarc,    "/usr/bin/arc tnw");
			sprintf(archiver.funarc,  "/usr/bin/arc xnw");
			sprintf(archiver.munarc,  "/usr/bin/arc enw");
			sprintf(archiver.iunarc,  "/usr/bin/arc enw");
			fwrite(&archiver, sizeof(archiver), 1, fil);

                        memset(&archiver, 0, sizeof(archiver));
                        sprintf(archiver.comment, "LHarc");
                        sprintf(archiver.name,    "LHA");
                        archiver.available = TRUE;
                        sprintf(archiver.marc,    "/usr/bin/lha aq");
                        sprintf(archiver.tarc,    "/usr/bin/lha tq");
                        sprintf(archiver.funarc,  "/usr/bin/lha xqf");
                        sprintf(archiver.munarc,  "/usr/bin/lha eqf");
                        sprintf(archiver.iunarc,  "/usr/bin/lha eqf");
                        fwrite(&archiver, sizeof(archiver), 1, fil);

                        memset(&archiver, 0, sizeof(archiver));
                        sprintf(archiver.comment, "RAR by Eugene Roshal");
                        sprintf(archiver.name,    "RAR");
                        archiver.available = TRUE;
                        sprintf(archiver.farc,    "/usr/bin/rar a -y -r");
                        sprintf(archiver.marc,    "/usr/bin/rar a -y");
                        sprintf(archiver.barc,    "/usr/bin/rar c -y");
                        sprintf(archiver.tarc,    "/usr/bin/rar t -y");
                        sprintf(archiver.funarc,  "/usr/bin/unrar x -o+ -y -r");
                        sprintf(archiver.munarc,  "/usr/bin/unrar e -o+ -y");
                        sprintf(archiver.iunarc,  "/usr/bin/unrar e");
                        fwrite(&archiver, sizeof(archiver), 1, fil);

                        memset(&archiver, 0, sizeof(archiver));
                        sprintf(archiver.comment, "TAR the GNU version");
                        sprintf(archiver.name,    "TAR");
                        archiver.available = TRUE;
                        sprintf(archiver.farc,    "/bin/tar cfz");
                        sprintf(archiver.marc,    "/bin/tar Afz");
                        sprintf(archiver.tarc,    "/bin/tar tfz");
                        sprintf(archiver.funarc,  "/bin/tar xfz");
                        sprintf(archiver.munarc,  "/bin/tar xfz");
                        sprintf(archiver.iunarc,  "/bin/tar xfz");
                        fwrite(&archiver, sizeof(archiver), 1, fil);

                        memset(&archiver, 0, sizeof(archiver));
                        sprintf(archiver.comment, "UNARJ by Robert K Jung");
                        sprintf(archiver.name,    "ARJ");
                        archiver.available = TRUE;
                        sprintf(archiver.tarc,    "/usr/bin/unarj t");
                        sprintf(archiver.funarc,  "/usr/bin/unarj x");
                        sprintf(archiver.munarc,  "/usr/bin/unarj e");
                        sprintf(archiver.iunarc,  "/usr/bin/unarj e");
                        fwrite(&archiver, sizeof(archiver), 1, fil);

                        memset(&archiver, 0, sizeof(archiver));
                        sprintf(archiver.comment, "ZIP and UNZIP by Info-ZIP");
                        sprintf(archiver.name,    "ZIP");
                        archiver.available = TRUE;
                        sprintf(archiver.farc,    "/usr/bin/zip -r -q");
                        sprintf(archiver.marc,    "/usr/bin/zip -q");
                        sprintf(archiver.barc,    "/usr/bin/zip -z");
                        sprintf(archiver.tarc,    "/usr/bin/zip -T");
                        sprintf(archiver.funarc,  "/usr/bin/unzip -o -q");
                        sprintf(archiver.munarc,  "/usr/bin/unzip -o -j -L");
                        sprintf(archiver.iunarc,  "/usr/bin/unzip -o -j");
                        fwrite(&archiver, sizeof(archiver), 1, fil);

                        memset(&archiver, 0, sizeof(archiver));
                        sprintf(archiver.comment, "ZOO archiver");
                        sprintf(archiver.name,    "ZOO");
                        archiver.available = TRUE;
                        sprintf(archiver.farc,    "/usr/bin/zoo aq");
                        sprintf(archiver.marc,    "/usr/bin/zoo aq:O");
                        sprintf(archiver.barc,    "/usr/bin/zoo aqC");
                        sprintf(archiver.funarc,  "/usr/bin/zoo xqO");
                        sprintf(archiver.munarc,  "/usr/bin/zoo eq:O");
                        sprintf(archiver.iunarc,  "/usr/bin/zoo eqO");
                        fwrite(&archiver, sizeof(archiver), 1, fil);

			fclose(fil);
			return 7;
		} else
			return -1;
	}

	fread(&archiverhdr, sizeof(archiverhdr), 1, fil);
	fseek(fil, 0, SEEK_END);
	count = (ftell(fil) - archiverhdr.hdrsize) / archiverhdr.recsize;
	fclose(fil);

	return count;
}



/*
 * Open database for editing. The datafile is copied, if the format
 * is changed it will be converted on the fly. All editing must be 
 * done on the copied file.
 */
int OpenArchive(void);
int OpenArchive(void)
{
	FILE	*fin, *fout;
	char	fnin[PATH_MAX], fnout[PATH_MAX];
	long	oldsize;

	sprintf(fnin,  "%s/etc/archiver.data", getenv("MBSE_ROOT"));
	sprintf(fnout, "%s/etc/archiver.temp", getenv("MBSE_ROOT"));
	if ((fin = fopen(fnin, "r")) != NULL) {
		if ((fout = fopen(fnout, "w")) != NULL) {
			fread(&archiverhdr, sizeof(archiverhdr), 1, fin);
			/*
			 * In case we are automaic upgrading the data format
			 * we save the old format. If it is changed, the
			 * database must always be updated.
			 */
			oldsize = archiverhdr.recsize;
			if (oldsize != sizeof(archiver)) {
				ArchUpdated = 1;
				Syslog('+', "Format of %s changed, updateing", fnin);
			} else
				ArchUpdated = 0;
			archiverhdr.hdrsize = sizeof(archiverhdr);
			archiverhdr.recsize = sizeof(archiver);
			fwrite(&archiverhdr, sizeof(archiverhdr), 1, fout);

			/*
			 * The datarecord is filled with zero's before each
			 * read, so if the format changed, the new fields
			 * will be empty.
			 */
			memset(&archiver, 0, sizeof(archiver));
			while (fread(&archiver, oldsize, 1, fin) == 1) {
				fwrite(&archiver, sizeof(archiver), 1, fout);
				memset(&archiver, 0, sizeof(archiver));
			}

			fclose(fin);
			fclose(fout);
			return 0;
		} else
			return -1;
	}
	return -1;
}



void CloseArchive(int);
void CloseArchive(int force)
{
	char	fin[PATH_MAX], fout[PATH_MAX];
	FILE	*fi, *fo;
	st_list	*arc = NULL, *tmp;

	sprintf(fin, "%s/etc/archiver.data", getenv("MBSE_ROOT"));
	sprintf(fout,"%s/etc/archiver.temp", getenv("MBSE_ROOT"));

	if (ArchUpdated == 1) {
		if (force || (yes_no((char *)"Database is changed, save changes") == 1)) {
			working(1, 0, 0);
			fi = fopen(fout, "r");
			fo = fopen(fin,  "w");
			fread(&archiverhdr, archiverhdr.hdrsize, 1, fi);
			fwrite(&archiverhdr, archiverhdr.hdrsize, 1, fo);

			while (fread(&archiver, archiverhdr.recsize, 1, fi) == 1)
				if (!archiver.deleted)
					fill_stlist(&arc, archiver.comment, ftell(fi) - archiverhdr.recsize);
			sort_stlist(&arc);

			for (tmp = arc; tmp; tmp = tmp->next) {
				fseek(fi, tmp->pos, SEEK_SET);
				fread(&archiver, archiverhdr.recsize, 1, fi);
				fwrite(&archiver, archiverhdr.recsize, 1, fo);
			}

			tidy_stlist(&arc);
			fclose(fi);
			fclose(fo);
			unlink(fout);
			Syslog('+', "Updated \"archiver.data\"");
			return;
		}
	}
	working(1, 0, 0);
	unlink(fout); 
}



int AppendArchive(void)
{
	FILE	*fil;
	char	ffile[PATH_MAX];

	sprintf(ffile, "%s/etc/archiver.temp", getenv("MBSE_ROOT"));
	if ((fil = fopen(ffile, "a")) != NULL) {
		memset(&archiver, 0, sizeof(archiver));
		fwrite(&archiver, sizeof(archiver), 1, fil);
		fclose(fil);
		ArchUpdated = 1;
		return 0;
	} else
		return -1;
}



/*
 * Edit one record, return -1 if there are errors, 0 if ok.
 */
int EditArchRec(int Area)
{
	FILE	*fil;
	char	mfile[PATH_MAX];
	long	offset;
	int	j;
	unsigned long crc, crc1;

	clr_index();
	working(1, 0, 0);
	IsDoing("Edit Archiver");

	sprintf(mfile, "%s/etc/archiver.temp", getenv("MBSE_ROOT"));
	if ((fil = fopen(mfile, "r")) == NULL) {
		working(2, 0, 0);
		return -1;
	}

	offset = sizeof(archiverhdr) + ((Area -1) * sizeof(archiver));
	if (fseek(fil, offset, 0) != 0) {
		working(2, 0, 0);
		return -1;
	}

	fread(&archiver, sizeof(archiver), 1, fil);
	fclose(fil);
	crc = 0xffffffff;
	crc = upd_crc32((char *)&archiver, crc, sizeof(archiver));
	working(0, 0, 0);

	set_color(WHITE, BLACK);
	mvprintw( 5, 2, "3.  EDIT ARCHIVER");
	set_color(CYAN, BLACK);
	mvprintw( 7, 2, "1.  Comment");
	mvprintw( 8, 2, "2.  Name");
	mvprintw( 9, 2, "3.  Available");
	mvprintw(10, 2, "4.  Deleted");
	mvprintw(11, 2, "5.  Arc files");
	mvprintw(12, 2, "6.  Arc mail");
	mvprintw(13, 2, "7.  Banners");
	mvprintw(14, 2, "8.  Arc test");
	mvprintw(15, 2, "9.  Un. files");
	mvprintw(16, 2, "10. Un. mail");
	mvprintw(17, 2, "11. FILE_ID");

	for (;;) {
		set_color(WHITE, BLACK);
		show_str( 7,16,40, archiver.comment);
		show_str( 8,16, 5, archiver.name);
		show_bool(9,16,    archiver.available);
		show_bool(10,16,   archiver.deleted);
		show_str(11,16,64, archiver.farc);
		show_str(12,16,64, archiver.marc);
		show_str(13,16,64, archiver.barc);
		show_str(14,16,64, archiver.tarc);
		show_str(15,16,64, archiver.funarc);
		show_str(16,16,64, archiver.munarc);
		show_str(17,16,64, archiver.iunarc);

		j = select_menu(11);
		switch(j) {
		case 0:	crc1 = 0xffffffff;
			crc1 = upd_crc32((char *)&archiver, crc1, sizeof(archiver));
			if (crc != crc1) {
				if (yes_no((char *)"Record is changed, save") == 1) {
					working(1, 0, 0);
					if ((fil = fopen(mfile, "r+")) == NULL) {
						working(2, 0, 0);
						return -1;
					}
					fseek(fil, offset, 0);
					fwrite(&archiver, sizeof(archiver), 1, fil);
					fclose(fil);
					ArchUpdated = 1;
					working(1, 0, 0);
					working(0, 0, 0);
				}
			}
			IsDoing("Browsing Menu");
			return 0;
		case 1: E_STR(  7,16,40,archiver.comment,  "The ^Comment^ for this record")
		case 2:	E_STR(  8,16,5, archiver.name,     "The ^name^ of this archiver")
		case 3:	E_BOOL( 9,16,   archiver.available,"Switch if this archiver is ^Available^ for use.")
		case 4:	E_BOOL(10,16,   archiver.deleted,  "Is this archiver ^deleted^")
		case 5:	E_STR( 11,16,64,archiver.farc,     "The ^Archive^ command for files")
		case 6:	E_STR( 12,16,64,archiver.marc,     "The ^Archive^ command for mail packets")
		case 7:	E_STR( 13,16,64,archiver.barc,     "The ^Archive^ command to insert/replace banners")
		case 8:	E_STR( 14,16,64,archiver.tarc,     "The ^Archive^ command to test an archive")
		case 9:	E_STR( 15,16,64,archiver.funarc,   "The ^Unarchive^ command for files")
		case 10:E_STR( 16,16,64,archiver.munarc,   "The ^Unarchive^ command for mail packets")
		case 11:E_STR( 17,16,64,archiver.iunarc,   "The ^Unarchive^ command to extract the FILE_ID.DIZ file")
		}
	}

	return 0;
}



void EditArchive(void)
{
	int	records, i, x, y;
	char	pick[12];
	FILE	*fil;
	char	temp[PATH_MAX];
	long	offset;

	clr_index();
	working(1, 0, 0);
	IsDoing("Browsing Menu");
	if (config_read() == -1) {
		working(2, 0, 0);
		return;
	}

	records = CountArchive();
	if (records == -1) {
		working(2, 0, 0);
		return;
	}

	if (OpenArchive() == -1) {
		working(2, 0, 0);
		return;
	}
	working(0, 0, 0);

	for (;;) {
		clr_index();
		set_color(WHITE, BLACK);
		mvprintw( 5, 4, "3.  ARCHIVER SETUP");
		set_color(CYAN, BLACK);
		if (records != 0) {
			sprintf(temp, "%s/etc/archiver.temp", getenv("MBSE_ROOT"));
			if ((fil = fopen(temp, "r")) != NULL) {
				fread(&archiverhdr, sizeof(archiverhdr), 1, fil);
				x = 2;
				y = 7;
				set_color(CYAN, BLACK);
				for (i = 1; i <= records; i++) {
					offset = sizeof(archiverhdr) + ((i - 1) * archiverhdr.recsize);
					fseek(fil, offset, 0);
					fread(&archiver, archiverhdr.recsize, 1, fil);
					if (i == 11) {
						x = 42;
						y = 7;
					}
					if (archiver.available)
						set_color(CYAN, BLACK);
					else
						set_color(LIGHTBLUE, BLACK);
					sprintf(temp, "%3d.  %-32s", i, archiver.comment);
					temp[37] = 0;
					mvprintw(y, x, temp);
					y++;
				}
				fclose(fil);
			}
			/* Show records here */
		}
		strcpy(pick, select_record(records, 20));
		
		if (strncmp(pick, "-", 1) == 0) {
			CloseArchive(FALSE);
			return;
		}

		if (strncmp(pick, "A", 1) == 0) {
			working(1, 0, 0);
			if (AppendArchive() == 0) {
				records++;
				working(3, 0, 0);
			} else
				working(2, 0, 0);
			working(0, 0, 0);
		}

		if ((atoi(pick) >= 1) && (atoi(pick) <= records))
			EditArchRec(atoi(pick));
	}
}



void InitArchive(void)
{
    CountArchive();
    OpenArchive();
    CloseArchive(TRUE);
}



char *PickArchive(char *shdr)
{
	static	char Arch[6] = "";
	int	records, i, x, y;
	char	pick[12];
	FILE	*fil;
	char	temp[PATH_MAX];
	long	offset;


	clr_index();
	working(1, 0, 0);
	if (config_read() == -1) {
		working(2, 0, 0);
		return Arch;
	}

	records = CountArchive();
	if (records == -1) {
		working(2, 0, 0);
		return Arch;
	}

	working(0, 0, 0);

	clr_index();
	set_color(WHITE, BLACK);
	sprintf(temp, "%s.  ARCHIVER SELECT", shdr);
	mvprintw( 5, 4, temp);
	set_color(CYAN, BLACK);
	if (records != 0) {
		sprintf(temp, "%s/etc/archiver.data", getenv("MBSE_ROOT"));
		if ((fil = fopen(temp, "r")) != NULL) {
			fread(&archiverhdr, sizeof(archiverhdr), 1, fil);
			x = 2;
			y = 7;
			set_color(CYAN, BLACK);
			for (i = 1; i <= records; i++) {
				offset = sizeof(archiverhdr) + ((i - 1) * archiverhdr.recsize);
				fseek(fil, offset, 0);
				fread(&archiver, archiverhdr.recsize, 1, fil);
				if (i == 11) {
					x = 41;
					y = 7;
				}
				if (archiver.available)
					set_color(CYAN, BLACK);
				else
					set_color(LIGHTBLUE, BLACK);
				sprintf(temp, "%3d.  %-32s", i, archiver.comment);
				temp[37] = 0;
				mvprintw(y, x, temp);
				y++;
			}
			strcpy(pick, select_pick(records, 20));

			if ((atoi(pick) >= 1) && (atoi(pick) <= records)) {
				offset = sizeof(archiverhdr) + ((atoi(pick) - 1) * archiverhdr.recsize);
				fseek(fil, offset, 0);
				fread(&archiver, archiverhdr.recsize, 1, fil);
				strcpy(Arch, archiver.name);
			}
			fclose(fil);
		}
	}
	return Arch;
}


int archive_doc(FILE *fp, FILE *toc, int page)
{
	char	temp[PATH_MAX];
	FILE	*arch;
	int	j;

	sprintf(temp, "%s/etc/archiver.data", getenv("MBSE_ROOT"));
	if ((arch = fopen(temp, "r")) == NULL)
		return page;

	page = newpage(fp, page);
	addtoc(fp, toc, 3, 0, page, (char *)"Archiver programs");
	j = 0;

	fprintf(fp, "\n\n");
	fread(&archiverhdr, sizeof(archiverhdr), 1, arch);
	while ((fread(&archiver, archiverhdr.recsize, 1, arch)) == 1) {

		if (j == 4) {
			page = newpage(fp, page);
			fprintf(fp, "\n");
			j = 0;
		}

		fprintf(fp, "     Comment         %s\n", archiver.comment);
		fprintf(fp, "     Short name      %s\n", archiver.name);
		fprintf(fp, "     Available       %s\n", getboolean(archiver.available));
		fprintf(fp, "     Pack files      %s\n", archiver.farc);
		fprintf(fp, "     Pack mail       %s\n", archiver.marc);
		fprintf(fp, "     Pack banners    %s\n", archiver.barc);
		fprintf(fp, "     Test archive    %s\n", archiver.tarc);
		fprintf(fp, "     Unpack files    %s\n", archiver.funarc);
		fprintf(fp, "     Unpack mail     %s\n", archiver.munarc);
		fprintf(fp, "     Get FILE_ID.DIZ %s\n", archiver.iunarc);
		fprintf(fp, "\n\n\n");
		j++;
	}

	fclose(arch);
	return page;
}



