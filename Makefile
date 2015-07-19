CCFLAGS+=-std=c99 -g -O0
LDFLAGS+=-lcurses
CC=gcc
LD=gcc
AR=ar rcu
RANLIB=ranlib

all:		vuitest

clean:
		rm -vf *.o libvui.a vuitest

vuitest:	vuitest.o libvui.a
		$(LD) $(LDFLAGS) $^ -o $@

libvui.a:	vui.o statemachine.o
		$(AR) $@ $^
		$(RANLIB) $@

vuitest.o:	vui.h

vui.o:		vui.h statemachine.h

statemachine.o:	statemachine.h

%.o:		%.c
		$(CC) $(CCFLAGS) -c $< -o $@

.PHONY:		all clean
