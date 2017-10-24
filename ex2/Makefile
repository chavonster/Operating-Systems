CC=g++
RANLIB=ranlib

LIBSRC=uthreads.cpp
LIBOBJ=$(LIBSRC:.cpp=.o)

INCS=-I.
CFLAGS = -Wall -g $(INCS)
LOADLIBES = -L./ 

UTHREADLIB = libuthreads.a
TARGETS = $(UTHREADLIB)

TAR=tar
TARFLAGS = -cvf
TARNAME = ex2.tar
TARSRCS = $(LIBSRC) Makefile README

all: $(TARGETS) 

$(TARGETS): $(LIBOBJ)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

clean:
	$(RM) $(TARGETS) $(UTHREADLIB) $(OBJ) $(LIBOBJ) *~ *core $(TARNAME)

depend:
	makedepend -- $(CFLAGS) -- $(SRC) $(LIBSRC)

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)
