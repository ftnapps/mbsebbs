/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: mbtask - chat server
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
#include "taskutil.h"
#include "taskregs.h"
#include "taskchat.h"
#include "taskibc.h"


#define	MAXMESSAGES 100		    /* Maximum ringbuffer for messages	*/



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
 *  The buffers
 */
_chat_messages		chat_messages[MAXMESSAGES];


int			buffer_head = 0;    /* Messages buffer head	*/
extern struct sysconfig CFG;		    /* System configuration	*/
extern int		s_bbsopen;	    /* The BBS open status	*/
extern srv_list		*servers;	    /* Connected servers	*/
extern usr_list		*users;		    /* Connected users		*/
extern chn_list		*channels;	    /* Connected channels	*/
extern int		usrchg;
extern int		chnchg;
extern int		srvchg;
extern pthread_mutex_t	b_mutex;


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
    int		first;
    usr_list	*tmpu;
    
    first = TRUE;
    for (tmpu = users; tmpu; tmpu = tmpu->next) {
	if (tmpu->pid) {
	    if (first) {
		Syslog('u', "  pid username                             nick      channel              sysop");
		Syslog('u', "----- ------------------------------------ --------- -------------------- -----");
		first = FALSE;
	    }
	    Syslog('u', "%5d %-36s %-9s %-20s %s", tmpu->pid, tmpu->realname, tmpu->nick,
		tmpu->channel, tmpu->sysop?"True ":"False");
	}
    }
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
    char        buf[512];
    va_list     va_ptr;
    usr_list	*tmpu;

    va_start(va_ptr, format);
    vsprintf(buf, format, va_ptr);
    va_end(va_ptr);

    for (tmpu = users; tmpu; tmpu = tmpu->next)
	if (tmpu->pid) {
	    system_msg(tmpu->pid, buf);
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
    char	buf[81];
    chn_list	*tmp;
    usr_list	*tmpu;

    Syslog('-', "Join pid %d to channel %s", pid, channel);

    if (channels) {
	for (tmp = channels; tmp; tmp = tmp->next) {
	    if (strcmp(tmp->name, channel) == 0) {
		for (tmpu = users; tmpu; tmpu = tmpu->next) {
		    if (tmpu->pid == pid) {

			pthread_mutex_lock(&b_mutex);
			strncpy(tmpu->channel, channel, 20);
			tmp->users++;
			pthread_mutex_unlock(&b_mutex);
			Syslog('+', "IBC: user %s has joined channel %s", tmpu->nick, channel);
			usrchg = TRUE;
			srvchg = TRUE;
			chnchg = TRUE;

			chat_dump();
			sprintf(buf, "%s has joined channel %s, now %d users", tmpu->nick, channel, tmp->users);
			chat_msg(channel, NULL, buf);

			/*
			 * The sysop channel is private to the system, no broadcast
			 */
			if (strcasecmp(channel, "#sysop"))
			    send_all("JOIN %s@%s %s\r\n", tmpu->nick, CFG.myfqdn, channel);
			return TRUE;
		    }
		}
	    }
	}
    }

    /*
     * A new channel must be created, but only the sysop may create the "sysop" channel
     */
    if (!sysop && (strcasecmp(channel, "#sysop") == 0)) {
	sprintf(buf, "*** Only the sysop may create channel \"%s\"", channel);
	system_msg(pid, buf);
	return FALSE;
    }

    /*
     * No matching channel found, add a new channel.
     */
    for (tmpu = users; tmpu; tmpu = tmpu->next) {
	if (tmpu->pid == pid) {
	    if (add_channel(&channels, channel, tmpu->nick, CFG.myfqdn) == 0) {

		pthread_mutex_lock(&b_mutex);
		strncpy(tmpu->channel, channel, 20);
		pthread_mutex_unlock(&b_mutex);
		Syslog('+', "IBC: user %s created and joined channel %s", tmpu->nick, channel);
		usrchg = TRUE;
		chnchg = TRUE;
		srvchg = TRUE;

		sprintf(buf, "* Created channel %s", channel);
		chat_msg(channel, NULL, buf);
		chat_dump();
		if (strcasecmp(channel, "#sysop"))
		    send_all("JOIN %s@%s %s\r\n", tmpu->nick, CFG.myfqdn, channel);

		return TRUE;
	    }
	}
    }

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
    char	buf[81];
    chn_list	*tmp;
    usr_list	*tmpu;

    if (strlen(reason) > 54)
	reason[54] = '\0';

    Syslog('-', "Part pid %d from channel, reason %s", pid, reason);

    for (tmpu = users; tmpu; tmpu = tmpu->next) {
	if ((tmpu->pid == pid) && strlen(tmpu->channel)) {
	    for (tmp = channels; tmp; tmp = tmp->next) {
		if (strcmp(tmp->name, tmpu->channel) == 0) {
		    /*
		     * Inform other users
		     */
		    if (reason != NULL) {
			chat_msg(tmpu->channel, tmpu->nick, reason);
		    }
		    sprintf(buf, "%s has left channel %s, %d users left", tmpu->nick, tmp->name, tmp->users);
		    chat_msg(tmpu->channel, NULL, buf);
		    if (strcasecmp(tmp->name, (char *)"#sysop")) {
			if (reason && strlen(reason))
			    send_all("PART %s@%s %s %s\r\n", tmpu->nick, CFG.myfqdn, tmp->name, reason);
			else
			    send_all("PART %s@%s %s\r\n", tmpu->nick, CFG.myfqdn, tmp->name);
		    }

		    /*
		     * Clean channel
		     */
		    pthread_mutex_lock(&b_mutex);
		    tmp->users--;
		    pthread_mutex_unlock(&b_mutex);
		    Syslog('+', "IBC: nick %s leaves channel %s", tmpu->nick, tmp->name);
		    if (tmp->users == 0) {
			/*
			 * Last user in channel, remove channel
			 */
			Syslog('+', "IBC: removed channel %s, no more users left", tmp->name);
			del_channel(&channels, tmp->name);
			chnchg = TRUE;
		    }

		    /*
		     * Update user data
		     */
		    pthread_mutex_lock(&b_mutex);
		    tmpu->channel[0] = '\0';
		    pthread_mutex_unlock(&b_mutex);
		    usrchg = TRUE;
		    chnchg = TRUE;
		    srvchg = TRUE;

		    chat_dump();
		    return TRUE;
		}
	    }
	}
    }

    return FALSE;
}



void chat_init(void)
{
    memset(&chat_messages, 0, sizeof(chat_messages));
}



void chat_cleanuser(pid_t pid)
{
    part(pid, (char *)"I'm hanging up!");
}



/*
 * Send message into channel
 */
void chat_msg(char *channel, char *nick, char *msg)
{
    char	buf[128], *logm;
    usr_list	*tmpu;

    if (nick == NULL)
	sprintf(buf, "%s", msg);
    else
	sprintf(buf, "<%s> %s", nick, msg);

    if (CFG.iAutoLog && strlen(CFG.chat_log)) {
	logm = calloc(PATH_MAX, sizeof(char));
	sprintf(logm, "%s/log/%s", getenv("MBSE_ROOT"), CFG.chat_log);
	ulog(logm, (char *)"+", channel, (char *)"-1", buf);
	free(logm);
    }
    buf[79] = '\0';

    for (tmpu = users; tmpu; tmpu = tmpu->next) {
	if (strlen(tmpu->channel) && (strcmp(tmpu->channel, channel) == 0)) {
	    system_msg(tmpu->pid, buf);
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
    int		count = 0, sys = FALSE;
    srv_list	*sl;
    usr_list	*tmpu;

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
     * Register with IBC
     */
    pid = strtok(data, ",");        /* Should be 3  */
    pid = strtok(NULL, ",");        /* The pid      */
    realname = strtok(NULL, ",");   /* Username     */
    nick = strtok(NULL, ",");       /* Mickname     */
    sys = atoi(strtok(NULL, ";"));  /* Sysop flag   */

    add_user(&users, CFG.myfqdn, nick, realname);
    send_all("USER %s@%s %s\r\n", nick, CFG.myfqdn, realname);

    /*
     * Now search the added entry to update the data
     */
    for (tmpu = users; tmpu; tmpu = tmpu->next) {
	if ((strcmp(tmpu->server, CFG.myfqdn) == 0) && (strcmp(tmpu->name, nick) == 0) && (strcmp(tmpu->realname, realname) == 0)) {
	    /*
	     * Oke, found
	     */
	    pthread_mutex_lock(&b_mutex);
	    tmpu->pid = atoi(pid);
	    tmpu->pointer = buffer_head;
	    tmpu->sysop = sys;
	    pthread_mutex_unlock(&b_mutex);
	    usrchg = TRUE;
	    srvchg = TRUE;
	    Syslog('-', "Connected user %s (%s) with chatserver, sysop %s", realname, pid, sys ? "True":"False");

            /*
	     * Now put welcome message into the ringbuffer and report success.
	     */
	    sprintf(buf, "MBSE BBS v%s chat server; type /help for help", VERSION);
	    system_msg(tmpu->pid, buf);
	    sprintf(buf, "Welcome to the Internet BBS Chat Network");
	    system_msg(tmpu->pid, buf);
	    sprintf(buf, "Current connected servers:");
	    system_msg(tmpu->pid, buf);
	    for (sl = servers; sl; sl = sl->next) {
		sprintf(buf, "  %s (%d user%s)", sl->fullname, sl->users, (sl->users == 1) ? "":"s");
		system_msg(tmpu->pid, buf);
		count += sl->users;
	    }
	    sprintf(buf, "There %s %d user%s connected", (count != 1)?"are":"is", count, (count != 1)?"s":"");
	    system_msg(tmpu->pid, buf);
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
    usr_list	*tmpu;

    Syslog('-', "CCLO:%s", data);
    memset(&buf, 0, sizeof(buf));
    pid = strtok(data, ",");
    pid = strtok(NULL, ";");
 
    for (tmpu = users; tmpu; tmpu = tmpu->next) {
	if (tmpu->pid == atoi(pid)) {
	    /*
	     * Remove from IBC network
	     */
	    send_all("QUIT %s@%s Leaving chat\r\n", tmpu->name, CFG.myfqdn);
	    del_user(&users, CFG.myfqdn, tmpu->name);
	    Syslog('-', "Closing chat for pid %s", pid);
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
    int		first, count;
    int		found;
    usr_list	*tmpu, *tmp;
    chn_list	*tmpc;
    char	temp[81];

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

    for (tmpu = users; tmpu; tmpu = tmpu->next) {
	if (tmpu->pid == atoi(pid)) {
	    if (msg[0] == '/') {
		/*
		 * A command, process this
		 */
		if (strncasecmp(msg, "/help", 5) == 0) {
		    chat_help(atoi(pid));
		    goto ack;
		} else if (strncasecmp(msg, "/echo", 5) == 0) {
		    sprintf(buf, "%s", msg);
		    system_msg(tmpu->pid, buf);
		    goto ack;
		} else if ((strncasecmp(msg, "/exit", 5) == 0) || 
		    (strncasecmp(msg, "/quit", 5) == 0) ||
		    (strncasecmp(msg, "/bye", 4) == 0)) {
                    part(tmpu->pid, (char *)"Quitting");
		    sprintf(buf, "Goodbye");
		    system_msg(tmpu->pid, buf);
		    goto hangup;
		} else if ((strncasecmp(msg, "/join", 5) == 0) ||
		    (strncasecmp(msg, "/j ", 3) == 0)) {
		    cmd = strtok(msg, " \0");
		    Syslog('-', "\"%s\"", cmd);
		    cmd = strtok(NULL, "\0");
		    Syslog('-', "\"%s\"", cmd);
		    if ((cmd == NULL) || (cmd[0] != '#') || (strcmp(cmd, "#") == 0)) {
			sprintf(buf, "** Try /join #channel");
			system_msg(tmpu->pid, buf);
		    } else if (strlen(tmpu->channel)) {
			sprintf(buf, "** Cannot join while in a channel");
			system_msg(tmpu->pid, buf);
		    } else {
			Syslog('-', "Trying to join channel %s", cmd);
			join(tmpu->pid, cmd, tmpu->sysop);
		    }
		    chat_dump();
		    goto ack;
		} else if (strncasecmp(msg, "/list", 5) == 0) {
		    first = TRUE;
		    for (tmpc = channels; tmpc; tmpc = tmpc->next) {
			if (first) {
			    sprintf(buf, "Cnt Channel name         Channel topic");
			    system_msg(tmpu->pid, buf);
			    sprintf(buf, "--- -------------------- ------------------------------------------------------");
			    system_msg(tmpu->pid, buf);
			}
			first = FALSE;
			sprintf(buf, "%3d %-20s %-54s", tmpc->users, tmpc->name, tmpc->topic);
			system_msg(tmpu->pid, buf);
		    }
		    if (first) {
			sprintf(buf, "No active channels to list");
			system_msg(tmpu->pid, buf);
		    }
		    goto ack;
		} else if (strncasecmp(msg, "/names", 6) == 0) {
		    if (strlen(tmpu->channel)) {
			sprintf(buf, "Present in channel %s:", tmpu->channel);
			system_msg(tmpu->pid, buf);
			sprintf(buf, "Nick                                     Real name                      Flags");
			system_msg(tmpu->pid, buf);
			sprintf(buf, "---------------------------------------- ------------------------------ -------");
			system_msg(tmpu->pid, buf);
			count = 0;
			for (tmp = users; tmp; tmp = tmp->next) {
			    if (strcmp(tmp->channel, tmpu->channel) == 0) {
				sprintf(temp, "%s@%s", tmp->nick, tmp->server);
				sprintf(buf, "%-40s %-30s %s", temp, tmp->realname,
				    tmp->sysop ? (char *)"sysop" : (char *)"");
				system_msg(tmpu->pid, buf);
				count++;
			    }
			}
			sprintf(buf, "%d user%s in this channel", count, (count == 1) ?"":"s");
			system_msg(tmpu->pid, buf);
		    } else {
			sprintf(buf, "** Not in a channel");
			system_msg(tmpu->pid, buf);
		    }
		    goto ack;
		} else if (strncasecmp(msg, "/nick", 5) == 0) {
		    cmd = strtok(msg, " \0");
		    cmd = strtok(NULL, "\0");
		    if ((cmd == NULL) || (strlen(cmd) == 0) || (strlen(cmd) > 9)) {
			sprintf(buf, "** Nickname must be between 1 and 9 characters");
		    } else {
			found = FALSE;
			for (tmp = users; tmp; tmp = tmp->next) {
			    if ((strcmp(tmp->name, cmd) == 0) || (strcmp(tmp->nick, cmd) == 0)) {
				found = TRUE;
			    }
			}

			if (!found ) {
			    strncpy(tmpu->nick, cmd, 9);
			    sprintf(buf, "Nick set to \"%s\"", cmd);
			    system_msg(tmpu->pid, buf);
			    send_all("NICK %s %s %s %s\r\n", tmpu->nick, tmpu->name, CFG.myfqdn, tmpu->realname);
			    usrchg = TRUE;
			    chat_dump();
			    goto ack;
			}
			sprintf(buf, "Can't set nick");
		    }
		    system_msg(tmpu->pid, buf);
		    chat_dump();
		    goto ack;
		} else if (strncasecmp(msg, "/part", 5) == 0) {
		    cmd = strtok(msg, " \0");
		    Syslog('-', "\"%s\"", cmd);
		    cmd = strtok(NULL, "\0");
		    Syslog('-', "\"%s\"", printable(cmd, 0));
		    if (part(tmpu->pid, cmd ? cmd : (char *)"Quitting") == FALSE) {
			sprintf(buf, "** Not in a channel");
			system_msg(tmpu->pid, buf);
		    }
		    chat_dump();
		    goto ack;
		} else if (strncasecmp(msg, "/topic", 6) == 0) {
		    if (strlen(tmpu->channel)) {
			sprintf(buf, "** Internal system error");
			for (tmpc = channels; tmpc; tmpc = tmpc->next) {
			    if (strcmp(tmpu->channel, tmpc->name) == 0) {
				if ((strcmp(tmpu->name, tmpc->owner) == 0) || (strcmp(tmpu->nick, tmpc->owner) == 0)) {
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
			    }
			}
		    } else {
			sprintf(buf, "** Not in a channel");
		    }
		    system_msg(tmpu->pid, buf);
		    chat_dump();
		    goto ack;
		} else {
		    /*
		     * If still here, the command was not recognized.
		     */
		    cmd = strtok(msg, " \t\r\n\0");
		    sprintf(buf, "*** \"%s\" :Unknown command", cmd+1);
		    system_msg(tmpu->pid, buf);
		    goto ack;
		}
	    }
	    if (strlen(tmpu->channel) == 0) {
		/*
		 * Trying messages while not in a channel
		 */
		sprintf(buf, "** No channel joined. Try /join #channel");
		system_msg(tmpu->pid, buf);
		chat_dump();
		goto ack;
	    } else {
		chat_msg(tmpu->channel, tmpu->nick, msg);
		send_all("PRIVMSG %s <%s> %s\r\n", tmpu->channel, tmpu->nick, msg);
		chat_dump();
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
    usr_list	*tmpu;

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

    for (tmpu = users; tmpu; tmpu = tmpu->next) {
	if (atoi(pid) == tmpu->pid) {
	    while (tmpu->pointer != buffer_head) {
		if (tmpu->pointer < MAXMESSAGES)
		    tmpu->pointer++;
		else
		    tmpu->pointer = 0;
		if (tmpu->pid == chat_messages[tmpu->pointer].topid) {
		    /*
		     * Message is for us
		     */
		    sprintf(buf, "100:2,0,%s;", chat_messages[tmpu->pointer].message);
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
    usr_list	*tmpu;

    memset(&buf, 0, sizeof(buf));
    pid = strtok(data, ",");
    pid = strtok(NULL, ";");

    if (reg_ispaging(pid)) {
	Syslog('-', "Check sysopchat for pid %s, user has paged", pid);

	/*
	 * Now check if sysop is present in the sysop channel
	 */
	for (tmpu = users; tmpu; tmpu = tmpu->next) {
	    if (atoi(pid) != tmpu->pid) {
		if (strlen(tmpu->channel) && (strcasecmp(tmpu->channel, "#sysop") == 0) && tmpu->sysop) {
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


