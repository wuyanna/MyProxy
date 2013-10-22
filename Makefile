CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: proxy

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

proxy.o: proxy.c lrucache.h csapp.h
	$(CC) $(CFLAGS) -c proxy.c

lrucache.o: lrucache.c lrucache.h csapp.h
	$(CC) $(CFLAGS) -c lrucache.c


proxy: proxy.o lrucache.o csapp.o

submit:
	(make clean; cd ..; tar cvf proxylab.tar proxylab-handout)

clean:
	rm -f *~ *.o proxy core

