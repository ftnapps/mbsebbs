#
# $Id$
#
# Installation script to install bootscripts.
#
PATH=/bin:/sbin:/usr/bin:/usr/sbin:${MBSE_ROOT}/bin
DISTNAME=
DISTVERS=
DISTINIT=
OSTYPE=`uname -s`

#------------------------------------------------------------------------
#
# Logging procedure, needs two parameters.
#
log() {
    /bin/echo `date +%d-%b-%y\ %X ` $1 $2 >> installinit.log
}



#------------------------------------------------------------------------
#

log "+" "installinit started"

# Basic checks.
if [ `whoami` != "root" ]; then
cat << EOF
*** Run $0 as root only! ***

  Because some of the system files must be changed, you must be root
  to use this script.

*** SETUP aborted ***
EOF
        log "!" "Aborted, not root"
        exit 2
fi

if [ "${MBSE_ROOT}" = "" ]; then
        echo "*** The MBSE_ROOT doesn't exist ***"
        log "!" "Aborted, MBSE_ROOT variable doesn't exist"
        exit 2
fi


# First do various tests to see which Linux distribution this is.
#
if [ "$OSTYPE" = "Linux" ]; then
    if [ -f /etc/slackware-version ]; then
    	# Slackware 7.0 and later
    	DISTNAME="Slackware"
    	DISTVERS=`cat /etc/slackware-version`
    else
    	if [ -f /etc/debian_version ]; then
	    # Debian, at least since version 2.2
	    DISTNAME="Debian"
	    DISTVERS=`cat /etc/debian_version`
    	else
	    if [ -f /etc/SuSE-release ]; then
	    	DISTNAME="SuSE"
	    	DISTVERS=`cat /etc/SuSE-release | grep VERSION | awk '{ print $3 }'`
	    else
		# Mandrake test before RedHat, Mandrake has a redhat-release
		# file also which is a symbolic link to mandrake-release.
		if [ -f /etc/mandrake-release ]; then
		    DISTNAME="Mandrake"
		    # Format: Linux Mandrake release 8.0 (Cooker) for i586
		    DISTVERS="`cat /etc/mandrake-release | awk '{ print $4 }'`"
		else
		    if [ -f /etc/redhat-release ]; then
			DISTNAME="RedHat"
			if [ -z "`grep e-smith /etc/redhat-release`" ]; then
			    DISTVERS=`cat /etc/redhat-release | awk '{ print $5 }'`
			else
			    DISTVERS=`cat /etc/redhat-release | awk '{ print $13 }' | tr -d \)`
			fi
		    else
                        if [ -f /etc/gentoo-release ]; then
                            DISTNAME="Gentoo"
                            DISTVERS=`cat /etc/gentoo-release | awk '{ print $5 }'`
                        else
		    	    DISTNAME="Unknown"
			    log "!" "unknown distribution, collecting data"
			    log "-" "`uname -a`"
			    log "-" "`ls -la /etc`"
			    echo "Failed to install bootscripts, unknown Linux distribution."
			    echo "Please mail the file `pwd`/script/installinit.log to mbroek@users.sourceforge.net"
			    echo "or send it as file attach to Michiel Broek at 2:280/2802@Fidonet."
			    echo "Add information about the distribution you use in the message."
			    exit 1;
			fi
		    fi
	    	fi
	    fi
    	fi
    fi
fi
if [ "$OSTYPE" = "FreeBSD" ]; then
    DISTNAME="FreeBSD"
    DISTVERS=`uname -r`
    PW="pw "
fi
if [ "$OSTYPE" = "NetBSD" ]; then
    DISTNAME="NetBSD"
    DISTVERS=`uname -r`
fi


log "+" "Distribution $OSTYPE $DISTNAME $DISTVERS"


#--------------------------------------------------------------------------
#
#  Adding scripts for SuSE, from 7.1 and later in /etc/init.d
#
if [ "$DISTNAME" = "SuSE" ]; then
    if [ "$DISTVERS" '>' "7.1" ]; then
	DISTDIR="/etc"
    else
	DISTDIR="/sbin"
    fi
    DISTINIT="$DISTDIR/init.d/mbsed"
    echo "Installing SystemV init scripts for SuSE $DISTVERS"
    log "+" "Installing SystemV init scripts for SuSE $DISTVERS"
    echo "Adding $DISTINIT"
    cp init.SuSE $DISTINIT
    chmod 755 $DISTINIT
    echo "Making links for start/stop in runlevel 2"
    ln -s ../mbsed $DISTDIR/init.d/rc2.d/K05mbsed
    ln -s ../mbsed $DISTDIR/init.d/rc2.d/S99mbsed
    echo "Making links for start/stop in runlevel 3"
    ln -s ../mbsed $DISTDIR/init.d/rc3.d/K05mbsed
    ln -s ../mbsed $DISTDIR/init.d/rc3.d/S99mbsed
    echo "Making links for start/stop in runlevel 5"
    ln -s ../mbsed $DISTDIR/init.d/rc5.d/K05mbsed
    ln -s ../mbsed $DISTDIR/init.d/rc5.d/S99mbsed
    echo "SuSE $DISTVERS SystemV init configured"
    log "+" "SuSE $DISTVERS SystemV init configured"
fi



#--------------------------------------------------------------------------
#
#  Adding scripts for Slackware
#
if [ "$DISTNAME" = "Slackware" ]; then
    mkdir -p /etc/rc.d/init.d
    DISTINIT="/etc/rc.d/init.d/mbsed"
    echo "Adding SystemV Slackware $DISTVERS MBSE BBS start/stop scripts"
    log "+" "Adding SystemV Slackware $DISTVERS MBSE BBS start/stop scripts"
    cp init.Slackware $DISTINIT
    chmod 755 $DISTINIT
    if [ -f ${MBSE_ROOT}/bin/mbse.start ]; then
        echo "Removing old startup scripts"
        rm ${MBSE_ROOT}/bin/mbse.start ${MBSE_ROOT}/bin/mbse.stop ${MBSE_ROOT}/etc/rc ${MBSE_ROOT}/etc/rc.shutdown
    fi
    if [ -d /var/log/setup ]; then
	cp setup.mbse /var/log/setup
	chmod 755 /var/log/setup/setup.mbse
	echo "Added setup script, as root use 'pkgtool' Setup to enable MBSE at boot"
	log "+" "Added Slackware setup script for use with pkgtool"
    else
	echo "Making links for start/stop in runlevel 3"
	mkdir -p /etc/rc.d/rc3.d /etc/rc.d/rc4.d
	if [ -f /etc/rc.d/rc3.d/K05mbsed ]; then
	    rm /etc/rc.d/rc3.d/K05mbsed
	fi
	ln -s ../init.d/mbsed /etc/rc.d/rc3.d/K05mbsed
	if [ -f /etc/rc.d/rc3.d/S95mbsed ]; then
	    rm /etc/rc.d/rc3.d/S95mbsed
	fi
	ln -s ../init.d/mbsed /etc/rc.d/rc3.d/S95mbsed
	echo "Making links for start/stop in runlevel 4"
	if [ -f /etc/rc.d/rc4.d/K05mbsed ]; then
	    rm /etc/rc.d/rc4.d/K05mbsed
	fi
	ln -s ../init.d/mbsed /etc/rc.d/rc4.d/K05mbsed
	if [ -f /etc/rc.d/rc4.d/S95mbsed ]; then
	    rm /etc/rc.d/rc4.d/S95mbsed
	fi
	ln -s ../init.d/mbsed /etc/rc.d/rc4.d/S95mbsed
	echo "Slackware SystemV init configured"
	log "+" "Slackware SystemV init configured"
    fi
fi



#--------------------------------------------------------------------------
#
#  Adding scripts for RedHat, e-smith and Mandrake
#
if [ "$DISTNAME" = "RedHat" ] || [ "$DISTNAME" = "Mandrake" ]; then

    log "+" "Adding RedHat/E-Smith/Mandrake SystemV init scripts"
    DISTINIT="/etc/rc.d/init.d/mbsed"
    #
    # Extra tests are added for the RedHat e-smith server distribution,
    # this is a special distribution based on RedHat.
    #
    if [ -f /etc/mandrake-release ]; then
	RHN="Mandrake"
    else
    	if [ -f /etc/redhat-release ]; then
	    if [ -z "`grep e-smith /etc/redhat-release`" ]; then
	    	RHN="RedHat"
	    else
	    	RHN="e-smith based on RedHat"
	    fi
	else
	    echo "You are in big trouble."
	fi
    fi
    echo "Adding startup file $DISTINIT"
    cp init.RedHat $DISTINIT
    chmod 755 $DISTINIT
    echo "Making links for stop in runlevels 0 and 6"
    if [ -f /etc/rc.d/rc0.d/K05mbsed ]; then
	rm /etc/rc.d/rc0.d/K05mbsed
    fi  
    ln -s ../init.d/mbsed /etc/rc.d/rc0.d/K05mbsed
    if [ -f /etc/rc.d/rc6.d/K05mbsed ]; then
	rm /etc/rc.d/rc6.d/K05mbsed
    fi
    ln -s ../init.d/mbsed /etc/rc.d/rc6.d/K05mbsed
    echo "Making links for start in runlevels 3 and 5"
    if [ -f /etc/rc.d/rc3.d/S95mbsed ]; then
	rm /etc/rc.d/rc3.d/S95mbsed
    fi  
    ln -s ../init.d/mbsed /etc/rc.d/rc3.d/S95mbsed
    if [ -f /etc/rc.d/rc5.d/S95mbsed ]; then
	rm /etc/rc.d/rc5.d/S95mbsed
    fi
    ln -s ../init.d/mbsed /etc/rc.d/rc5.d/S95mbsed
    if [ "$RHN" = "e-smith based on RedHat" ]; then
	echo "Making link for start in runlevel 7"
	if [ -f /etc/rc.d/rc7.d/S95mbsed ]; then
	    rm /etc/rc.d/rc7.d/S95mbsed
	fi
	ln -s ../init.d/mbsed /etc/rc.d/rc7.d/S95mbsed
    fi
fi



#--------------------------------------------------------------------------
#
#  Adding scripts for Debian
#
#
if [ "$DISTNAME" = "Debian" ]; then
	echo "You are running Debian Linux $DISTVERS"
        log "+" "Adding Debian SystemV init script"
	DISTINIT="/etc/init.d/mbsebbs"
	cp init.Debian $DISTINIT
	chmod 755 $DISTINIT
	update-rc.d mbsebbs defaults
	echo "Debian install ready."
        log "+" "Debian SystemV init script installed"
fi


#-------------------------------------------------------------------------
#
#  Adding scripts for Gentoo
#
#
if [ "$DISTNAME" = "Gentoo" ]; then
       echo "You are running Gentoo Linux $DISTVERS"
       log "+" "Adding Gentoo init script"
       DISTINIT="/etc/init.d/mbsebbs"
       cp init.Gentoo $DISTINIT
       chmod 755 $DISTINIT
       rc-update add mbsebbs default
       echo "Gentoo install ready."
       log "+" "Gentoo init script installed"
fi


#--------------------------------------------------------------------------
#
#  Adding scripts for NetBSD
#
#
if [ "$DISTNAME" = "NetBSD" ]; then
    #
    # NetBSD init
    #
    DISTINIT="/etc/rc.d/mbsebbs"
    echo "Adding $DISTNAME style MBSE BBS start/stop script"
    log "+" "Adding $DISTNAME style MBSE BBS start/stop script"
    cp init.NetBSD $DISTINIT
    chmod 0755 $DISTINIT
fi


#--------------------------------------------------------------------------
#
#  Adding scripts for FreeBSD
#
#
if [ "$DISTNAME" = "FreeBSD" ]; then
    #
    # FreeBSD init
    # 
    DISTINIT="/usr/local/etc/rc.d/mbse.sh"
    echo "Adding $DISTNAME style MBSE BBS start/stop script"
    log "+" "Adding $DISTNAME style MBSE BBS start/stop script"
    cp init.FreeBSD $DISTINIT
    chmod 0755 $DISTINIT
fi



echo
echo "Please note, your MBSE BBS startup file is \"$DISTINIT\""
echo 
