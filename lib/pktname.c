/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: BinkleyTerm outbound naming
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
#include "libs.h"
#include "structs.h"
#include "users.h"
#include "records.h"
#include "clcomm.h"
#include "common.h"


#ifndef PATH_MAX
#define PATH_MAX 512
#endif

#define ptyp "ut"
#define ftyp "lo"
#define ttyp "pk"
#define rtyp "req"
#define styp "spl"
#define btyp "bsy"
#define qtyp "sts"
#define ltyp "pol"


static char buf[PATH_MAX];


char *prepbuf(faddr *addr)
{
	char	*p, *domain=NULL;
	char	zpref[8];
	int	i;

	sprintf(buf, "%s", CFG.outbound);

	if (CFG.addr4d) {
		Syslog('o', "Use 4d addressing, zone is %d", addr->zone);

		if ((addr->zone == 0) || (addr->zone == CFG.aka[0].zone))
			zpref[0] = '\0';
		else
			sprintf(zpref, ".%03x", addr->zone);
	} else {
		/*
		 * If we got a 5d address we use the given domain, if
		 * we got a 4d address, we look for a matching domain name.
		 */
		if (addr->domain)
			domain = xstrcpy(addr->domain);
		else
			for (i = 0; i < 40; i++)
				if (CFG.aka[i].zone == addr->zone) {
					domain = xstrcpy(CFG.aka[i].domain);
					break;
				}

		if ((domain != NULL) && (strlen(CFG.aka[0].domain) != 0) &&
		    (strcasecmp(domain,CFG.aka[0].domain) != 0)) {
			if ((p = strrchr(buf,'/'))) 
				p++;
			else 
				p = buf;
			strcpy(p, domain);
			for (; *p; p++) 
				*p = tolower(*p);
			for (i = 0; i < 40; i++)
				if ((strlen(CFG.aka[i].domain)) &&
				   (strcasecmp(CFG.aka[i].domain, domain) == 0))
					break;

			/*
			 * The default zone must be the first one in the
			 * setup, other zones get the hexadecimal zone
			 * number appended.
			 */
			if (CFG.aka[i].zone == addr->zone)
				zpref[0] = '\0';
			else
				sprintf(zpref, ".%03x", addr->zone);
		} else {
			/*
			 * this is our primary domain
			 */
			if ((addr->zone == 0) || (addr->zone == CFG.aka[0].zone))
				zpref[0]='\0';
			else 
				sprintf(zpref,".%03x",addr->zone);
		}
	}

	p = buf + strlen(buf);

	if (addr->point)
		sprintf(p,"%s/%04x%04x.pnt/%08x.", zpref,addr->net,addr->node,addr->point);
	else
		sprintf(p,"%s/%04x%04x.",zpref,addr->net,addr->node);

	p = buf + strlen(buf);
	if (domain)
		free(domain);
	return p;
}



char *pktname(faddr *addr, char flavor)
{
	char	*p;

	p = prepbuf(addr);
	if (flavor == 'f') 
		flavor = 'o';

	sprintf(p, "%c%s", flavor, ptyp);
	Syslog('O', "packet name is \"%s\"",buf);
	return buf;
}



char *floname(faddr *addr, char flavor)
{
	char	*p;

	p = prepbuf(addr);
	if (flavor == 'o') 
		flavor = 'f';
	sprintf(p, "%c%s", flavor, ftyp);
	Syslog('O', "flo file name is \"%s\"",buf);
	return buf;
}



char *reqname(faddr *addr)
{
	char *p;

	p = prepbuf(addr);
	sprintf(p, "%s", rtyp);
	Syslog('O', "req file name is \"%s\"",buf);
	return buf;
}



char *splname(faddr *addr)
{
	char *p;

	p = prepbuf(addr);
	sprintf(p, "%s", styp);
	Syslog('O', "spl file name is \"%s\"",buf);
	return buf;
}



char *bsyname(faddr *addr)
{
	char	*p;

	p = prepbuf(addr);
	sprintf(p, "%s", btyp);
	Syslog('O', "bsy file name is \"%s\"",buf);
	return buf;
}



char *stsname(faddr *addr)
{
	char *p;

	p = prepbuf(addr);
	sprintf(p, "%s", qtyp);
	Syslog('O', "sts file name is \"%s\"",buf);
	return buf;
}



char *polname(faddr *addr)
{
	char	*p;

	p = prepbuf(addr);
	sprintf(p, "%s", ltyp);
	Syslog('O', "pol file name is \"%s\"", buf);
	return buf;
}



static char *dow[] = {(char *)"su", (char *)"mo", (char *)"tu", (char *)"we", 
		      (char *)"th", (char *)"fr", (char *)"sa"};

char *dayname(void)
{
	time_t	tt;
	struct	tm *ptm;

	tt = time(NULL);
	ptm = localtime(&tt);
	sprintf(buf, "%s", dow[ptm->tm_wday]);

	return buf;	
}



char *arcname(faddr *addr, unsigned short Zone, int ARCmailCompat)
{
	char	*p;
	char	*ext;
	time_t	tt;
	struct	tm *ptm;
	faddr	*bestaka;

	tt = time(NULL);
	ptm = localtime(&tt);
	ext = dow[ptm->tm_wday];

	bestaka = bestaka_s(addr);

	(void)prepbuf(addr);
	p = strrchr(buf, '/');

	if (!ARCmailCompat && (Zone != addr->zone)) {
		/*
		 * Generate ARCfile name from the CRC of the ASCII string
		 * of the node address.
		 */
		sprintf(p, "/%08lx.%s0", StringCRC32(ascfnode(addr, 0x1f)), ext);
	} else {
		if (addr->point) {
			sprintf(p, "/%04x%04x.%s0",
				((bestaka->net) - (addr->net)) & 0xffff,
				((bestaka->node) - (addr->node) + (addr->point)) & 0xffff,
				ext);
		} else if (bestaka->point) {
			/*
			 * Inserted the next code for if we are a point,
			 * I hope this is ARCmail 0.60 compliant. 21-May-1999
			 */
			sprintf(p, "/%04x%04x.%s0", ((bestaka->net) - (addr->net)) & 0xffff,
				((bestaka->node) - (addr->node) - (bestaka->point)) & 0xffff, ext);
		} else {
			sprintf(p, "/%04x%04x.%s0", ((bestaka->net) - (addr->net)) & 0xffff,
				((bestaka->node) - (addr->node)) &0xffff, ext);
		}
	}

	Syslog('O', "Arc file name is \"%s\"", buf);
	return buf;
}


