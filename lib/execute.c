/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: Execute subprogram
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


int	e_pid = 0;		/* Execute child pid	*/



int _execute(char **, char *, char *, char *);
int _execute(char **args, char *in, char *out, char *err)
{
    char    buf[PATH_MAX];
    int	    i, pid, terrno = 0, status = 0, rc = 0;

    memset(&buf, 0, sizeof(buf));
    for (i = 0; i < 16; i++) {
	if (args[i])
	    sprintf(buf, "%s %s", buf, args[i]);
	else
	    break;
    }
    Syslog('+', "Execute:%s",buf);

    fflush(stdout);
    fflush(stderr);

    if ((pid = fork()) == 0) {
	if (in) {
	    close(0);
	    if (open(in,O_RDONLY) != 0) {
		WriteError("$Reopen of stdin to %s failed", MBSE_SS(in));
		exit(MBERR_EXEC_FAILED);
	    }
	}
	if (out) {
	    close(1);
	    if (open(out,O_WRONLY | O_APPEND | O_CREAT,0600) != 1) {
		WriteError("$Reopen of stdout to %s failed", MBSE_SS(out));
		exit(MBERR_EXEC_FAILED);
	    }
	}
	if (err) {
	    close(2);
	    if (open(err,O_WRONLY | O_APPEND | O_CREAT,0600) != 2) {
		WriteError("$Reopen of stderr to %s failed", MBSE_SS(err));
		exit(MBERR_EXEC_FAILED);
	    }
	}

	errno = 0;
	rc = getpriority(PRIO_PROCESS, 0);
	if (errno == 0) {
	    rc = setpriority(PRIO_PROCESS, 0, 15);
	    if (rc)
		WriteError("$execv can't set priority to 15");
	}
	rc = execv(args[0],args);
	WriteError("$execv \"%s\" returned %d", MBSE_SS(args[0]), rc);
	setpriority(PRIO_PROCESS, 0, 0);
	exit(MBERR_EXEC_FAILED);
    }

    e_pid = pid;

    do {
	rc = wait(&status);
	e_pid = 0;
    } while (((rc > 0) && (rc != pid)) || ((rc == -1) && (errno == EINTR)));

    terrno = errno;
    setpriority(PRIO_PROCESS, 0, 0);
    errno = terrno;

    switch (rc) {
	case -1:
		/*
		 * Seems to be a problem on fast systems, hope it is ok.
		 */
		if (errno == ECHILD) {
		    Syslog('+', "Execute: no child process, this seems well");
		    return 0;
		} else {
		    WriteError("$Wait returned %d, status %d,%d", rc,status>>8,status&0xff);
		    return -1;
		}
	case 0:
		return 0;
	default:
		if (WIFEXITED(status)) {
		    rc = WEXITSTATUS(status);
		    if (rc) {
			WriteError("Execute: returned error %d", rc);
			return (rc + MBERR_EXTERNAL);
		    }
		}
		if (WIFSIGNALED(status)) {
		    rc = WTERMSIG(status);
		    WriteError("Wait stopped on signal %d", rc);
		    return rc;
		}
		if (rc)
		    WriteError("Wait stopped unknown, rc=%d", rc);
		return rc;	
    }
    return 0;
}



int execute(char **args, char *in, char *out, char *err)
{
    int	    rc;

#ifdef __linux__
    sync();
#endif
    msleep(300);
    rc = _execute(args, in, out, err);
#ifdef __linux__
    sync();
#endif
    msleep(300);
    return rc;
}



/*
 * The old behaviour, parse command strings to arguments.
 */
int execute_str(char *cmd, char *fil, char *pkt, char *in, char *out, char *err)
{
    int     i;
    char    *args[16], buf[PATH_MAX];

    memset(args, 0, sizeof(args));
    memset(&buf, 0, sizeof(buf));
    i = 0;

    if ((pkt != NULL) && strlen(pkt))
	sprintf(buf, "%s %s %s", cmd, fil, pkt);
    else
	sprintf(buf, "%s %s", cmd, fil);
	
    args[i++] = strtok(buf, " \t\0");
    while ((args[i++] = strtok(NULL," \t\n")) && (i < 15));
    args[i++] = NULL;

    return execute(args, in, out, err);
}



/*
 * Execute command in the PATH.
 */
int execute_pth(char *prog, char *opts, char *in, char *out, char *err)
{
    char    *pth;
    int	    rc;

    if (strchr(prog, ' ') || strchr(prog, '/')) {
	WriteError("First parameter of execute_pth() must be a program name");
	return -1;
    }

    pth = xstrcpy((char *)"/usr/bin/");
    pth = xstrcat(pth, prog);
    if (access(pth, X_OK) == -1) {
	free(pth);
	pth = xstrcpy((char *)"/usr/local/bin/");
	pth = xstrcat(pth, prog);
	if (access(pth, X_OK) == -1) {
	    free(pth);
	    pth = xstrcpy((char *)"/bin/");
	    pth = xstrcat(pth, prog);
	    if (access(pth, X_OK) == -1) {
		free(pth);
		pth = xstrcpy((char *)"/usr/pkg/bin/");
		pth = xstrcat(pth, prog);
		if (access(pth, X_OK) == -1) {
		    WriteError("Can't find %s", prog);
		    free(pth);
		    return -1;
		}
	    }
	}
    }

    rc = execute_str(pth, opts, NULL, in, out, err);
    free(pth);
    return rc;
}



#define SHELL "/bin/sh"


int _execsh(char *, char *, char *, char *);
int _execsh(char *cmd, char *in, char *out, char *err)
{
    int	pid, status, rc, sverr;

    Syslog('+', "Execute shell: %s", MBSE_SS(cmd));
    fflush(stdout);
    fflush(stderr);

    if ((pid = fork()) == 0) {
	if (in) {
	    close(0);
	    if (open(in, O_RDONLY) != 0) {
		WriteError("$Reopen of stdin to %s failed",MBSE_SS(in));
		exit(MBERR_EXEC_FAILED);
	    }
	}
	if (out) {
	    close(1);
	    if (open(out, O_WRONLY | O_APPEND | O_CREAT,0600) != 1) {
		WriteError("$Reopen of stdout to %s failed",MBSE_SS(out));
		exit(MBERR_EXEC_FAILED);
	    }
	}
	if (err) {
	    close(2);
	    if (open(err, O_WRONLY | O_APPEND | O_CREAT,0600) != 2) {
		WriteError("$Reopen of stderr to %s failed",MBSE_SS(err));
		exit(MBERR_EXEC_FAILED);
	    }
	}

	rc = execl(SHELL, "sh", "-c", cmd, NULL);
	WriteError("$execl \"%s\" returned %d", MBSE_SS(cmd), rc);
	exit(MBERR_EXEC_FAILED);
    }

    e_pid = pid;

    do {
	rc = wait(&status);
	e_pid = 0;
	sverr = errno;
	if (status)
	    WriteError("$Wait returned %d, status %d,%d", rc, status >> 8, status & 0xff);
    }

    while (((rc > 0) && (rc != pid)) || ((rc == -1) && (sverr == EINTR))); 
    if (rc == -1) {
	WriteError("$Wait returned %d, status %d,%d", rc, status >> 8, status & 0xff);
	return 0;
    }

    return status;
}


int execsh(char *cmd, char *in, char *out, char *err)
{
    int	rc;

#ifdef __linux__
    sync();
#endif
    msleep(300);
    rc = _execsh(cmd, in, out, err);
#ifdef __linux__
    sync();
#endif
    msleep(300);
    return rc;
}

