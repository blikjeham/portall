SHELL = /bin/sh

.SUFFIXES:
.SUFFIXES: .c .o

DEBUG = -ggdb
CFLAGS = -Wall -O2 $(DEBUG)

INCLUDES += -I/usr/local/include
#LDFLAGS += -L/usr/local/lib -lpbuffer

COMPILE = $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) $(CFLAGS)
LINK = $(CC) $(CFLAGS)

INSTALL = /usr/bin/install
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA = $(INSTALL) -m 644

DEPS = list.h
DEPS += channels.h
DEPS += conf.h

OBJ = list.o
OBJ += channels.o
OBJ += conf.o

MCOBJ = main.o $(OBJ)

all: multicat

%.o: %.c $(DEPS)
	$(COMPILE) -c -o $@ $<

multicat: $(MCOBJ)
	$(LINK) -o $@ $^ $(LDFLAGS)

clean:
	@rm -v -f *.o *~ multicat

.PHONY: clean
