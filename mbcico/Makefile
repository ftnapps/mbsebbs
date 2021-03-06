# Makefile for mbcico.
# Copyright (c) 1998, 2001 by M. Broek.
# $Id: Makefile,v 1.46 2007/05/27 12:50:47 mbse Exp $

include ../Makefile.global

SRCS		= zmmisc.c zmrle.c zmrecv.c zmsend.c binkp.c md5b.c \
		  xmsend.c xmrecv.c m7recv.c m7send.c hydra.c inbound.c \
		  answer.c chat.c dial.c dietifna.c emsidat.c filelist.c \
		  openfile.c openport.c opentcp.c rdoptions.c yoohoo.c \
		  recvbark.c respfreq.c sendbark.c tcp.c tcpproto.c wazoo.c \
		  filetime.c ftsc.c atoul.c portsel.c telnet.c bopenfile.c \
		  ttyio.c lutil.c scanout.c emsi.c ulock.c dirlock.c \
		  callstat.c session.c call.c htoul.c mbcico.c \
		  outstat.c nlinfo.c mbout.c
HDRS		= zmodem.h binkp.h config.h statetbl.h md5b.h \
		  xmsend.h xmrecv.h m7recv.h m7send.h hydra.h inbound.h \
		  answer.h chat.h dial.h dietifna.h emsidat.h filelist.h \
		  openfile.h openport.h opentcp.h rdoptions.h yoohoo.h \
		  recvbark.h respfreq.h sendbark.h tcp.h tcpproto.h wazoo.h \
		  filetime.h ftsc.h atoul.h portsel.h telnet.h bopenfile.h \
		  ttyio.h lutil.h scanout.h emsi.h ulock.h dirlock.h \
		  callstat.h session.h call.h htoul.h mbcico.h \
		  outstat.h nlinfo.h
MBCICO_OBJS	= zmmisc.o zmrle.o zmrecv.o zmsend.o binkp.o md5b.o \
		  xmsend.o xmrecv.o m7recv.o m7send.o hydra.o inbound.o \
		  answer.o chat.o dial.o dietifna.o emsidat.o filelist.o \
		  openfile.o openport.o opentcp.o rdoptions.o yoohoo.o \
		  recvbark.o respfreq.o sendbark.o tcp.o tcpproto.o wazoo.o \
		  filetime.o ftsc.o atoul.o portsel.o telnet.o bopenfile.o \
		  ttyio.o lutil.o scanout.o emsi.o ulock.o dirlock.o \
		  callstat.o session.o call.o htoul.o mbcico.o
MBOUT_OBJS	= outstat.o nlinfo.o mbout.o scanout.o callstat.o
SLIBS		= ../lib/libmbse.a ../lib/libmsgbase.a ../lib/libdbase.a ../lib/libnodelist.a
OTHER		= Makefile README
TARGET		= mbcico mbout

#############################################################################

.c.o:
		${CC} ${CFLAGS} ${INCLUDES} ${DEFINES} -c $<

all:		${TARGET}

clean:
		rm -f ${TARGET} *.o *.h~ *.c~ core filelist Makefile.bak

install:	all
		${INSTALL} -c -s -g ${GROUP} -o ${OWNER} -m 4750 mbcico    ${BINDIR}
		${INSTALL} -c -s -g ${GROUP} -o ${OWNER} -m 0750 mbout     ${BINDIR}
		@rm -f ${BINDIR}/mbtelind
		@rm -f ${BINDIR}/mbtelout

mbcico:		${MBCICO_OBJS} ${SLIBS}
		${CC} -o mbcico ${MBCICO_OBJS} ${LDFLAGS} ${LIBS} ${SLIBS}

mbout:		${MBOUT_OBJS} ${SLIBS}
		${CC} -o mbout ${MBOUT_OBJS} ${LDFLAGS} ${LIBS} ${SLIBS}

filelist:	Makefile
		BASE=`pwd`; \
		BASE=`basename $${BASE}`; \
		(for f in ${SRCS} ${HDRS} ${OTHER} ;do echo ${PACKAGE}-${VERSION}/$${BASE}/$$f; done) >filelist

depend:
	@rm -f Makefile.bak; \
	mv Makefile Makefile.bak; \
	sed -e '/^# DO NOT DELETE/,$$d' Makefile.bak >Makefile; \
	${ECHO} '# DO NOT DELETE THIS LINE - MAKE DEPEND RELIES ON IT' \
		>>Makefile; \
	${ECHO} '# Dependencies generated by make depend' >>Makefile; \
	for f in ${SRCS}; \
	do \
		${ECHO} "Dependencies for $$f:\c"; \
		${ECHO} "`basename $$f .c`.o:\c" >>Makefile; \
		for h in `sed -n -e \
			's/^#[ 	]*include[ 	]*"\([^"]*\)".*/\1/p' $$f`; \
		do \
			${ECHO} " $$h\c"; \
			${ECHO} " $$h\c" >>Makefile; \
		done; \
		${ECHO} " done."; \
		${ECHO} "" >>Makefile; \
	done; \
	${ECHO} '# End of generated dependencies' >>Makefile

# DO NOT DELETE THIS LINE - MAKE DEPEND RELIES ON IT
# Dependencies generated by make depend
zmmisc.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ttyio.h session.h zmodem.h
zmrle.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ttyio.h session.h zmodem.h
zmrecv.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h lutil.h ttyio.h session.h zmodem.h config.h emsi.h openfile.h filelist.h openport.h
zmsend.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ttyio.h session.h zmodem.h lutil.h emsi.h filelist.h
binkp.o: ../config.h ../lib/mbselib.h ../lib/users.h ../lib/nodelist.h ../lib/mbsedb.h ttyio.h session.h statetbl.h config.h emsi.h bopenfile.h respfreq.h filelist.h opentcp.h rdoptions.h lutil.h binkp.h config.h md5b.h inbound.h callstat.h mbcico.h
md5b.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h lutil.h md5b.h
xmsend.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h session.h ttyio.h statetbl.h xmsend.h m7send.h filelist.h filetime.h
xmrecv.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h session.h ttyio.h statetbl.h config.h lutil.h openfile.h m7recv.h xmrecv.h filelist.h filetime.h
m7recv.o: ../config.h ../lib/mbselib.h statetbl.h ttyio.h m7recv.h
m7send.o: ../config.h ../lib/mbselib.h statetbl.h ttyio.h m7send.h
hydra.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h session.h filelist.h filetime.h ttyio.h statetbl.h config.h emsi.h openfile.h lutil.h respfreq.h mbcico.h hydra.h
inbound.o: ../config.h ../lib/mbselib.h dirlock.h inbound.h
answer.o: ../config.h ../lib/mbselib.h ../lib/users.h ../lib/nodelist.h ../lib/mbsedb.h lutil.h session.h config.h answer.h openport.h portsel.h dial.h rdoptions.h mbcico.h
chat.o: ../config.h ../lib/mbselib.h config.h chat.h ttyio.h
dial.o: ../config.h ../lib/mbselib.h ../lib/users.h ../lib/nodelist.h ../lib/mbsedb.h config.h chat.h ttyio.h session.h dial.h
dietifna.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ttyio.h session.h emsi.h dietifna.h respfreq.h filelist.h xmrecv.h xmsend.h
emsidat.o: ../config.h ../lib/mbselib.h ../lib/users.h ../lib/nodelist.h ../lib/mbsedb.h emsi.h session.h lutil.h config.h emsidat.h filetime.h
filelist.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ../lib/users.h ../lib/mbsedb.h config.h session.h filelist.h
openfile.o: ../config.h ../lib/mbselib.h config.h lutil.h openfile.h
openport.o: ../config.h ../lib/mbselib.h ulock.h ttyio.h mbcico.h openport.h
opentcp.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ../lib/users.h ../lib/mbsedb.h session.h ttyio.h openport.h opentcp.h
rdoptions.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ../lib/users.h ../lib/mbsedb.h session.h config.h
yoohoo.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ../lib/users.h ../lib/mbsedb.h statetbl.h ttyio.h session.h config.h emsi.h hydra.h rdoptions.h wazoo.h dietifna.h yoohoo.h inbound.h callstat.h
recvbark.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ttyio.h session.h statetbl.h recvbark.h respfreq.h filelist.h
respfreq.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ../lib/msg.h ../lib/users.h ../lib/mbsedb.h session.h lutil.h config.h atoul.h respfreq.h filelist.h
sendbark.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ttyio.h session.h statetbl.h sendbark.h xmrecv.h
tcp.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ttyio.h session.h statetbl.h config.h emsi.h respfreq.h filelist.h tcpproto.h tcp.h
tcpproto.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ttyio.h session.h config.h emsi.h lutil.h openfile.h filelist.h tcpproto.h
wazoo.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ttyio.h session.h statetbl.h config.h emsi.h respfreq.h filelist.h wazoo.h zmodem.h
filetime.o: ../config.h ../lib/mbselib.h filetime.h
ftsc.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h session.h ttyio.h statetbl.h config.h ftsc.h rdoptions.h recvbark.h filelist.h sendbark.h respfreq.h xmrecv.h xmsend.h inbound.h
atoul.o: ../config.h ../lib/mbselib.h atoul.h
portsel.o: ../config.h ../lib/mbselib.h portsel.h
telnet.o: ../config.h ../lib/mbselib.h telnet.h
bopenfile.o: ../config.h ../lib/mbselib.h config.h lutil.h bopenfile.h
ttyio.o: ../config.h ../lib/mbselib.h ttyio.h lutil.h
lutil.o: ../config.h ../lib/mbselib.h lutil.h
scanout.o: ../config.h ../lib/mbselib.h ../lib/users.h ../lib/mbsedb.h config.h scanout.h lutil.h
emsi.o: ../config.h ../lib/mbselib.h ../lib/users.h ../lib/nodelist.h ../lib/mbsedb.h ttyio.h session.h statetbl.h config.h emsi.h emsidat.h hydra.h rdoptions.h tcp.h wazoo.h inbound.h callstat.h
ulock.o: ../config.h ../lib/mbselib.h
dirlock.o: ../config.h ../lib/mbselib.h dirlock.h
callstat.o: ../config.h ../lib/mbselib.h callstat.h
session.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ttyio.h statetbl.h emsi.h ftsc.h session.h yoohoo.h mbcico.h binkp.h callstat.h inbound.h opentcp.h telnet.h
call.o: ../config.h ../lib/mbselib.h ../lib/users.h ../lib/nodelist.h ../lib/mbsedb.h session.h callstat.h call.h config.h dial.h lutil.h portsel.h openport.h opentcp.h rdoptions.h inbound.h
htoul.o: ../config.h ../lib/mbselib.h htoul.h
mbcico.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ../lib/users.h ../lib/mbsedb.h config.h answer.h call.h lutil.h mbcico.h session.h binkp.h
outstat.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ../lib/users.h ../lib/mbsedb.h scanout.h callstat.h outstat.h
nlinfo.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h nlinfo.h
mbout.o: ../config.h ../lib/mbselib.h ../lib/nodelist.h ../lib/users.h ../lib/mbsedb.h outstat.h nlinfo.h
# End of generated dependencies
