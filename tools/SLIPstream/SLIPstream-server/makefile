CC=gcc
CFLAGS=-I. 

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

all: main.o
	$(CC) -o SLIPstream main.o -I. -lpthread
clean: 
	rm -f *.o *~ core SLIPstream

