/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: MBSE BBS Mail Gate
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

/* Base on E.C. Crosser's ifmail.
 *
 * ### Modified by P.Saratxaga on 19 Sep 1995 ###
 * - Added X-FTN-From and X-FTN-To support
 * - added code by T.Tanaka, dated 13 Mar 1995, to put the freename in the ftn
 *   header, instead of the userid, when the address is fido parseable
 * - modified ^aREPLY: code, to look in In-Reply-To:
 * - support to decode MSGID from fidogate "Message-ID: <MSGID_....>"
 * - suport for X-Apparently-To: (generated by the french fido->usenet gate)
 * - added don't regate code by Wim Van Sebroeck <vsebro@medelec.uia.ac.be>
 * - corriged a bug when Organization: has only blanks
 */

#include "../lib/libs.h"
#include "../lib/structs.h"
#include "../lib/users.h"
#include "../lib/records.h"
#include "../lib/common.h"
#include "../lib/clcomm.h"
#include "../lib/dbcfg.h"
#include "atoul.h"
#include "hash.h"
#include "aliasdb.h"
#include "mkftnhdr.h"



#ifndef ULONG_MAX
#define ULONG_MAX 4294967295
#endif


char	*replyaddr=NULL;
char	*ftnmsgidstyle=NULL;
faddr	*bestaka;



int ftnmsgid(char *msgid, char **s, unsigned long *n, char *areaname)
{
	char		*buf, *l, *r, *p;
	unsigned long	nid = 0L;
	faddr		*tmp;
	static int	ftnorigin = 0;

	if (msgid == NULL) {
		*s = NULL;
		*n = 0L;
		return ftnorigin;
	}

	buf = malloc(strlen(msgid)+65);
	strcpy(buf, msgid);
	if ((l = strchr(buf,'<'))) 
		l++;
	else 
		l = buf;
	while (isspace(*l)) 
		l++;
	if ((r = strchr(l,'>'))) 
		*r = '\0';
	r = l + strlen(l) - 1;
	while (isspace(*r) && (r > l)) 
		(*r--)='\0';
	if ((tmp = parsefaddr(l))) {
		if (tmp->name) {
			if (strspn(tmp->name,"0123456789") == strlen(tmp->name))
				nid = atoul(tmp->name);
			else 
				nid = ULONG_MAX;
			if (nid == ULONG_MAX) {
				hash_update_s(&nid, tmp->name);
			} else
				ftnorigin = 1;
		} else {
			hash_update_s(&nid,l);
		}
		*s = xstrcpy(ascfnode(tmp, 0x1f));
		tidy_faddr(tmp);
	} else {
		if ((r=strchr(l,'@')) == NULL) { /* should never happen */
			Syslog('!', "ftnmsgid: should never happen");
			*s = xstrcpy(l);
			hash_update_s(&nid,l);
		/* <MSGID_mimeanything_abcd1234@ftn.domain> */
		} else if (strncmp(l,"MSGID_",6) == 0) {
			*r = '\0';
			r = strrchr(l+6,'_');
                	if (r) 
				*r++ = '\0';
                	*s = xstrcpy(qp_decode(l+6));
			if (r) 
				sscanf(r,"%lx",&nid);
			ftnorigin = 1;
		/* <NOMSGID_mimeanything_abcd1234@ftn.domain> */
		} else if (strncmp(l,"NOMSGID_",8) == 0) {
			*s = NULL;
			*n = 0L;
			ftnorigin = 1;
			return ftnorigin;
		/* <ftn_2.204.226$fidonet_1d17b3b3_Johan.Olofsson@magnus.ct.se> */
		} else if (strncmp(l,"ftn_",4) == 0) {
			*r = '\0';
			if ((r = strchr(l+4,'$')) || (r=strchr(l+4,'#'))) {
				if (*r=='$') 
					*r='@';
				if ((r=strchr(l+4,'.')))
					*r=':';
				if ((r=strchr(l+4,'.')))
					*r='/';
			}
			while ((r=strrchr(l+4,'_')) != strchr(l+4,'_')) 
				*r='\0';
			r=strchr(l+4,'_');
			*r++='\0';
			*s=xstrcpy(l+4);
			sscanf(r,"%lx",&nid);
			ftnorigin=1;
		/* <wgcid$3$g712$h610$i22$kfidonet$j6596dbf5@brazerko.com> */
		} else if (strncmp(l,"wgcid$",6) == 0) {
			*r='\0';
			if ((r=strstr(l+6,"$g"))) {
				*r='\0';
				*s=xstrcpy(l+6);
				*s=xstrcat(*s,(char *)":");
				l=r+2;
			}
			if ((r=strstr(l,"$h"))) {
				*r++='\0';
				*s=xstrcat(*s,l);
				*s=xstrcat(*s,(char *)"/");
				l=r+2;
			}
			if ((r=strstr(l,"$i"))) {
				*r='\0';
				*s=xstrcat(*s,l);
				*s=xstrcat(*s,(char *)".");
				l=r+2;
			}
			if ((r=strstr(l,"$k"))) {
				*r='\0';
				*s=xstrcat(*s,l);
				*s=xstrcat(*s,(char *)"@");
				l=r+2;
			}
			if ((r=strstr(l,"$j"))) {
				*r='\0';
				*s=xstrcat(*s,l);
				sscanf(r+2,"%lx",&nid);
			}
		} else {
			*r='\0';
			if ((p=strchr(l,'%'))) {
				*p='\0';
				if (strspn(l,"0123456789") == strlen(l)) {
					*r='@';
					r=p;
				} else 
					*p='%';
			}
			r++;
			if (strspn(l,"0123456789") == strlen(l))
				nid = atoul(l);
			else 
				nid = ULONG_MAX;
			if (nid == ULONG_MAX)
				hash_update_s(&nid,l);
			*s=xstrcpy(r);
		}
	}
	*n=nid;

	free(buf);
	return ftnorigin;
}



ftnmsg *mkftnhdr(rfcmsg *msg, int incode, int outcode, int newsmode, faddr *recipient)
{
	char		*freename = NULL, *rfcfrom = NULL, *p, *q, *l, *r;
	char		*fbuf = NULL;
	char		*ftnfrom=NULL;
	static ftnmsg	*tmsg;
	int		needreplyaddr = 1;
	faddr		*tmp, *tmp2;

	tmsg=(ftnmsg *)malloc(sizeof(ftnmsg));
	memset(tmsg, 0, sizeof(ftnmsg));

	if (newsmode) {
		p = xstrcpy(hdr((char *)"Comment-To",msg));
		if (p == NULL) 
			p = xstrcpy(hdr((char *)"X-Comment-To",msg));
		if (p == NULL) 
			p = xstrcpy(hdr((char *)"X-FTN-To",msg));
		if (p == NULL) 
			p = xstrcpy(hdr((char *)"X-Fidonet-Comment-To",msg));
		if (p == NULL) 
			p = xstrcpy(hdr((char *)"X-Apparently-To",msg));
		if (p == NULL)
			p = xstrcpy(hdr((char *)"To", msg));  /* 14-Aug-2001 MB */
		if (p) {
			if ((tmsg->to = parsefaddr(p)) == NULL)
				tmsg->to = parsefaddr((char *)"All@p0.f0.n0.z0");
			if ((l = strrchr(p,'<')) && (r = strchr(p,'>')) && (l < r)) {
				r = l;
				*r-- = '\0';
				if ((l = strchr(p,'"')) && (r = strrchr(p,'"')) && (l < r)) {
					l++;
					*r-- = '\0';
				}
				while (isspace(*r)) 
					*r-- = '\0';
					if (!l) 
						l = p;
				while (isspace(*l)) 
					l++;
			} else if ((l = strrchr(p,'(')) && (r = strchr(p,')')) && (l < r)) {
				*r-- = '\0';
				while (isspace(*r)) 
					*r-- = '\0';
				l++;
				while (isspace(*l)) 
					l++;
			} else {
				l = p;
				while (isspace(*l)) 
					l++;
				r = p + strlen(p) -1;
				if (*r == '\n') 
					*r-- = '\0';
				while (isspace(*r)) 
					*r-- = '\0';
			}

			if (*l) {
				strcpy(l,hdrnconv(l,incode,outcode,MAXNAME));
				if (strlen(l) > MAXNAME)
					l[MAXNAME]='\0';
				free(tmsg->to->name);
				tmsg->to->name=xstrcpy(l);
			}
			free(p);
			/*
			 *  It will become echomail, the destination FTN address must
			 *  be our address.  14-Aug-2001 MB.
			 */
			tmsg->to->zone   = msgs.Aka.zone;
			tmsg->to->net    = msgs.Aka.net;
			tmsg->to->node   = msgs.Aka.node;
			tmsg->to->point  = msgs.Aka.point;
			tmsg->to->domain = xstrcpy(msgs.Aka.domain);
		} else {
			/*
			 *  Filling a default To: address.
			 */
			tmsg->to = (faddr*)malloc(sizeof(faddr));
			tmsg->to->name   = xstrcpy((char *)"All");
			tmsg->to->zone   = msgs.Aka.zone;
			tmsg->to->net    = msgs.Aka.net;
			tmsg->to->node   = msgs.Aka.node;
			tmsg->to->point  = msgs.Aka.point;
			tmsg->to->domain = xstrcpy(msgs.Aka.domain);
		}
		Syslog('N', "TO: %s",ascfnode(tmsg->to,0xff));
	} else {
		if (recipient) {
			/*
			 *  In mbmail mode the recipient is valid and must be used 
			 *  as the destination address. The To: field is probably
			 *  an RFC address an cannot be used to route the message.
			 */
			tmsg->to = (faddr *)malloc(sizeof(faddr));
			tmsg->to->point = recipient->point;
			tmsg->to->node  = recipient->node;
			tmsg->to->net   = recipient->net;
			tmsg->to->zone  = recipient->zone;
			tmsg->to->name  = xstrcpy(recipient->name);
			if (tmsg->to->name && (strlen(tmsg->to->name) > MAXNAME))
				tmsg->to->name[MAXNAME]='\0';
			tmsg->to->domain = xstrcpy(recipient->domain);
			Syslog('m', "Recipient TO: %s", ascfnode(tmsg->to,0xff));
		} else {
			p = xstrcpy(hdr((char *)"To",msg));
			if (p == NULL)
				p = xstrcpy(hdr((char *)"X-Apparently-To",msg));
			if (p) {
				if ((tmsg->to = parsefaddr(p)) == NULL)
					WriteError("Unparsable destination address");
				else
					Syslog('m', "RFC parsed TO: %s",ascfnode(tmsg->to,0xff));
			}
		}
	} /* else (newsmode) */

	p = fbuf = xstrcpy(hdr((char *)"Reply-To", msg));
	if (fbuf == NULL) 
		p = fbuf = xstrcpy(hdr((char *)"From", msg));
	if (fbuf == NULL) 
		p = fbuf = xstrcpy(hdr((char *)"X-UUCP-From", msg));
	if (p) {
	        q = p;
		while (isspace(*q)) 
			q++;
		fbuf = parserfcaddr(q).remainder;
		if (parserfcaddr(q).target) {
			fbuf = xstrcat(fbuf, (char *)"@");
			fbuf = xstrcat(fbuf, parserfcaddr(q).target);
		}	
		rfcfrom = fbuf;
	}
	if (p)
		free(p);
	p = NULL;
	if (!rfcfrom) 
		rfcfrom = xstrcpy((char *)"postmaster");
        p = fbuf = xstrcpy(hdr((char *)"From", msg));
        if (fbuf == NULL) 
		p = fbuf = xstrcpy(hdr((char *)"X-UUCP-From", msg));
	if (p) {
	        q = p;
		while (isspace(*q)) 
			q++;
                if ((q) && (*q !=  '\0'))
                	freename = parserfcaddr(q).comment;
                else 
			freename = NULL;
	} else 
		freename = xstrcpy((char *)"Unidentified User");
	if (freename) {
		while (isspace(*freename)) 
			freename++;
	}

	if (rfcfrom) {
		while (isspace(*rfcfrom)) 
			rfcfrom++;
		p = rfcfrom + strlen(rfcfrom) -1;
		while ((isspace(*p)) || (*p == '\n')) 
			*(p--)='\0';
	}

	if ((freename) && (*freename != '\0')) {
		while (isspace(*freename)) 
			freename++;
		p = freename + strlen(freename) -1;
		while ((isspace(*p)) || (*p == '\n')) 
			*(p--)='\0';
		strcpy(freename, hdrconv(freename, incode, outcode));
		if ((*freename == '\"') && (*(p=freename+strlen(freename)-1) == '\"')) {
			freename++;
			*p='\0';
		}
	}
	if ((!freename) || ((freename) && (*freename == '\0')) || (strcmp(freename,".")==0)) 
	    freename=rfcfrom;

	if (newsmode)
		Syslog('M', "FROM: %s <%s>", freename, rfcfrom);
	else
		Syslog('+', "from: %s <%s>",freename,rfcfrom);

	needreplyaddr = 1;
	if ((tmsg->from=parsefaddr(rfcfrom)) == NULL) {
		if (freename && rfcfrom)
			if (!strchr(freename,'@') && !strchr(freename,'%') && 
			    strncasecmp(freename,rfcfrom,MAXNAME) &&
			    strncasecmp(freename,"uucp",4) &&
			    strncasecmp(freename,"usenet",6) &&
			    strncasecmp(freename,"news",4) &&
			    strncasecmp(freename,"super",5) &&
			    strncasecmp(freename,"admin",5) &&
			    strncasecmp(freename,"postmaster",10) &&
			    strncasecmp(freename,"sys",3)) 
				needreplyaddr=registrate(freename,rfcfrom);
	} else {
		tmsg->ftnorigin = 1;
		tmsg->from->name = xstrcpy(freename);
		if (strlen(tmsg->from->name) > MAXNAME)
			tmsg->from->name[MAXNAME]='\0';
	}
	if (replyaddr) {
		free(replyaddr);
		replyaddr=NULL;
	}
	if (needreplyaddr && (tmsg->from == NULL)) {
		Syslog('M', "fill replyaddr with \"%s\"",rfcfrom);
		replyaddr=xstrcpy(rfcfrom);
	}

	if (tmsg->from)
	    Syslog('m', "From address was%s distinguished as ftn", tmsg->from ? "" : " not");

	if (newsmode) {
		tmp2 = fido2faddr(msgs.Aka);
		bestaka = bestaka_s(tmp2);
		tidy_faddr(tmp2);
	} else
		bestaka = bestaka_s(tmsg->to);

	if ((tmsg->from == NULL) && (bestaka)) {
		if (CFG.dontregate) {
			p = xstrcpy(hdr((char *)"X-FTN-Sender",msg));
			if (p == NULL) {
				if ((p = hdr((char *)"X-FTN-From",msg))) {
					tmp = parsefnode(p);
					p = xstrcpy(ascinode(tmp, 0xff));
					tidy_faddr(tmp);
				}
			}
			if (p) {
	        		q = p;
				while (isspace(*q)) 
					q++;
				ftnfrom = parserfcaddr(q).remainder;
				if (parserfcaddr(q).target) {
					ftnfrom = xstrcat(ftnfrom,(char *)"@");
					ftnfrom = xstrcat(ftnfrom,parserfcaddr(q).target);
				}	
				Syslog('m', "Ftn gateway: \"%s\"", ftnfrom);
				Syslog('+', "Ftn sender: %s",ftnfrom);
				if (ftnfrom) 
					tmsg->from = parsefaddr(ftnfrom);
				if ((tmsg->from) && (!tmsg->from->name))
					tmsg->from->name = xstrcpy(rfcfrom);
			}
			if (p)
				free(p);
			p = NULL;
			if (tmsg->from == NULL) {
				tmsg->from=(faddr *)malloc(sizeof(faddr));
				tmsg->from->name=xstrcpy(freename);
				if (tmsg->from->name && (strlen(tmsg->from->name) > MAXNAME))
					tmsg->from->name[MAXNAME]='\0';
				tmsg->from->point=bestaka->point;
				tmsg->from->node=bestaka->node;
				tmsg->from->net=bestaka->net;
				tmsg->from->zone=bestaka->zone;
				tmsg->from->domain=xstrcpy(bestaka->domain);
			}
		} else {
			tmsg->from=(faddr *)xmalloc(sizeof(faddr));
			tmsg->from->name=xstrcpy(freename);
			if (tmsg->from->name && (strlen(tmsg->from->name) > MAXNAME))
				tmsg->from->name[MAXNAME]='\0';
			tmsg->from->point=bestaka->point;
			tmsg->from->node=bestaka->node;
			tmsg->from->net=bestaka->net;
			tmsg->from->zone=bestaka->zone;
			tmsg->from->domain=xstrcpy(bestaka->domain);
		}
	}
	if (fbuf) 
		free(fbuf); 
	fbuf = NULL;

	p = hdr((char *)"Subject", msg);
	if (p) {
		while (isspace(*p)) 
			p++;
		/*
		 * charset conversion for subject line is done in message.c 
		 * here we only convert quoted-printable and base64 to 8 bit
		 */
		tmsg->subj = xstrcpy(hdrnconv(p, 0, 0, MAXSUBJ));
		tmsg->subj = xstrcpy(p);
		if (*(p=tmsg->subj+strlen(tmsg->subj)-1) == '\n') 
			*p='\0';
		if (strlen(tmsg->subj) > MAXSUBJ) 
			tmsg->subj[MAXSUBJ]='\0';
	} else {
		tmsg->subj = xstrcpy((char *)" ");
	}
	Syslog('M', "SUBJ: \"%s\"", tmsg->subj);

	if ((p = hdr((char *)"X-FTN-FLAGS",msg))) 
		tmsg->flags |= flagset(p);
	if (hdr((char *)"Return-Receipt-To",msg)) 
		tmsg->flags |= M_RRQ;
	if (hdr((char *)"Notice-Requested-Upon-Delivery-To",msg)) 
		tmsg->flags |= M_RRQ;
	if (!newsmode)
		tmsg->flags |= M_PVT;

	if ((p = hdr((char *)"X-Origin-Date",msg))) 
		tmsg->date = parsedate(p, NULL);
	else if ((p = hdr((char *)"Date",msg))) 
		tmsg->date = parsedate(p, NULL);
	else 
		tmsg->date = time((time_t *)NULL);

	if ((p = hdr((char *)"X-FTN-MSGID", msg))) {
		tmsg->ftnorigin &= 1;
		while (isspace(*p)) 
			p++;
		tmsg->msgid_s = xstrcpy(p);
		if (*(p = tmsg->msgid_s + strlen(tmsg->msgid_s) -1) == '\n') 
			*p='\0';
	} else if ((p = hdr((char *)".MSGID",msg))) {
		tmsg->ftnorigin &= 1;
		while (isspace(*p)) 
			p++;
		tmsg->msgid_s = xstrcpy(p);
		if (*(p = tmsg->msgid_s + strlen(tmsg->msgid_s) -1) == '\n') 
			*p='\0';
	} else if ((p = hdr((char *)"Message-ID",msg))) {
		tmsg->ftnorigin &= ftnmsgid(p,&(tmsg->msgid_a),&(tmsg->msgid_n),tmsg->area);
	} else
		tmsg->msgid_a = NULL;

	if ((p = hdr((char *)"X-FTN-REPLY",msg))) {
		while (isspace(*p)) 
			p++;
		tmsg->reply_s = xstrcpy(p);
		if (*(p=tmsg->reply_s + strlen(tmsg->reply_s) -1) == '\n') 
			*p='\0';
	} else {
		if (newsmode) {
			p = hdr((char *)"References",msg);
			if (p) {       
				l = xstrcpy(p);
				r = strtok(l," \t\n");
				while ((l=strtok(NULL," \t\n")) != NULL) 
					r = l;
				p = r;
				free(l);
			}
		} else
			p = hdr((char *)"In-Reply-To",msg);
	}
	if (p)
		(void)ftnmsgid(p,&(tmsg->reply_a),&(tmsg->reply_n),NULL);
	else
		tmsg->reply_a=NULL;

	Syslog('M', "DATE: %s, MSGID: %s %lx, REPLY: %s %lx",
		ftndate(tmsg->date), MBSE_SS(tmsg->msgid_a),tmsg->msgid_n, MBSE_SS(tmsg->reply_a),tmsg->reply_n);

	p = hdr((char *)"Organization",msg);
	if (p == NULL)
		p = hdr((char *)"Organisation",msg);
	if (p) {
		while (isspace(*p)) 
			p++;
                tmsg->origin = xstrcpy(hdrconv(p, incode, outcode));
		if (tmsg->origin)
			if (*(p = tmsg->origin + strlen(tmsg->origin)-1) == '\n') 
				*p='\0';
	} else {
		/*
		 *  No Organization header, insert the default BBS origin.
		 */
		tmsg->origin = xstrcpy(CFG.origin);
	}

	Syslog('M', "ORIGIN: %s", MBSE_SS(tmsg->origin));
	return tmsg;
}


