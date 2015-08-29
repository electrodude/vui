CCFLAGS+=-std=c99 -g -O0
LDFLAGS+=-lcurses
CC=gcc
LD=gcc
AR=ar rcu
RANLIB=ranlib

all:		vuitest

clean:
		rm -vf *.o libvui.a vuitest

vuitest:	vuitest.o graphviz.o libvui.a
		$(LD) $^ $(LDFLAGS) -o $@

libvui.a:	vui.o statemachine.o stack.o string.o utf8.o
		$(AR) $@ $^
		$(RANLIB) $@

%.o:		%.c
		$(CC) $(CCFLAGS) -c $< -o $@

depend:		
		$(CC) $(CCFLAGS) -MM {vuitest,graphviz,vui,statemachine,stack,string,utf8}.c

.PHONY:		all clean

# output of `make depend`

vuitest.o: vuitest.c vui.h string.h statemachine.h stack.h graphviz.h
graphviz.o: graphviz.c graphviz.h statemachine.h stack.h
vui.o: vui.c utf8.h vui.h string.h statemachine.h stack.h
statemachine.o: statemachine.c utf8.h statemachine.h stack.h
stack.o: stack.c stack.h
string.o: string.c utf8.h string.h
utf8.o: utf8.c utf8.h
