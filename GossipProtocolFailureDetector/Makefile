#**********************
#*
#* Progam Name: Gossip Style Membership Protocol.
#*
#* Current file: Makefile
#* About this file: Build Script.
#* 
#***********************

CFLAGS =  -Wall -g

all: app

app: node.o emulnet.o app.o log.o params.o queue.o
	gcc -o app node.o emulnet.o app.o log.o params.o queue.o

node.o: node.c node.h emulnet.h params.h requests.h queue.h stdincludes.h log.h MPtemplate.h nodeaddr.h 
	gcc -c node.c ${CFLAGS}

emulnet.o: emulnet.c emulnet.h nodeaddr.h log.h params.h stdincludes.h
	gcc -c emulnet.c ${CFLAGS}

app.o: app.c app.h nodeaddr.h node.h MPtemplate.h stdincludes.h params.h queue.h requests.h emulnet.h log.h
	gcc -c app.c ${CFLAGS}

log.o: log.c log.h nodeaddr.h stdincludes.h
	gcc -c log.c ${CFLAGS}

params.o: params.c params.h stdincludes.h
	gcc -c params.c ${CFLAGS}

queue.o: queue.c queue.h stdincludes.h
	gcc -c queue.c ${CFLAGS}

clean:
	rm -rf *.o app dbg.log msgcount.log stats.log
