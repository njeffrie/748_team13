MIO_ROOT=/home/dermeister/workspace/mio
MIO_LIBRARY_PATH=/home/dermeister/workspace/mio/libs/c/libMIO

-include $(MIO_LIBRARY_PATH)/vars.mk
UNAME := $(shell uname)

CC=gcc
LIBS = -l$(MIO_LIB_NAME) -lstrophe -lexpat -lpthread -lm -lresolv -lssl
ifeq ($(UNAME), Linux)
	 LIBS += -luuid
endif
INCLUDE=-I. -I../common/ -I$(STROPHE_INCLUDE) -I$(STROPHE_INCLUDE_SRC) -I$(MIO_LIBRARY_PATH) -I../common/
CFLAGS= -g -Wall $(INCLUDE) -O0
LDFLAGS=-L. -L$(MIO_LIBRARY_PATH) -L$(STROPHE_LIB) $(LIBS)
DEFINES=-DUSE_MIO

all: slipclient.o slipstream.o ../common/pkt.o
	gcc -o mio_gateway slipclient.o slipstream.o ../common/pkt.o $(CFLAGS) $(LDFLAGS) $(DEFINES)
	
slipclient.o: slipclient.c
	gcc -c -o $@ $< $(CFLAGS) $(LDFLAGS) $(DEFINES)
	
pkt.o: pkt.c
	gcc -c -o $@ $< $(CFLAGS) $(LDFLAGS)
	
slipstream.o: ../common/slipstream.c
	gcc -c -o $@ $< $(CFLAGS) $(LDFLAGS)

clean: 
	rm -f ../common/*.o *.o *~ core mio_gateway
	
