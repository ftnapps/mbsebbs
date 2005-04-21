/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: mbtask - Internet BBS Chat (but it looks like...)
 *
 *****************************************************************************
 * Copyright (C) 1997-2005
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

#include "../config.h"
#include "../lib/mbselib.h"
#include "taskstat.h"
#include "taskutil.h"
#include "taskibc.h"



#ifdef  USE_EXPERIMENT

int		    ibc_run = FALSE;	    /* Thread running		*/
extern int	    T_Shutdown;		    /* Program shutdown		*/
extern int	    internet;		    /* Internet status		*/
time_t		    scfg_time = (time_t)0;  /* Servers config time	*/
time_t		    now;		    /* Current time		*/
ncs_list	    *ncsl = NULL;	    /* Neighbours list		*/
srv_list	    *servers = NULL;	    /* Active servers		*/
usr_list	    *users = NULL;	    /* Active users		*/
int		    ls;			    /* Listen socket		*/
struct sockaddr_in  myaddr_in;		    /* Listen socket address	*/
struct sockaddr_in  clientaddr_in;	    /* Remote socket address	*/
int		    changed = FALSE;	    /* Databases changed	*/
char		    crbuf[512];		    /* Chat receive buffer	*/
int		    srvchg = FALSE;	    /* Is serverlist changed	*/
int		    usrchg = FALSE;	    /* Is userlist changed	*/



pthread_mutex_t b_mutex = PTHREAD_MUTEX_INITIALIZER;


typedef enum {NCS_INIT, NCS_CALL, NCS_WAITPWD, NCS_CONNECT, NCS_HANGUP, NCS_FAIL, NCS_DEAD} NCSTYPE;


static char *ncsstate[] = {
    (char *)"init", (char *)"call", (char *)"waitpwd", (char *)"connect", 
    (char *)"hangup", (char *)"fail", (char *)"dead"
};



/*
 * Internal prototypes
 */
void fill_ncslist(ncs_list **, char *, char *, char *);
void dump_ncslist(void);
void tidy_servers(srv_list **);
void del_userbyserver(usr_list **, char *);
void add_server(srv_list **, char *, int, char *, char *, char *, char *);
void del_server(srv_list **, char *);
void del_router(srv_list **, char *);
int  send_msg(ncs_list *, const char *, ...);
void broadcast(char *, const char *, ...);
void check_servers(void);
void command_pass(char *, char *);
void command_server(char *, char *);
void command_squit(char *, char *);
void command_user(char *, char *);
void command_quit(char *, char *);
void receiver(struct servent *);



/*
 * Add a server to the serverlist
 */
void fill_ncslist(ncs_list **fdp, char *server, char *myname, char *passwd)
{
    ncs_list	*tmp, *ta;
    int		rc;

    if ((rc = pthread_mutex_lock(&b_mutex)))
	Syslog('!', "fill_ncslist() mutex_lock failed rc=%d", rc);

    tmp = (ncs_list *)malloc(sizeof(ncs_list));
    memset(tmp, 0, sizeof(tmp));
    tmp->next = NULL;
    strncpy(tmp->server, server, 63);
    strncpy(tmp->myname, myname, 63);
    strncpy(tmp->passwd, passwd, 15);
    tmp->state = NCS_INIT;
    tmp->action = now;
    tmp->last = (time_t)0;
    tmp->version = 0;
    tmp->remove = FALSE;
    tmp->socket = -1;
    tmp->token = 0;
    tmp->gotpass = FALSE;
    tmp->gotserver = FALSE;

    if (*fdp == NULL) {
	*fdp = tmp;
    } else {
	for (ta = *fdp; ta; ta = ta->next)
	if (ta->next == NULL) {
	    ta->next = (ncs_list *)tmp;
	    break;
	}
    }

    if ((rc = pthread_mutex_unlock(&b_mutex)))
	Syslog('!', "fill_ncslist() mutex_unlock failed rc=%d", rc);
}



void dump_ncslist(void)
{   
    ncs_list	*tmp;
    srv_list	*srv;
    usr_list	*usrp;

    if (!changed && !srvchg)
	return;

    Syslog('r', "Server                         State   Del Pwd Srv Next action");
    Syslog('r', "------------------------------ ------- --- --- --- -----------");
    
    for (tmp = ncsl; tmp; tmp = tmp->next) {
	Syslog('r', "%-30s %-7s %s %s %s %d", tmp->server, ncsstate[tmp->state], 
		tmp->remove ? "yes":"no ", tmp->gotpass ? "yes":"no ", 
		tmp->gotserver ? "yes":"no ", (int)tmp->action - (int)now);
    }

    if (srvchg) {
	Syslog('+', "IBC: Server                    Router                     Hops Users Connect time");
	Syslog('+', "IBC: ------------------------- ------------------------- ----- ----- --------------------");
	for (srv = servers; srv; srv = srv->next) {
	    Syslog('+', "IBC: %-25s %-25s %5d %5d %s", srv->server, srv->router, srv->hops, srv->users, rfcdate(srv->connected));
	}
    }
    
    if (usrchg) {
	Syslog('+', "IBC: Server               User                 Nick      Channel       Cop Connect time");
	Syslog('+', "IBC: -------------------- -------------------- --------- ------------- --- --------------------");
	for (usrp = users; usrp; usrp = usrp->next) {
	    Syslog('+', "IBC: %-20s %-20s %-9s %-13s %s %s", usrp->server, usrp->realname, usrp->nick, usrp->channel, 
		    usrp->chanop ? "yes":"no ", rfcdate(usrp->connected));
	}
    }

    srvchg = FALSE;
    usrchg = FALSE;
    changed = FALSE;
}



void tidy_servers(srv_list ** fdp)
{
    srv_list *tmp, *old;

    for (tmp = *fdp; tmp; tmp = old) {
	old = tmp->next;
	free(tmp);
    }
    *fdp = NULL;
}



/*
 * Add one user to the userlist. Returns:
 *  0 = Ok
 *  1 = User already registered.
 */
int add_user(usr_list **fap, char *server, char *nick, char *realname)
{
    usr_list    *tmp, *ta;
    srv_list	*sl;
    int         rc;

    Syslog('r', "add_user %s %s %s", server, nick, realname);

    for (ta = *fap; ta; ta = ta->next) {
	if ((strcmp(ta->server, server) == 0) && (strcmp(ta->realname, realname) == 0)) {
	    Syslog('-', "IBC: add_user(%s %s %s), already registered", server, nick, realname);
	    return 1;
	}
    }

    if ((rc = pthread_mutex_lock(&b_mutex)))
	Syslog('!', "add_user() mutex_lock failed rc=%d", rc);

    tmp = (usr_list *)malloc(sizeof(usr_list));
    memset(tmp, 0, sizeof(usr_list));
    tmp->next = NULL;
    strncpy(tmp->server, server, 63);
    strncpy(tmp->nick, nick, 9);
    strncpy(tmp->realname, realname, 36);
    tmp->connected = now;

    if (*fap == NULL) {
	*fap = tmp;
    } else {
	for (ta = *fap; ta; ta = ta->next)
	    if (ta->next == NULL) {
		ta->next = (usr_list *)tmp;
		break;
	    }
    }

    for (sl = servers; sl; sl = sl->next) {
	if (strcmp(sl->server, server) == 0) {
		sl->users++;
		srvchg = TRUE;
	}
    }

    if ((rc = pthread_mutex_unlock(&b_mutex)))
	Syslog('!', "add_user() mutex_unlock failed rc=%d", rc);

    usrchg = TRUE;
    return 0;
}



/*
 * Delete one user.
 */
void del_user(usr_list **fap, char *server, char *nick)
{
    usr_list    **tmp, *tmpa;
    srv_list	*sl;
    int         rc;

    Syslog('r', "deluser %s %s", server, nick);

    if (*fap == NULL)
	return;

    if ((rc = pthread_mutex_lock(&b_mutex)))
	Syslog('!', "del_user() mutex_lock failed rc=%d", rc);

    tmp = fap;
    while (*tmp) {
//	Syslog('r', "%s %s", (*tmp)->server, (*tmp)->realname);
	if ((strcmp((*tmp)->server, server) == 0) && (strcmp((*tmp)->nick, nick) == 0)) {
//	    Syslog('r', "remove");
	    tmpa = *tmp;
	    *tmp=(*tmp)->next;
	    free(tmpa);
	    usrchg = TRUE;
	} else {
	    tmp = &((*tmp)->next);
	}
    }

//    if (!usrchg)
//	Syslog('r', "Could not delete user");

    for (sl = servers; sl; sl = sl->next) {
	if ((strcmp(sl->server, server) == 0) && sl->users) {
	    sl->users--;
	    srvchg = TRUE;
	}
    }

    if ((rc = pthread_mutex_unlock(&b_mutex)))
	Syslog('!', "del_user() mutex_unlock failed rc=%d", rc);
}



void add_server(srv_list **fdp, char *name, int hops, char *prod, char *vers, char *fullname, char *router)
{
    srv_list	*tmp, *ta;
    int		rc;
    
    Syslog('r', "add_server %s %d %s %s %s", name, hops, prod, vers, fullname);
 
    for (ta = *fdp; ta; ta = ta->next) {
	if (strcmp(ta->server, name) == 0) {
	    Syslog('r', "duplicate, ignore");
	    return;
	}
    }

    if ((rc = pthread_mutex_lock(&b_mutex)))
	Syslog('!', "add_server() mutex_lock failed rc=%d", rc);
    
    tmp = (srv_list *)malloc(sizeof(srv_list));
    memset(tmp, 0, sizeof(tmp));
    tmp->next = NULL;
    strncpy(tmp->server, name, 63);
    strncpy(tmp->router, router, 63);
    strncpy(tmp->prod, prod, 20);
    strncpy(tmp->vers, vers, 20);
    strncpy(tmp->fullname, fullname, 35);
    tmp->connected = now;
    tmp->users = 0;
    tmp->hops = hops;

    if (*fdp == NULL) {
	*fdp = tmp;
    } else {
	for (ta = *fdp; ta; ta = ta->next)
	    if (ta->next == NULL) {
		ta->next = (srv_list *)tmp;
		break;
	    }
    }

    if ((rc = pthread_mutex_unlock(&b_mutex)))
	Syslog('!', "add_server() mutex_unlock failed rc=%d", rc);

    srvchg = TRUE;
}



/*
 * Delete server.
 */
void del_server(srv_list **fap, char *name)
{
    srv_list	*ta, *tan;
    int		rc;
    
    Syslog('r', "delserver %s", name);

    if (*fap == NULL)
	return;

    if ((rc = pthread_mutex_lock(&b_mutex)))
	Syslog('!', "del_server() mutex_lock failed rc=%d", rc);

    for (ta = *fap; ta; ta = ta->next) {
	while ((tan = ta->next) && (strcmp(tan->server, name) == 0)) {
	    ta->next = tan->next;
	    free(tan);
	    srvchg = TRUE;
	}
	ta->next = tan;
    }

    if ((rc = pthread_mutex_unlock(&b_mutex)))
	Syslog('!', "del_server() mutex_unlock failed rc=%d", rc);
}



/*  
 * Delete router.
 */ 
void del_router(srv_list **fap, char *name)
{   
    srv_list	*ta, *tan;
    int		rc;
    
    Syslog('r', "delrouter %s", name);

    if (*fap == NULL)
	return;

    if ((rc = pthread_mutex_lock(&b_mutex)))
	Syslog('!', "del_router() mutex_lock failed rc=%d", rc);

    for (ta = *fap; ta; ta = ta->next) {
	while ((tan = ta->next) && (strcmp(tan->router, name) == 0)) {
	    ta->next = tan->next;
	    free(tan);
	    srvchg = TRUE;
	}
	ta->next = tan;
    }

    if ((rc = pthread_mutex_unlock(&b_mutex)))
	Syslog('!', "del_router() mutex_unlock failed rc=%d", rc);
}



/*
 * Send a message to all servers
 */
void send_all(const char *format, ...)
{
    ncs_list	*tnsl;
    char	buf[512];
    va_list	va_ptr;

    va_start(va_ptr, format);
    vsprintf(buf, format, va_ptr);
    va_end(va_ptr);

    for (tnsl = ncsl; tnsl; tnsl = tnsl->next) {
	if (tnsl->state == NCS_CONNECT) {
	    send_msg(tnsl, buf);
	}
    }
}



/*
 * Broadcast a message to all servers except the originating server
 */
void broadcast(char *origin, const char *format, ...)
{
    ncs_list    *tnsl;
    va_list     va_ptr;
    char	buf[512];

    va_start(va_ptr, format);
    vsprintf(buf, format, va_ptr);
    va_end(va_ptr);
    
    for (tnsl = ncsl; tnsl; tnsl = tnsl->next) {
	if ((tnsl->state == NCS_CONNECT) && (strcmp(origin, tnsl->server))) {
	    send_msg(tnsl, buf);
	}
    }
}



/*
 * Send message to a server
 */
int send_msg(ncs_list *tnsl, const char *format, ...)
{
    char	buf[512];
    va_list     va_ptr;
	
    va_start(va_ptr, format);
    vsprintf(buf, format, va_ptr);
    va_end(va_ptr);

    Syslog('r', "> %s: %s", tnsl->server, printable(buf, 0));

    if (sendto(tnsl->socket, buf, strlen(buf), 0, (struct sockaddr *)&tnsl->servaddr_in, sizeof(struct sockaddr_in)) == -1) {
	Syslog('r', "$IBC: can't send message");
	return -1;
    }
    return 0;
}



void check_servers(void)
{
    char	    *errmsg, scfgfn[PATH_MAX];
    FILE	    *fp;
    ncs_list	    *tnsl;
    int		    j, inlist;
    int		    a1, a2, a3, a4;
    struct servent  *se;
    struct hostent  *he;

    sprintf(scfgfn, "%s/etc/ibcsrv.data", getenv("MBSE_ROOT"));
    
    /*
     * Check if configuration is changed, if so then apply the changes.
     */
    if (file_time(scfgfn) != scfg_time) {
	Syslog('r', "%s filetime changed, rereading");

	if (servers == NULL) {
	    /*
	     * First add this server name to the servers database.
	     */
	    add_server(&servers, CFG.myfqdn, 0, (char *)"mbsebbs", (char *)VERSION, CFG.bbs_name, (char *)"none");
	}

	if ((fp = fopen(scfgfn, "r"))) {
	    fread(&ibcsrvhdr, sizeof(ibcsrvhdr), 1, fp);

	    while (fread(&ibcsrv, ibcsrvhdr.recsize, 1, fp)) {
		Syslog('r', "IBC server \"%s\", Active %s", ibcsrv.server, ibcsrv.Active ?"Yes":"No");
		if (ibcsrv.Active) {
		    inlist = FALSE;
		    for (tnsl = ncsl; tnsl; tnsl = tnsl->next) {
			if (strcmp(tnsl->server, ibcsrv.server) == 0) {
			    inlist = TRUE;
			}
		    }
		    if (!inlist ) {
			Syslog('r', "  not in neighbour list, add");
			fill_ncslist(&ncsl, ibcsrv.server, ibcsrv.myname, ibcsrv.passwd);
			changed = TRUE;
			Syslog('+', "IBC: added Internet BBS Chatserver %s", ibcsrv.server);
		    }
		}
	    }

	    /*
	     * Now check for neighbours to delete
	     */
	    for (tnsl = ncsl; tnsl; tnsl = tnsl->next) {
		fseek(fp, ibcsrvhdr.hdrsize, SEEK_SET);
		inlist = FALSE;
		Syslog('r', "IBC server \"%s\"", ibcsrv.server);

		while (fread(&ibcsrv, ibcsrvhdr.recsize, 1, fp)) {
		    if ((strcmp(tnsl->server, ibcsrv.server) == 0) && ibcsrv.Active) {
			inlist = TRUE;
		    }
		}
		if (!inlist) {
		    Syslog('r', "  not in configuration, remove");
		    tnsl->remove = TRUE;
		    tnsl->action = now;
		    changed = TRUE;
		}
	    }
	    fclose(fp);
	}

	scfg_time = file_time(scfgfn);
    }

    dump_ncslist();

    /*
     * Check if we need to make state changes
     */
    for (tnsl = ncsl; tnsl; tnsl = tnsl->next) {
	if (((int)tnsl->action - (int)now) <= 0) {
	    switch (tnsl->state) {
		case NCS_INIT:	    Syslog('r', "%s init", tnsl->server);
				    changed = TRUE;

				    /*
				     * If Internet is available, setup the connection.
				     */
				    if (internet) {
					/*
					 * Get IP address for the hostname, set default next action
					 * to 60 seconds.
					 */
					tnsl->action = now + (time_t)60;
					memset(&tnsl->servaddr_in, 0, sizeof(struct sockaddr_in));
					se = getservbyname("fido", "udp");
					tnsl->servaddr_in.sin_family = AF_INET;
					tnsl->servaddr_in.sin_port = se->s_port;
					
					if (sscanf(tnsl->server,"%d.%d.%d.%d",&a1,&a2,&a3,&a4) == 4)
					    tnsl->servaddr_in.sin_addr.s_addr = inet_addr(tnsl->server);
					else if ((he = gethostbyname(tnsl->server)))
					    memcpy(&tnsl->servaddr_in.sin_addr, he->h_addr, he->h_length);
					else {
					    switch (h_errno) {
						case HOST_NOT_FOUND:    errmsg = (char *)"Authoritative: Host not found"; break;
						case TRY_AGAIN:         errmsg = (char *)"Non-Authoritive: Host not found"; break;
						case NO_RECOVERY:       errmsg = (char *)"Non recoverable errors"; break;
						default:                errmsg = (char *)"Unknown error"; break;
					    }
					    Syslog('!', "IBC: no IP address for %s: %s", tnsl->server, errmsg);
					    tnsl->action = now + (time_t)120;
					    tnsl->state = NCS_FAIL;
					    changed = TRUE;
					    break;
					}
					
					tnsl->socket = socket(AF_INET, SOCK_DGRAM, 0);
					if (tnsl->socket == -1) {
					    Syslog('!', "$IBC: can't create socket for %s", tnsl->server);
					    tnsl->state = NCS_FAIL;
					    tnsl->action = now + (time_t)120;
					    changed = TRUE;
					    break;
					}

					Syslog('r', "socket %d", tnsl->socket);
					tnsl->state = NCS_CALL;
					tnsl->action = now + (time_t)1;
				    } else {
					tnsl->action = now + (time_t)10;
				    }
				    break;
				    
		case NCS_CALL:	    /*
				     * In this state we accept PASS and SERVER commands from
				     * the remote with the same token as we have sent.
				     */
				    Syslog('r', "%s call", tnsl->server);
				    tnsl->token = gettoken();
				    send_msg(tnsl, "PASS %s 0100 %s\r\n", tnsl->passwd, tnsl->compress ? "Z":"");
				    send_msg(tnsl, "SERVER %s 0 %ld mbsebbs %s %s\r\n",  tnsl->myname, tnsl->token, 
					    VERSION, CFG.bbs_name);
				    tnsl->action = now + (time_t)10;
				    tnsl->state = NCS_WAITPWD;
				    changed = TRUE;
				    break;
				    
		case NCS_WAITPWD:   /*
				     * This state can be left by before the timeout is reached
				     * by a reply from the remote if the connection is accepted.
				     */
				    Syslog('r', "%s waitpwd", tnsl->server);
				    tnsl->token = 0;
				    tnsl->state = NCS_CALL;
				    while (TRUE) {
					j = 1+(int) (1.0 * CFG.dialdelay * rand() / (RAND_MAX + 1.0));
					if ((j > (CFG.dialdelay / 10)) && (j > 9))
					    break;
				    }
				    Syslog('r', "next call in %d %d seconds", CFG.dialdelay, j);
				    tnsl->action = now + (time_t)j;
				    changed = TRUE;
				    break;

		case NCS_CONNECT:   /*
				     * In this state we check if the connection is still alive
				     */
				    if (((int)now - (int)tnsl->last) > 70) {
					Syslog('+', "IBC: server %s connection is dead", tnsl->server);
					tnsl->state = NCS_DEAD;
					tnsl->action = now + (time_t)120;    // 2 minutes delay before calling again.
					tnsl->gotpass = FALSE;
					tnsl->gotserver = FALSE;
					tnsl->token = 0;
					del_router(&servers, tnsl->server);
					broadcast(tnsl->server, "SQUIT %s Connection died\r\n", tnsl->server);
					changed = TRUE;
					break;
				    }
				    if (((int)now - (int)tnsl->last) > 60) {
					send_msg(tnsl, "PING\r\n");
				    }
				    tnsl->action = now + (time_t)10;
				    break;

		case NCS_HANGUP:    Syslog('r', "%s hangup => call", tnsl->server);
				    tnsl->action = now + (time_t)1;
				    tnsl->state = NCS_CALL;
				    changed = TRUE;
				    break;

		case NCS_DEAD:	    Syslog('r', "%s dead -> call", tnsl->server);
				    tnsl->action = now + (time_t)1;
				    tnsl->state = NCS_CALL;
				    changed = TRUE;
				    break;

		case NCS_FAIL:	    Syslog('r', "%s fail => init", tnsl->server);
				    tnsl->action = now + (time_t)1;
				    tnsl->state = NCS_INIT;
				    changed = TRUE;
				    break;
	    }
	}
    }

    dump_ncslist();
}



void command_pass(char *hostname, char *parameters)
{
    ncs_list	*tnsl;
    char	*passwd, *version, *lnk;

    for (tnsl = ncsl; tnsl; tnsl = tnsl->next) {
	if (strcmp(tnsl->server, hostname) == 0) {
	    break;
	}
    }

    passwd = strtok(parameters, " \0");
    version = strtok(NULL, " \0");
    lnk = strtok(NULL, " \0");

//    Syslog('r', "passwd \"%s\"", printable(passwd, 0));
//    Syslog('r', "version \"%s\"", printable(version, 0));
//    Syslog('r', "link \"%s\"", printable(lnk, 0));

    if (version == NULL) {
	send_msg(tnsl, "461 PASS: Not enough parameters\r\n");
	return;
    }

    if (strcmp(passwd, tnsl->passwd)) {
	Syslog('!', "IBC: got bad password %s from %s", passwd, hostname);
	return;
    }

    tnsl->gotpass = TRUE;
    tnsl->version = atoi(version);
    if (lnk && strchr(lnk, 'Z'))
	tnsl->compress = TRUE;
    changed = TRUE;
}



void command_server(char *hostname, char *parameters)
{
    ncs_list	    *tnsl;
    srv_list	    *ta;
    char	    *name, *hops, *id, *prod, *vers, *fullname;
    unsigned long   token;
    int		    ihops, found = FALSE;

    name = strtok(parameters, " \0");
    hops = strtok(NULL, " \0");
    id = strtok(NULL, " \0");
    prod = strtok(NULL, " \0");
    vers = strtok(NULL, " \0");
    fullname = strtok(NULL, "\0");
    ihops = atoi(hops) + 1;

    for (tnsl = ncsl; tnsl; tnsl = tnsl->next) {
	if (strcmp(tnsl->server, name) == 0) {
	    found = TRUE;
	    break;
	}
    }

//    Syslog('r', "name \"%s\"", printable(name, 0));
//    Syslog('r', "hops \"%s\"", printable(hops, 0));
//    Syslog('r', "id   \"%s\"", printable(id, 0));
//    Syslog('r', "prod \"%s\"", printable(prod, 0));
//    Syslog('r', "vers \"%s\"", printable(vers, 0));
//    Syslog('r', "full \"%s\"", printable(fullname, 0));

    if (fullname == NULL) {
	send_msg(tnsl, "461 SERVER: Not enough parameters\r\n");
	return;
    }

    token = atoi(id);

    if (found && tnsl->token) {
	/*
	 * We are in calling state, so we expect the token from the
	 * remote is the same as the token we sent.
	 * In that case, the session is authorized.
	 */
	if (tnsl->token == token) {
	    broadcast(tnsl->server, "SERVER %s %d %s %s %s %s\r\n", name, ihops, id, prod, vers, fullname);
	    tnsl->gotserver = TRUE;
	    changed = TRUE;
	    tnsl->state = NCS_CONNECT;
	    tnsl->action = now + (time_t)10;
	    Syslog('+', "IBC: connected with %s", tnsl->server);
	    /*
	     * Send all already known servers
	     */
	    for (ta = servers; ta; ta = ta->next) {
		if (ta->hops) {
		    send_msg(tnsl, "SERVER %s %d 0 %s %s %s\r\n", ta->server, ta->hops, ta->prod, ta->vers, ta->fullname);
		}
	    }
	    add_server(&servers, tnsl->server, ihops, prod, vers, fullname, hostname);
	    return;
	}
	Syslog('r', "IBC: collision with %s", tnsl->server);
	return;
    }

    /*
     * We are in waiting state, so we sent our PASS and SERVER
     * messages and set the session to connected if we got a
     * valid PASS command.
     */
    if (found && tnsl->gotpass) {
	send_msg(tnsl, "PASS %s 0100 %s\r\n", tnsl->passwd, tnsl->compress ? "Z":"");
	send_msg(tnsl, "SERVER %s 0 %ld mbsebbs %s %s\r\n",  tnsl->myname, token, VERSION, CFG.bbs_name);
	broadcast(tnsl->server, "SERVER %s %d %s %s %s %s\r\n", name, ihops, id, prod, vers, fullname);
	tnsl->gotserver = TRUE;
	tnsl->state = NCS_CONNECT;
	tnsl->action = now + (time_t)10;
	Syslog('+', "IBC: connected with %s", tnsl->server);
	/*
	 * Send all already known servers
	 */
	for (ta = servers; ta; ta = ta->next) {
	    if (ta->hops) {
		send_msg(tnsl, "SERVER %s %d 0 %s %s %s\r\n", ta->server, ta->hops, ta->prod, ta->vers, ta->fullname);
	    }
	}
	add_server(&servers, tnsl->server, ihops, prod, vers, fullname, hostname);
	changed = TRUE;
	return;
    }

    if (! found) {
       /*
	* Got a message about a server that is not our neighbour.
	*/
	add_server(&servers, name, ihops, prod, vers, fullname, hostname);
	broadcast(hostname, "SERVER %s %d %s %s %s %s\r\n", name, ihops, id, prod, vers, fullname);
	changed = TRUE;
	return;
    }

    Syslog('r', "IBC: got SERVER command without PASS command from %s", hostname);
    return;
}



void command_squit(char *hostname, char *parameters)
{
    ncs_list    *tnsl;
    char        *name, *message;
    
    for (tnsl = ncsl; tnsl; tnsl = tnsl->next) {
	if (strcmp(tnsl->server, hostname) == 0) {
	    break;
	}
    }

    name = strtok(parameters, " \0");
    message = strtok(NULL, "\0");

    if (strcmp(name, tnsl->server) == 0) {
	Syslog('+', "IBC: disconnect server %s: %s", name, message);
	tnsl->state = NCS_HANGUP;
	tnsl->action = now + (time_t)120;	// 2 minutes delay before calling again.
	tnsl->gotpass = FALSE;
	tnsl->gotserver = FALSE;
	tnsl->token = 0;
	del_router(&servers, name);
    } else {
	Syslog('r', "IBC: disconnect server %s: message is not for us, but update database", name);
	del_server(&servers, name);
    }

    broadcast(hostname, "SQUIT %s %s\r\n", name, message);
    changed = TRUE;
}



void command_user(char *hostname, char *parameters)
{
    ncs_list    *tnsl;
    char	*nick, *server, *realname;

    for (tnsl = ncsl; tnsl; tnsl = tnsl->next) {
	if (strcmp(tnsl->server, hostname) == 0) {
	    break;
	}
    }
	
    nick = strtok(parameters, "@\0");
    server = strtok(NULL, " \0");
    realname = strtok(NULL, "\0");

    if (realname == NULL) {
	send_msg(tnsl, "461 USER: Not enough parameters\r\n");
	return;
    }
    
    if (add_user(&users, server, nick, realname) == 0)
	broadcast(hostname, "USER %s@%s %s\r\n", nick, server, realname);
}



void command_quit(char *hostname, char *parameters)
{
    ncs_list    *tnsl;
    char	*nick, *server, *message;

    for (tnsl = ncsl; tnsl; tnsl = tnsl->next) {
	if (strcmp(tnsl->server, hostname) == 0) {
	    break;
	}
    }

    nick = strtok(parameters, "@\0");
    server = strtok(NULL, " \0");
    message = strtok(NULL, "\0");

    if (server == NULL) {
	send_msg(tnsl, "461 QUIT: Not enough parameters\r\n");
	return;
    }

    if (message)
	send_all("MSG ** %s is leaving: %s\r\n", nick, message);
    else
	send_all("MSG ** %s is leaving: Quit\r\n", nick);
    del_user(&users, server, nick);
    broadcast(hostname, "QUIT %s@%s %s\r\n", nick, server, parameters);
}



void receiver(struct servent  *se)
{
    struct pollfd   pfd;
    struct hostent  *hp;
    int             rc, len, inlist;
    socklen_t       sl;
    ncs_list	    *tnsl;
    char            *hostname, *prefix, *command, *parameters;

    pfd.fd = ls;
    pfd.events = POLLIN;
    pfd.revents = 0;

    if ((rc = poll(&pfd, 1, 1000) < 0)) {
	Syslog('r', "$poll/select failed");
	return;
    } 
	
    if (pfd.revents & POLLIN || pfd.revents & POLLERR || pfd.revents & POLLHUP || pfd.revents & POLLNVAL) {
	sl = sizeof(myaddr_in);
	memset(&clientaddr_in, 0, sizeof(struct sockaddr_in));
        memset(&crbuf, 0, sizeof(crbuf));
        if ((len = recvfrom(ls, &crbuf, sizeof(crbuf)-1, 0,(struct sockaddr *)&clientaddr_in, &sl)) != -1) {
	    hp = gethostbyaddr((char *)&clientaddr_in.sin_addr, sizeof(struct in_addr), clientaddr_in.sin_family);
	    if (hp == NULL)
	        hostname = inet_ntoa(clientaddr_in.sin_addr);
	    else
	        hostname = hp->h_name;

	    if ((crbuf[strlen(crbuf) -2] != '\r') && (crbuf[strlen(crbuf) -1] != '\n')) {
	        Syslog('r', "Message not terminated with CR-LF, dropped");
	        return;
	    }

	    inlist = FALSE;
	    for (tnsl = ncsl; tnsl; tnsl = tnsl->next) {
		if (strcmp(tnsl->server, hostname) == 0) {
		    inlist = TRUE;
		    break;
		}
	    }
	    if (!inlist) {
		Syslog('!', "IBC: message from unknown host (%s), dropped", hostname);
		return;
	    }

	    if (tnsl->state == NCS_INIT) {
		Syslog('r', "IBC: message received from %s while in init state, dropped", hostname);
		return;
	    }

	    tnsl->last = now;
	    crbuf[strlen(crbuf) -2] = '\0';
	    Syslog('r', "< %s: \"%s\"", hostname, printable(crbuf, 0));

	    /*
	     * Parse message
	     */
	    if (crbuf[0] == ':') {
		prefix = strtok(crbuf, " ");
		command = strtok(NULL, " \0");
		parameters = strtok(NULL, "\0");
	    } else {
		prefix = NULL;
		command = strtok(crbuf, " \0");
		parameters = strtok(NULL, "\0");
	    }
//	    Syslog('r', "prefix     \"%s\"", printable(prefix, 0));
//	    Syslog('r', "command    \"%s\"", printable(command, 0));
//	    Syslog('r', "parameters \"%s\"", printable(parameters, 0));

	    if (! strcmp(command, (char *)"PASS")) {
		if (parameters == NULL) {
		    send_msg(tnsl, "461 %s: Not enough parameters\r\n", command);
		} else {
		    command_pass(hostname, parameters);
		}
	    } else if (! strcmp(command, (char *)"SERVER")) {
		if (parameters == NULL) {
		    send_msg(tnsl, "461 %s: Not enough parameters\r\n", command);
		} else {
		    command_server(hostname, parameters);
		}
	    } else if (! strcmp(command, (char *)"PING")) {
		send_msg(tnsl, "PONG\r\n");
	    } else if (! strcmp(command, (char *)"PONG")) {
		/*
		 * Just accept
		 */
	    } else if (! strcmp(command, (char *)"SQUIT")) {
		if (parameters == NULL) {
		    send_msg(tnsl, "461 %s: Not enough parameters\r\n", command);
		} else {
		    command_squit(hostname, parameters);
		}
	    } else if (! strcmp(command, (char *)"USER")) {
		if (parameters == NULL) {
		    send_msg(tnsl, "461 %s: Not enough parameters\r\n", command);
		} else {
		    command_user(hostname, parameters);
		}
	    } else if (! strcmp(command, (char *)"QUIT")) {
		if (parameters == NULL) {
		    send_msg(tnsl, "461 %s: Not enough parameters\r\n", command);
		} else {
		    command_quit(hostname, parameters);
		}
	    } else if (atoi(command)) {
		Syslog('r', "IBC: Got error %d", atoi(command));
	    } else if (tnsl->state == NCS_CONNECT) {
		/*
		 * Only if connected we send a error response
		 */
		send_msg(tnsl, "421 %s: Unknown command\r\n", command);
	    }
	} else {
	    Syslog('r', "recvfrom returned len=%d", len);
	}
    }
}



/*
 * IBC thread
 */
void *ibc_thread(void *dummy)
{
    struct servent  *se;
    ncs_list        *tnsl;

    Syslog('+', "Starting IBC thread");

    if ((se = getservbyname("fido", "udp")) == NULL) {
	Syslog('!', "IBC: no fido udp entry in /etc/services, cannot start Internet BBS Chat");
	goto exit;
    }

    myaddr_in.sin_family = AF_INET;
    myaddr_in.sin_addr.s_addr = INADDR_ANY;
    myaddr_in.sin_port = se->s_port;
    Syslog('+', "IBC: listen on %s, port %d", inet_ntoa(myaddr_in.sin_addr), ntohs(myaddr_in.sin_port));

    ls = socket(AF_INET, SOCK_DGRAM, 0);
    if (ls == -1) {
	Syslog('!', "$IBC: can't create listen socket");
	goto exit;
    }

    if (bind(ls, (struct sockaddr *)&myaddr_in, sizeof(struct sockaddr_in)) == -1) {
	Syslog('!', "$IBC: can't bind listen socket");
	goto exit;
    }

    ibc_run = TRUE;
    srand(getpid());

    while (! T_Shutdown) {

	now = time(NULL);
	/*
	 * Check neighbour servers state
	 */
	check_servers();

	/*
	 * Get any incoming messages
	 */
	receiver(se);
    }

    Syslog('r', "IBC: start shutdown connections");
    for (tnsl = ncsl; tnsl; tnsl = tnsl->next) {
	if (tnsl->state == NCS_CONNECT) {
	    send_msg(tnsl, "SQUIT %s System shutdown\r\n", tnsl->myname);
	}
    }

    tidy_servers(&servers);

exit:
    ibc_run = FALSE;
    Syslog('+', "IBC thread stopped");
    pthread_exit(NULL);
}


#endif
