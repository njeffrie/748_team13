CC=gcc
CFLAGS=-I. -I../common/ -ljansson 

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

all: main.o ../common/sparkmac_pkt.o
	$(CC) -o SLIPstreamJ main.o ../common/sparkmac_pkt.o -I. -ljansson

clean: 
	rm -f *.o *~ core SLIPstreamJ ../common/sparkmac_pkt.o

