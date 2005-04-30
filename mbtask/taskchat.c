/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: mbtask - chat server
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
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************/

#include "../config.h"
#include "../lib/mbselib.h"
#include "taskutil.h"
#include "taskregs.h"
#include "taskchat.h"
#include "taskibc.h"


#ifndef	USE_EXPERIMENT
#define	MAXCHANNELS 10		    /* Maximum chat channels		*/
#endif
#define	MAXMESSAGES 100		    /* Maximum ringbuffer for messages	*/


typedef enum {CH_FREE, CH_PRIVATE, CH_PUBLIC} CHANNELTYPE;


/*
 *  Users connected to the chatserver.
 */
typedef struct _ch_user_rec {
    pid_t	pid;		    /* User's pid			*/
    char	realname[36];	    /* Real name                      	*/
    char	name[10];	    /* Unix name			*/
    char	nick[10];	    /* Nickname				*/
    time_t	connected;	    /* Time connected			*/
#ifdef	USE_EXPERIMENT
    char	channel[21];	    /* Connected channel		*/
#else
    int		channel;	    /* Connected channel or -1		*/
#endif
    int		pointer;	    /* Message pointer			*/
    unsigned	chatting    : 1;    /* Is chatting in a channel		*/
    unsigned	chanop	    : 1;    /* Is a chanop			*/
    unsigned	sysop	    : 1;    /* User is sysop in channel #sysop	*/
} _chat_users;



/*
 *  Buffer for messages, this is the structure of a ringbuffer which will
 *  hold all messages, private public etc. There is one global input pointer
 *  which points to the current head of the ringbuffer. When a user connects
 *  to a channel, he will get the latest messages in the channel if they
 *  are present.
 */
typedef struct	_chatmsg {
    pid_t	topid;		    /* Destination pid of message	*/
    char	fromname[36];	    /* Message from user		*/
    char	message[81];	    /* The message to display		*/
    time_t	posted;		    /* Timestamp for posted message	*/
} _chat_messages;



/*
 * List of channels
 */
#ifndef	USE_EXPERIMENT
typedef struct _channel_rec {
    char	name[21];	    /* Channel name			*/
    char	topic[55];	    /* Channel topic			*/
    pid_t	owner;		    /* Channel owner			*/
    int		users;		    /* Users in channel			*/
    time_t	created;	    /* Creation time			*/
    unsigned	active	    : 1;    /* Channel active			*/
} _channel;
#endif



/*
 *  List of banned users from a channel. This is a dynamic list.
 */
typedef struct	_banned {
    int		channel;	    /* Channel the user is banned from	*/
    char	user[36];	    /* The user who is banned		*/
} banned_users;



/*
 *  The buffers
 */
_chat_messages		chat_messages[MAXMESSAGES];
_chat_users		chat_users[MAXCLIENT];
#ifndef	USE_EXPERIMENT
_channel		chat_channels[MAXCHANNELS];
#endif


int			buffer_head = 0;    /* Messages buffer head	*/
extern struct sysconfig CFG;		    /* System configuration	*/
extern int		s_bbsopen;	    /* The BBS open status	*/
#ifdef	USE_EXPERIMENT
extern srv_list		*servers;	    /* Connected servers	*/
extern usr_list		*users;		    /* Connected users		*/
extern chn_list		*channels;	    /* Connected channels	*/
extern int		usrchg;
extern int		chnchg;
extern pthread_mutex_t	b_mutex;
#endif


/*
 * Prototypes
 */
void chat_dump(void);
void system_msg(pid_t, char *);
void chat_help(pid_t);
int join(pid_t, char *, int);
int part(pid_t, char*);



void chat_dump(void)
{
    int	    i, first;
    
    first = TRUE;
    for (i = 0; i < MAXCLIENT; i++)
	if (chat_users[i].pid) {
	    if (first) {
#ifdef	USE_EXPERIMENT
		Syslog('u', "  pid username                             nick      channel              chats sysop");
		Syslog('u', "----- ------------------------------------ --------- -------------------- ----- -----");
#else
		Syslog('u', "  pid username                             nick      ch chats sysop");
		Syslog('u', "----- ------------------------------------ --------- -- ----- -----");
#endif
		first = FALSE;
	    }
#ifdef USE_EXPERIMENT
	    Syslog('u', "%5d %-36s %-9s %-20s %s %s", chat_users[i].pid, chat_users[i].realname, chat_users[i].nick, 
		    chat_users[i].channel, chat_users[i].chatting?"True ":"False", chat_users[i].sysop?"True ":"False");
#else
	    Syslog('u', "%5d %-36s %-9s %2d %s %s", chat_users[i].pid, chat_users[i].realname, chat_users[i].nick, 
		chat_users[i].channel, chat_users[i].chatting?"True ":"False", chat_users[i].sysop?"True ":"False");
#endif
	}
    first = TRUE;
#ifndef	USE_EXPERIMENT
    for (i = 0; i < MAXCHANNELS; i++)
	if (chat_channels[i].owner) {
	    if (first) {
		Syslog('c', "channel name         owner cnt activ");
		Syslog('c', "-------------------- ----- --- -----");
		first = FALSE;
	    }
	    Syslog('c', "%-20s %5d %3d %s", chat_channels[i].name, chat_channels[i].owner, chat_channels[i].users,
		    chat_channels[i].active?"True":"False");
	}
#endif
}



/*
 * Put a system message into the chatbuffer
 */
void system_msg(pid_t pid, char *msg)
{
    if (buffer_head < MAXMESSAGES)
	buffer_head++;
    else
	buffer_head = 0;

    Syslog('-', "system_msg(%d, %s) ptr=%d", pid, msg, buffer_head);
    memset(&chat_messages[buffer_head], 0, sizeof(_chat_messages));
    chat_messages[buffer_head].topid = pid;
    sprintf(chat_messages[buffer_head].fromname, "Server");
    strncpy(chat_messages[buffer_head].message, msg, 80);
    chat_messages[buffer_head].posted = time(NULL);
}



/*
 * Shout a message to all users
 */
void system_shout(const char *format, ...)
{
    int		i;
    char        buf[512];
    va_list     va_ptr;

    va_start(va_ptr, format);
    vsprintf(buf, format, va_ptr);
    va_end(va_ptr);

    for (i = 0; i < MAXCLIENT; i++)
	if (chat_users[i].pid) {
	    system_msg(chat_users[i].pid, buf);
	}
}



/*
 * Show help
 */
void chat_help(pid_t pid)
{
    system_msg(pid, (char *)"                     Help topics available:");
    system_msg(pid, (char *)"");
    system_msg(pid, (char *)" /BYE              - Exit from chatserver");
    system_msg(pid, (char *)" /ECHO <message>   - Echo message to yourself");
    system_msg(pid, (char *)" /EXIT             - Exit from chatserver");
    system_msg(pid, (char *)" /JOIN #channel    - Join or create a channel");
    system_msg(pid, (char *)" /J #channel       - Join or create a channel");
//  system_msg(pid, (char *)" /KICK <nick>      - Kick nick out of the channel");
    system_msg(pid, (char *)" /LIST             - List active channels");
    system_msg(pid, (char *)" /NAMES            - List nicks in current channel");
    system_msg(pid, (char *)" /NICK <name>      - Set new nickname");
    system_msg(pid, (char *)" /PART             - Leave current channel");
    system_msg(pid, (char *)" /QUIT             - Exit from chatserver");
    system_msg(pid, (char *)" /TOPIC <topic>    - Set topic for current channel");
    system_msg(pid, (char *)"");
    system_msg(pid, (char *)" All other input (without a starting /) is sent to the channel.");
}



/*
 * Join a channel
 */
int join(pid_t pid, char *channel, int sysop)
{
    int		j;
    char	buf[81];
#ifdef	USE_EXPERIMENT
    chn_list	*tmp;
    usr_list	*tmpu;
#else
    int		i;
#endif

    Syslog('-', "Join pid %d to channel %s", pid, channel);

#ifdef USE_EXPERIMENT
    if (channels) {
	for (tmp = channels; tmp; tmp = tmp->next) {
	    if (strcmp(tmp->name, channel) == 0) {
		tmp->users++;
		for (j = 0; j < MAXCLIENT; j++) {
		    if (chat_users[j].pid == pid) {
			strncpy(chat_users[j].channel, channel, 20);
			chat_users[j].chatting = TRUE;
			Syslog('r', "Added user %d to channel %s", j, channel);
			chat_dump();
			sprintf(buf, "%s has joined channel %s, now %d users", chat_users[j].nick, channel, tmp->users);
			chat_msg(channel, NULL, buf);

		    	for (tmpu = users; tmpu; tmpu = tmpu->next) {
			    if ((strcmp(tmpu->server, CFG.myfqdn) == 0) && 
				((strcmp(tmpu->nick, chat_users[j].nick) == 0) || (strcmp(tmpu->name, chat_users[j].nick) == 0))) {
				pthread_mutex_lock(&b_mutex);
				strncpy(tmpu->channel, channel, 20);
				pthread_mutex_unlock(&b_mutex);
				Syslog('+', "IBC: user %s has joined channel %s", chat_users[j].nick, channel);
				usrchg = TRUE;
			    }
			}

			/*
			 * The sysop channel is private to the system, no broadcast
			 */
			if (strcasecmp(channel, "#sysop"))
			    send_all("JOIN %s@%s %s\r\n", chat_users[j].nick, CFG.myfqdn, channel);
			return TRUE;
		    }
		}
	    }
	}
    }
#else
    for (i = 0; i < MAXCHANNELS; i++) {
	if (strcasecmp(chat_channels[i].name, channel) == 0) {
	    /*
	     * Existing channel, add user to channel.
	     */
	    chat_channels[i].users++;
	    for (j = 0; j < MAXCLIENT; j++) {
		if (chat_users[j].pid == pid) {
		    chat_users[j].channel = i;
		    chat_users[j].chatting = TRUE;
		    Syslog('-', "Added user %d to channel %d", j, i);
		    chat_dump();
		    sprintf(buf, "%s has joined channel #%s, now %d users", chat_users[j].nick, channel, chat_channels[i].users);
		    chat_msg(i, NULL, buf);
		    return TRUE;
		}
	    }
	}
    }
#endif

    /*
     * A new channel must be created, but only the sysop may create the "sysop" channel
     */
#ifdef	USE_EXPERIMENT
    if (!sysop && (strcasecmp(channel, "#sysop") == 0)) {
#else
    if (!sysop && (strcasecmp(channel, "sysop") == 0)) {
#endif
	sprintf(buf, "*** Only the sysop may create channel \"%s\"", channel);
	system_msg(pid, buf);
	return FALSE;
    }

    /*
     * No matching channel found, add a new channel.
     */
#ifdef	USE_EXPERIMENT
    for (j = 0; j < MAXCLIENT; j++) {
	if (chat_users[j].pid == pid) {
	    if (add_channel(&channels, channel, chat_users[j].nick, CFG.myfqdn) == 0) {
		strncpy(chat_users[j].channel, channel, 20);
		chat_users[j].chatting = TRUE;
		Syslog('r', "Added user %d to channel %s", j, channel);
		sprintf(buf, "* Created channel %s", channel);
		chat_msg(channel, NULL, buf);
		chat_dump();
		send_all("JOIN %s@%s %s\r\n", chat_users[j].nick, CFG.myfqdn, channel);

		for (tmpu = users; tmpu; tmpu = tmpu->next) {
		    if ((strcmp(tmpu->server, CFG.myfqdn) == 0) &&
			((strcmp(tmpu->nick, chat_users[j].nick) == 0) || (strcmp(tmpu->name, chat_users[j].nick) == 0))) {
			pthread_mutex_lock(&b_mutex);
			strncpy(tmpu->channel, channel, 20);
			pthread_mutex_unlock(&b_mutex);
			Syslog('+', "IBC: user %s created and joined channel %s", chat_users[j].nick, channel);
			usrchg = TRUE;
		    }
		}

		return TRUE;
	    }
	}
    }
#else
    for (i = 0; i < MAXCHANNELS; i++) {
	if (chat_channels[i].active == FALSE) {
	    /*
	     * Got one, register channel.
	     */
	    strncpy(chat_channels[i].name, channel, 20);
	    chat_channels[i].owner = pid;
	    chat_channels[i].users = 1;
	    chat_channels[i].created = time(NULL);
	    chat_channels[i].active = TRUE;
	    Syslog('-', "Created channel %d", i);
	    /*
	     * Register user to channel
	     */
	    for (j = 0; j < MAXCLIENT; j++) {
		if (chat_users[j].pid == pid) {
		    chat_users[j].channel = i;
		    chat_users[j].chatting = TRUE;
		    Syslog('-', "Added user %d to channel %d", j, i);
		    sprintf(buf, "Created channel #%s", channel);
		    chat_msg(i, NULL, buf);
		}
	    }
	    chat_dump();
	    return TRUE;
	}
    }
#endif

    /*
     * No matching or free channels
     */
    sprintf(buf, "*** Cannot create chat channel %s, no free channels", channel);
    system_msg(pid, buf);
    Syslog('+', "%s", buf);
    return FALSE;
}



/*
 * Part from a channel
 */
int part(pid_t pid, char *reason)
{
    int		i;
    char	buf[81];
#ifdef	USE_EXPERIMENT
    chn_list	*tmp;
    usr_list	*tmpu;
#endif

    if (strlen(reason) > 54)
	reason[54] = '\0';

    Syslog('-', "Part pid %d from channel, reason %s", pid, reason);

#ifdef	USE_EXPERIMENT
    for (i = 0; i < MAXCLIENT; i++) {
	if ((chat_users[i].pid == pid) && chat_users[i].chatting) {
	    for (tmp = channels; tmp; tmp = tmp->next) {
		if (strcmp(tmp->name, chat_users[i].channel) == 0) {
		    tmp->users--;
		    chnchg = TRUE;

		    /*
		     * Inform other users
		     */
		    if (reason != NULL)
			chat_msg(chat_users[i].channel, chat_users[i].nick, reason);
		    sprintf(buf, "%s has left channel %s, %d users left", chat_users[i].nick, tmp->name, tmp->users);
		    chat_msg(chat_users[i].channel, NULL, buf);
		    if (strcmp(tmp->name, (char *)"#sysop")) {
			if (reason && strlen(reason))
			    send_all("PART %s@%s %s %s\r\n", chat_users[i].nick, CFG.myfqdn, tmp->name, reason);
			else
			    send_all("PART %s@%s %s\r\n", chat_users[i].nick, CFG.myfqdn, tmp->name);
		    }

		    /*
		     * Clean channel
		     */
		    Syslog('-', "Nick %s leaves channel %s", chat_users[i].nick, tmp->name);
		    if (tmp->users == 0) {
			/*
			 * Last user in channel, remove channel
			 */
			Syslog('-', "Remove channel %s, no more users left", tmp->name);
			del_channel(&channels, tmp->name);
		    }

		    for (tmpu = users; tmpu; tmpu = tmpu->next) {
			if ((strcmp(tmpu->server, CFG.myfqdn) == 0) &&
			    ((strcmp(tmpu->nick, chat_users[i].nick) == 0) || (strcmp(tmpu->name, chat_users[i].nick) == 0))) {
			    pthread_mutex_lock(&b_mutex);
			    tmpu->channel[0] = '\0';
			    pthread_mutex_unlock(&b_mutex);
			    Syslog('+', "IBC: user %s created and joined channel %s", chat_users[i].nick,  chat_users[i].channel);
			    usrchg = TRUE;
			}
		    }

		    chat_users[i].channel[0] = '\0';
		    chat_users[i].chatting = FALSE;
		    chat_dump();
		    return TRUE;
		}
	    }
	}
    }
#else
    for (i = 0; i < MAXCLIENT; i++) {
	if ((chat_users[i].pid == pid) && chat_users[i].chatting) {
	    chat_channels[chat_users[i].channel].users--;

	    /*
	     * Inform other users
	     */
	    if (reason != NULL)
		chat_msg(chat_users[i].channel, chat_users[i].nick, reason);
	    sprintf(buf, "%s has left channel #%s, %d users left", chat_users[i].nick, chat_channels[chat_users[i].channel].name,
		    chat_channels[chat_users[i].channel].users);
	    chat_msg(chat_users[i].channel, NULL, buf);
	    
	    /*
	     * First clean channel
	     */
	    Syslog('-', "User leaves channel %s", chat_channels[chat_users[i].channel].name);
	    if (chat_channels[chat_users[i].channel].users == 0) {
		/*
		 * Last user from channel, clear channel
		 */
		Syslog('-', "Remove channel %s, no more users left", chat_channels[chat_users[i].channel].name);
		memset(&chat_channels[chat_users[i].channel], 0, sizeof(_channel));
	    }
	    chat_users[i].channel = -1;
	    chat_users[i].chatting = FALSE;
	    chat_dump();
	    return TRUE;
	}
    }
#endif

    Syslog('-', "No channel found");
    return FALSE;
}



void chat_init(void)
{
#ifndef	USE_EXPERIMENT
    int	    i;

    for (i = 0; i < MAXCLIENT; i++)
	chat_users[i].channel = -1;
    memset(&chat_channels, 0, sizeof(chat_channels));
#endif
    memset(&chat_users, 0, sizeof(chat_users));
    memset(&chat_messages, 0, sizeof(chat_messages));
}



void chat_cleanuser(pid_t pid)
{
    part(pid, (char *)"I'm hanging up!");
}



/*
 * Send message into channel
 */
#ifdef	USE_EXPERIMENT
void chat_msg(char *channel, char *nick, char *msg)
#else
void chat_msg(int channel, char *nick, char *msg)
#endif
{
    int	    i;
    char    buf[128], *logm;

    if (nick == NULL)
	sprintf(buf, "%s", msg);
    else
	sprintf(buf, "<%s> %s", nick, msg);

    if (CFG.iAutoLog && strlen(CFG.chat_log)) {
	logm = calloc(PATH_MAX, sizeof(char));
	sprintf(logm, "%s/log/%s", getenv("MBSE_ROOT"), CFG.chat_log);
#ifdef	USE_EXPERIMENT
	ulog(logm, (char *)"+", channel, (char *)"-1", buf);
#else
	ulog(logm, (char *)"+", chat_channels[channel].name, (char *)"-1", buf);
#endif
	free(logm);
    }
    buf[79] = '\0';

    for (i = 0; i < MAXCLIENT; i++) {
#ifdef	USE_EXPERIMENT
	if ((strcmp(chat_users[i].channel, channel) == 0) && chat_users[i].chatting) {
#else
	if ((chat_users[i].channel == channel) && chat_users[i].chatting) {
#endif
	    system_msg(chat_users[i].pid, buf);
	}
    }
}



/*
 * Connect a session to the chatserver.
 */
char *chat_connect(char *data)
{
    char	*pid, *realname, *nick;
    static char buf[200];
    int		i, count = 0, sys = FALSE;
#ifdef	USE_EXPERIMENT
    srv_list	*sl;
#endif

    Syslog('-', "CCON:%s", data);
    memset(&buf, 0, sizeof(buf));

    if (IsSema((char *)"upsalarm")) {
	sprintf(buf, "100:1,*** Power failure, running on UPS;");
	return buf;
    }

    if (s_bbsopen == FALSE) {
	sprintf(buf, "100:1,*** The BBS is closed now;");
	return buf;
    }

    /*
     * Search free userslot
     */
    for (i = 0; i < MAXCLIENT; i++) {
	if (chat_users[i].pid == 0) {
	    /*
	     * Oke, found
	     */
	    pid = strtok(data, ",");	    /* Should be 3  */
	    pid = strtok(NULL, ",");	    /* The pid	    */
	    realname = strtok(NULL, ",");   /* Username	    */
	    nick = strtok(NULL, ",");	    /* Mickname	    */
	    sys = atoi(strtok(NULL, ";"));  /* Sysop flag   */
	    chat_users[i].pid = atoi(pid);
	    strncpy(chat_users[i].realname, realname, 36);
	    strncpy(chat_users[i].nick, nick, 9);
	    strncpy(chat_users[i].name, nick, 9);
	    chat_users[i].connected = time(NULL);
	    chat_users[i].pointer = buffer_head;
#ifdef	USE_EXPERIMENT
	    chat_users[i].channel[0] = '\0';
#else
	    chat_users[i].channel = -1;
#endif
	    chat_users[i].sysop = sys;

	    Syslog('-', "Connected user %s (%s) with chatserver, slot %d, sysop %s", realname, pid, i, sys ? "True":"False");

#ifdef	USE_EXPERIMENT
	    /*
	     * Register with IBC
	     */
	    add_user(&users, CFG.myfqdn, nick, realname);
	    send_all("USER %s@%s %s\r\n", nick, CFG.myfqdn, realname);
#endif

	    /*
	     * Now put welcome message into the ringbuffer and report success.
	     */
	    sprintf(buf, "MBSE BBS v%s chat server; type /help for help", VERSION);
	    system_msg(chat_users[i].pid, buf);
	    sprintf(buf, "Welcome to the Internet BBS Chat Network");
	    system_msg(chat_users[i].pid, buf);
#ifdef	USE_EXPERIMENT
	    sprintf(buf, "Current connected servers:");
	    system_msg(chat_users[i].pid, buf);
	    for (sl = servers; sl; sl = sl->next) {
		sprintf(buf, "  %s (%d user%s)", sl->fullname, sl->users, (sl->users == 1) ? "":"s");
		system_msg(chat_users[i].pid, buf);
		count += sl->users;
	    }
#endif
	    sprintf(buf, "There %s %d user%s connected", (count != 1)?"are":"is", count, (count != 1)?"s":"");
	    system_msg(chat_users[i].pid, buf);

	    sprintf(buf, "100:0;");
	    return buf;
	}
    }
    sprintf(buf, "100:1,Too many users connected;");
    return buf;
}



char *chat_close(char *data)
{
    static char buf[200];
    char	*pid;
    int		i;

    Syslog('-', "CCLO:%s", data);
    memset(&buf, 0, sizeof(buf));
    pid = strtok(data, ",");
    pid = strtok(NULL, ";");
 
    for (i = 0; i < MAXCLIENT; i++) {
	if (chat_users[i].pid == atoi(pid)) {
	    /*
	     * Remove from IBC network
	     */
#ifdef	USE_EXPERIMENT
	    del_user(&users, CFG.myfqdn, chat_users[i].name);
	    send_all("QUIT %s@%s Leaving chat\r\n", chat_users[i].name, CFG.myfqdn);
#endif
	    Syslog('-', "Closing chat for pid %s, slot %d", pid, i);
	    memset(&chat_users[i], 0, sizeof(_chat_users));
#ifdef	USE_EXPERIMENT
	    chat_users[i].channel[0] = '\0';
#else
	    chat_users[i].channel = -1;
#endif
	    sprintf(buf, "100:0;");
	    return buf;
	}
    }
    Syslog('-', "Pid %s was not connected to chatserver");
    sprintf(buf, "100:1,*** ERROR - Not connected to server;");
    return buf;
}



char *chat_put(char *data)
{
    static char buf[200];
    char	*pid, *msg, *cmd;
    int		i, first, count;
#ifdef	USE_EXPERIMENT
    int		found;
    usr_list	*tmp;
    chn_list	*tmpc;
#else
    int		j;
#endif

    Syslog('-', "CPUT:%s", data);
    memset(&buf, 0, sizeof(buf));

    if (IsSema((char *)"upsalarm")) {
	sprintf(buf, "100:2,1,*** Power alarm, running on UPS;");
	return buf;
    }

    if (s_bbsopen == FALSE) {
	sprintf(buf, "100:2,1,*** The BBS is closed now;");
	return buf;
    }
	
    pid = strtok(data, ",");
    pid = strtok(NULL, ",");
    msg = strtok(NULL, "\0");
    msg[strlen(msg)-1] = '\0';

    for (i = 0; i < MAXCLIENT; i++) {
	if (chat_users[i].pid == atoi(pid)) {
	    if (msg[0] == '/') {
		/*
		 * A command, process this
		 */
		if (strncasecmp(msg, "/help", 5) == 0) {
		    chat_help(atoi(pid));
		    goto ack;
		} else if (strncasecmp(msg, "/echo", 5) == 0) {
		    sprintf(buf, "%s", msg);
		    system_msg(chat_users[i].pid, buf);
		    goto ack;
		} else if ((strncasecmp(msg, "/exit", 5) == 0) || 
		    (strncasecmp(msg, "/quit", 5) == 0) ||
		    (strncasecmp(msg, "/bye", 4) == 0)) {
		    part(chat_users[i].pid, (char *)"Quitting");
		    sprintf(buf, "Goodbye");
		    system_msg(chat_users[i].pid, buf);
		    goto hangup;
		} else if ((strncasecmp(msg, "/join", 5) == 0) ||
		    (strncasecmp(msg, "/j ", 3) == 0)) {
		    cmd = strtok(msg, " \0");
		    Syslog('-', "\"%s\"", cmd);
		    cmd = strtok(NULL, "\0");
		    Syslog('-', "\"%s\"", cmd);
		    if ((cmd == NULL) || (cmd[0] != '#') || (strcmp(cmd, "#") == 0)) {
			sprintf(buf, "** Try /join #channel");
			system_msg(chat_users[i].pid, buf);
#ifdef	USE_EXPERIMENT
		    } else if (strlen(chat_users[i].channel)) {
#else
		    } else if (chat_users[i].channel != -1) {
#endif
			sprintf(buf, "** Cannot join while in a channel");
			system_msg(chat_users[i].pid, buf);
		    } else {
			Syslog('-', "Trying to join channel %s", cmd);
#ifdef	USE_EXPERIMENT
			join(chat_users[i].pid, cmd, chat_users[i].sysop);
#else
			join(chat_users[i].pid, cmd+1, chat_users[i].sysop);
#endif
		    }
		    chat_dump();
		    goto ack;
		} else if (strncasecmp(msg, "/list", 5) == 0) {
		    first = TRUE;
#ifdef USE_EXPERIMENT
		    for (tmpc = channels; tmpc; tmpc = tmpc->next) {
			if (first) {
			    sprintf(buf, "Cnt Channel name         Channel topic");
			    system_msg(chat_users[i].pid, buf);
			    sprintf(buf, "--- -------------------- ------------------------------------------------------");
			    system_msg(chat_users[i].pid, buf);
			}
			first = FALSE;
			sprintf(buf, "%3d %-20s %-54s", tmpc->users, tmpc->name, tmpc->topic);
			system_msg(chat_users[i].pid, buf);
#else
		    for (j = 0; j < MAXCHANNELS; j++) {
			if (chat_channels[j].owner && chat_channels[j].active) {
			    if (first) {
				sprintf(buf, "Cnt Channel name         Channel topic");
				system_msg(chat_users[i].pid, buf);
				sprintf(buf, "--- -------------------- ------------------------------------------------------");
				system_msg(chat_users[i].pid, buf);
			    }
			    first = FALSE;
			    sprintf(buf, "%3d %-20s %-54s", chat_channels[j].users, chat_channels[j].name, chat_channels[j].topic);
			    system_msg(chat_users[i].pid, buf);
			}
#endif
		    }
		    if (first) {
			sprintf(buf, "No active channels to list");
			system_msg(chat_users[i].pid, buf);
		    }
		    goto ack;
		} else if (strncasecmp(msg, "/names", 6) == 0) {
#ifdef USE_EXPERIMENT
		    if (strlen(chat_users[i].channel)) {
#else
		    if (chat_users[i].channel != -1) {
#endif
			sprintf(buf, "Present in this channel:");
			system_msg(chat_users[i].pid, buf);
			count = 0;
#ifdef	USE_EXPERIMENT
			for (tmp = users; tmp; tmp = tmp->next) {
			    Syslog('r', "/names list %s %s", tmp->name, tmp->channel);
			    if (strcmp(tmp->channel, chat_users[i].channel) == 0) {
				sprintf(buf, "%s@%s (%s)%s", tmp->nick, tmp->server, tmp->realname,
				    tmp->chanop ? (char *)" (chanop)" : (char *)"");
				system_msg(chat_users[i].pid, buf);
				count++;
			    }
			}
#else
			for (j = 0; j < MAXCLIENT; j++) {
			    if ((chat_users[j].channel == chat_users[i].channel) && chat_users[j].pid) {
				sprintf(buf, "%s %s", chat_users[j].nick, 
					chat_users[j].chanop ?"(chanop)": chat_users[j].sysop ?"(sysop)":"");
				system_msg(chat_users[i].pid, buf);
				count++;
			    }
			}
#endif
			sprintf(buf, "%d user%s in this channel", count, (count == 1) ?"":"s");
			system_msg(chat_users[i].pid, buf);
		    } else {
			sprintf(buf, "** Not in a channel");
			system_msg(chat_users[i].pid, buf);
		    }
		    goto ack;
		} else if (strncasecmp(msg, "/nick", 5) == 0) {
		    cmd = strtok(msg, " \0");
		    cmd = strtok(NULL, "\0");
		    if ((cmd == NULL) || (strlen(cmd) == 0) || (strlen(cmd) > 9)) {
			sprintf(buf, "** Nickname must be between 1 and 9 characters");
		    } else {
#ifdef	USE_EXPERIMENT
			found = FALSE;
			for (tmp = users; tmp; tmp = tmp->next) {
			    if ((strcmp(tmp->name, cmd) == 0) || (strcmp(tmp->nick, cmd) == 0)) {
				found = TRUE;
			    }
			}

			if (!found ) {
			    for (tmp = users; tmp; tmp = tmp->next) {
				if ((strcmp(tmp->server, CFG.myfqdn) == 0) && (strcmp(tmp->realname, chat_users[i].realname) == 0)) {
				    found = TRUE;
				    strncpy(tmp->nick, cmd, 9);
				    strncpy(chat_users[i].nick, cmd, 9);
				    sprintf(buf, "Nick set to \"%s\"", cmd);
				    system_msg(chat_users[i].pid, buf);
				    send_all("NICK %s %s %s %s\r\n", chat_users[i].nick, chat_users[i].name, 
					    CFG.myfqdn, chat_users[i].realname);
				    usrchg = TRUE;
				    chat_dump();
				    goto ack;
				}
			    }
			}
			sprintf(buf, "Can't set nick");
		    }
#else
			strncpy(chat_users[i].nick, cmd, 9);
			sprintf(buf, "Nick set to \"%s\"", cmd);
		    }
#endif
		    system_msg(chat_users[i].pid, buf);
		    chat_dump();
		    goto ack;
		} else if (strncasecmp(msg, "/part", 5) == 0) {
		    cmd = strtok(msg, " \0");
		    Syslog('-', "\"%s\"", cmd);
		    cmd = strtok(NULL, "\0");
		    Syslog('-', "\"%s\"", printable(cmd, 0));
		    if (part(chat_users[i].pid, cmd ? cmd : (char *)"Quitting") == FALSE) {
			sprintf(buf, "** Not in a channel");
			system_msg(chat_users[i].pid, buf);
		    }
		    chat_dump();
		    goto ack;
		} else if (strncasecmp(msg, "/topic", 6) == 0) {
#ifdef	USE_EXPERIMENT
		    if (strlen(chat_users[i].channel)) {
			sprintf(buf, "** Internal system error");
			for (tmpc = channels; tmpc; tmpc = tmpc->next) {
			    if (strcmp(chat_users[i].channel, tmpc->name) == 0) {
				if ((strcmp(chat_users[i].name, tmpc->owner) == 0) || (strcmp(chat_users[i].nick, tmpc->owner) == 0)) {
				    cmd = strtok(msg, " \0");
				    cmd = strtok(NULL, "\0");
				    if ((cmd == NULL) || (strlen(cmd) == 0) || (strlen(cmd) > 54)) {
					sprintf(buf, "** Topic must be between 1 and 54 characters");
				    } else {
					strncpy(tmpc->topic, cmd, 54);
					sprintf(buf, "Topic set to \"%s\"", cmd);
					send_all("TOPIC %s %s\r\n", tmpc->name, tmpc->topic);
				    }
				} else {
				    sprintf(buf, "** You are not the channel owner");
				}
				break;
			    } else {
				Syslog('r', "channel %s is not what we want", tmpc->name);
			    }
			}
#else
		    if (chat_users[i].channel != -1) {
			if (chat_channels[chat_users[i].channel].owner == chat_users[i].pid) {
			    cmd = strtok(msg, " \0");
			    cmd = strtok(NULL, "\0");
			    if ((cmd == NULL) || (strlen(cmd) == 0) || (strlen(cmd) > 54)) {
				sprintf(buf, "** Topic must be between 1 and 54 characters");
			    } else {
				strncpy(chat_channels[chat_users[i].channel].topic, cmd, 54);
				sprintf(buf, "Topic set to \"%s\"", cmd);
			    }
			} else {
			    sprintf(buf, "** You are not the channel owner");
			}
#endif
		    } else {
			sprintf(buf, "** Not in a channel");
		    }
		    system_msg(chat_users[i].pid, buf);
		    chat_dump();
		    goto ack;
		} else {
		    /*
		     * If still here, the command was not recognized.
		     */
		    cmd = strtok(msg, " \t\r\n\0");
		    sprintf(buf, "*** \"%s\" :Unknown command", cmd+1);
		    system_msg(chat_users[i].pid, buf);
		    goto ack;
		}
	    }
#ifdef	USE_EXPERIMENT
	    if (strlen(chat_users[i].channel) == 0) {
#else
	    if (chat_users[i].channel == -1) {
#endif
		/*
		 * Trying messages while not in a channel
		 */
		sprintf(buf, "** No channel joined. Try /join #channel");
		system_msg(chat_users[i].pid, buf);
		chat_dump();
		goto ack;
	    } else {
		chat_msg(chat_users[i].channel, chat_users[i].nick, msg);
		chat_dump();
#ifdef	USE_EXPERIMENT
		send_all("PRIVMSG %s <%s> %s\r\n", chat_users[i].channel, chat_users[i].nick, msg);
#endif
	    }
	    goto ack;
	}
    }
    Syslog('-', "Pid %s was not connected to chatserver");
    sprintf(buf, "100:2,1,*** ERROR - Not connected to server;");
    return buf;

ack:
    sprintf(buf, "100:0;");
    return buf;

hangup:
    sprintf(buf, "100:2,1,Disconnecting;");
    return buf;
}



/*
 * Check for a message for the user. Return the message or signal that
 * nothing is there to display.
 */
char *chat_get(char *data)
{
    static char buf[200];
    char	*pid;
    int		i;
    
    if (IsSema((char *)"upsalarm")) {
	sprintf(buf, "100:2,1,*** Power failure, running on UPS;");
	return buf;
    }

    if (s_bbsopen == FALSE) {
	sprintf(buf, "100:2,1,*** The BBS is closed now;");
	return buf;
    }

    memset(&buf, 0, sizeof(buf));
    pid = strtok(data, ",");
    pid = strtok(NULL, ";");

    for (i = 0; i < MAXCLIENT; i++) {
	if (atoi(pid) == chat_users[i].pid) {
	    /*
	     * First check if we are a normal user in the sysop channel
	     */
//	    if ((! chat_users[i].sysop) && (strcasecmp(channels[chat_users[i].channel].name, "sysop") == 0)) {
//	    }

	    while (chat_users[i].pointer != buffer_head) {
		if (chat_users[i].pointer < MAXMESSAGES)
		    chat_users[i].pointer++;
		else
		    chat_users[i].pointer = 0;
		if (chat_users[i].pid == chat_messages[chat_users[i].pointer].topid) {
		    /*
		     * Message is for us.
		     */
		    sprintf(buf, "100:2,0,%s;", chat_messages[chat_users[i].pointer].message);
		    Syslog('-', "%s", buf);
		    return buf;
		}
	    }
	    sprintf(buf, "100:0;");
	    return buf;
	}
    }
    sprintf(buf, "100:2,1,*** ERROR - Not connected to server;");
    return buf;
}



/*
 * Check for sysop present for forced chat
 */
char *chat_checksysop(char *data)
{
    static char	buf[20];
    char	*pid;
    int		i;

    memset(&buf, 0, sizeof(buf));
    pid = strtok(data, ",");
    pid = strtok(NULL, ";");

    if (reg_ispaging(pid)) {
	Syslog('-', "Check sysopchat for pid %s, user has paged", pid);

	/*
	 * Now check if sysop is present in the sysop channel
	 */
	for (i = 0; i < MAXCLIENT; i++) {
	    if (atoi(pid) != chat_users[i].pid) {
		if (chat_users[i].chatting && chat_users[i].sysop) {
		    Syslog('-', "Sending ACK on check");
		    sprintf(buf, "100:1,1;");
		    reg_sysoptalk(pid);
		    return buf;
		}
	    }
	}
    }

    sprintf(buf, "100:1,0;");
    return buf;
}


