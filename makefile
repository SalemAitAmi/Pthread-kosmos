# For UVic CSC 360, Summer 2022
# Assignment #2

CC=gcc
CFLAGS=-g -D_REENTRANT -Wall -Werror -std=c18  # Note: This version disables noisy VERBOSE messages.
#CFLAGS=-g -D_REENTRANT -DVERBOSE -Wall -Werror -std=c18 # Note: This version ENABLE noisy VERBOSE messages.
HEADERS=logging.h
OBJECT=logging.o kosmos-mcv.o
LIBS=-lpthread

all: kosmos-mcv

kosmos-mcv: $(OBJECT)
	$(CC) -o kosmos-mcv $(OBJECT) $(LIBS) $(CFLAGS)

kosmos-mcv.o: kosmos-mcv.c
	$(CC) $(CFLAGS) -c kosmos-mcv.c

logging.o: logging.c logging.h
	$(CC) $(CFLAGS) -c logging.c

clean:
	-rm -f $(OBJECT) kosmos-mcv
