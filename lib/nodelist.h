/* $Id$ */

#ifndef _NODELIST_H
#define	_NODELIST_H

#include "../config.h"

#define MAXNAME		35
#define MAXUFLAGS	16


/*
 *  Nodelist entry
 */
typedef struct	_node {
	faddr		addr;			/* Node address		*/
	unsigned short	upnet;			/* Uplink netnumber	*/
	unsigned short  upnode;			/* Uplink nodenumber	*/
	unsigned short	region;			/* Region belongin to	*/
	unsigned char	type;
	unsigned char	pflag;
	char		*name;			/* System name		*/
	char		*location;		/* System location	*/
	char		*sysop;			/* Sysop name		*/
	char		*phone;			/* Phone number		*/
	unsigned	speed;			/* Baudrate		*/
	unsigned long	mflags;			/* Modem flags		*/
	unsigned long	dflags;			/* ISDN flags		*/
	unsigned long	iflags;			/* TCP-IP flags		*/
	unsigned long	oflags;			/* Online flags		*/
	unsigned long	xflags;			/* Request flags	*/
	char		*uflags[MAXUFLAGS];	/* User flags		*/
	int		t1;			/* T flag, first char	*/
	int		t2;			/* T flag, second char	*/
	char		*url;			/* URL for connection	*/
	unsigned	is_cm	    : 1;	/* Node is CM		*/
	unsigned	is_icm	    : 1;	/* Node is ICM		*/
	unsigned	can_pots    : 1;	/* Can do POTS or ISDN	*/
	unsigned	can_ip	    : 1;	/* Can do TCP/IP	*/
} node;



/*
 * Memory array structures read from nodelist.conf
 */
typedef struct _nodelist_flag {
	struct _nodelist_flag	*next;
	char			*name;
	unsigned long		value;
} nodelist_flag;


typedef struct _nodelist_modem {
	struct _nodelist_modem	*next;
	char			*name;
	unsigned long		mask;
	unsigned long		value;
} nodelist_modem;


typedef struct _nodelist_array {
	struct _nodelist_array	*next;
	char			*name;
} nodelist_array;


typedef struct _nodelist_domsuf {
	struct _nodelist_domsuf	*next;
	unsigned short		zone;
	char			*name;
} nodelist_domsuf;


typedef struct _nodelist_service {
	struct _nodelist_service    *next;
	char			    *flag;
	char			    *service;
	unsigned long		    port;
} nodelist_service;


extern struct _nodelist {
	char		*domain;
	FILE		*fp;
} *nodevector;


struct _ixentry {
	unsigned short	zone;
	unsigned short	net;
	unsigned short	node;
	unsigned short	point;
};


extern struct _pkey {
	char		*key;
	unsigned char	type;
	unsigned char	pflag;
} pkey[];



nodelist_flag	    *nl_online;
nodelist_flag	    *nl_request;
nodelist_flag	    *nl_reqbits;
nodelist_modem	    *nl_pots;
nodelist_modem	    *nl_isdn;
nodelist_modem	    *nl_tcpip;
nodelist_array	    *nl_search;
nodelist_array	    *nl_dialer;
nodelist_array	    *nl_ipprefix;
nodelist_domsuf	    *nl_domsuffix;
nodelist_service    *nl_service;


/*
 * From nodelist.c
 */
int		initnl(void);
void		deinitnl(void);
node		*getnlent(faddr *);
void		olflags(unsigned long);
void		rqflags(unsigned long);
void		moflags(unsigned long);
void		diflags(unsigned long);
void		ipflags(unsigned long);


#endif

