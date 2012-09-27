CC = gcc
NCLUDEDIR=/usr/include/collectd/ ${EXTRA_INCLUDE}
COPTS = -O2 -Wall -fPIC -DPIC -DFP_LAYOUT_NEED_NOTHING -pthread -I . -I${INCLUDE_DIR} -DVERSION=${VERSION}

VERSION != uname -r | sed -e 's/\.//g'

GENERATION = new

all: pf.so

test: pfcmd pfrulescmd

clean:
	rm -f *.o *.so pfcmd pfrulescmd pfutils.h

pf.so: pf.c
	${CC} -shared ${COPTS} -o pf.so pf.c

pfrules.so: pfutils.h pfrules.c pfutils-${GENERATION}.c
	${CC} ${COPTS} -c pfrules.c
	${CC} ${COPTS} -o pfutils.o -c pfutils-${GENERATION}.c
	${CC} -shared ${COPTS} -o pfrules.so pfrules.o pfutils.o

pfcmd: pf.c
	${CC} -DTEST ${COPTS} -o pfcmd pf.c

pfrulescmd: pfutils.h pfrules.c pfutils-${GENERATION}.c
	${CC} -DTEST ${COPTS} -c pfrules.c
	${CC} -DTEST ${COPTS} -o pfutils.o -c pfutils-${GENERATION}.c
	${CC} -o pfrulescmd pfrules.o pfutils.o

pfutils.h: pfutils-${GENERATION}.h
	cp pfutils-${GENERATION}.h pfutils.h
