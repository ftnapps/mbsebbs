#ifndef _TASKIBC_H
#define	_TASKIBC_H

/* $Id$ */

#ifdef	USE_EXPERIMENT


/*
 * Linked list of neighbour chatservers
 */
typedef struct _ncs_list {
    struct _ncs_list	*next;
    char		server[64];	    /* Server address		*/
    char		myname[64];	    /* My server address	*/
    char		passwd[16];	    /* Server password		*/
    int			state;		    /* Connection state		*/
    time_t		action;		    /* Time for next action	*/
    time_t		last;		    /* Last received message	*/
    int			version;	    /* Protocol version of peer	*/
    unsigned		remove	    : 1;    /* If entry must be removed	*/
    unsigned		compress    : 1;    /* User link compression	*/
    unsigned		gotpass	    : 1;    /* Received valid password	*/
    unsigned		gotserver   : 1;    /* Received valid server	*/
    struct sockaddr_in	servaddr_in;	    /* Peer socketaddress	*/
    int			socket;		    /* Peer socket		*/
    unsigned long	token;		    /* Server token		*/
} ncs_list;



/*
 * Database with servers
 */
typedef struct _srv_list {
    struct _srv_list	*next;
    char		server[64];	    /* FQDN of the server	*/
    char		router[64];	    /* Route to this server	*/
    int			hops;		    /* Howmany hops away	*/
    time_t		connected;	    /* Connection time		*/
    char		prod[21];	    /* Product name		*/
    char		vers[21];	    /* Version string		*/
    char		fullname[37];	    /* Full BBS name		*/
    int			users;		    /* Users in chat		*/
} srv_list;



/*
 * Database with users
 */
typedef struct _usr_list {
    struct _usr_list	*next;
    char		server[64];	    /* FQDN of users server	*/
    char		name[10];	    /* Users unix name		*/
    char		nick[10];	    /* Users nick name		*/
    char		realname[37];	    /* Users real name		*/
    char		channel[21];	    /* Users channel		*/
    time_t		connected;	    /* Users connect time	*/
    unsigned		chanop	    : 1;    /* User is a chanop		*/
    unsigned		sysop	    : 1;    /* User is a sysop		*/
    unsigned		chatting    : 1;    /* User is chating		*/
    pid_t		pid;		    /* Users pid if local	*/
    int			pointer;	    /* Users message pointer	*/
} usr_list;



/*
 * Database with channels
 */
typedef struct _chn_list {
    struct _chn_list	*next;
    char		server[64];	    /* Originating server	*/
    char		name[21];	    /* Channel name		*/
    char		topic[55];	    /* Channel topic		*/
    char		owner[10];	    /* Channel owner		*/
    time_t		created;	    /* Channel created		*/
    time_t		lastmsg;	    /* Last message in channel	*/
    int			users;		    /* Users in channel		*/
} chn_list;



int  add_user(usr_list **, char *, char *, char *);
void del_user(usr_list **, char *, char *);
int  add_channel(chn_list **, char *, char *, char *);
void del_channel(chn_list **, char *);
int  do_command(char *, char *, char *);

void send_all(const char *, ...);
void *ibc_thread(void *);

#endif

#endif
