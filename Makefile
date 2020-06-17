CC = gcc
CFLAGS = -g -Wall
CFILES = blkhistd.c db.c udev.c util.c

blkhistd:
	$(CC) $(CFLAGS) $(CFILES) -o blkhistd -ludev -lpq -ldevmapper

all: default

clean:
	rm -f *.o blkhistd *.db
