CCFLAGS+=--std=c99 -g -O0
LDFLAGS+=
CC=gcc
LD=gcc
AR=ar rcu
RANLIB=ranlib

all:		libvui.a

clean:
		rm -vf *.o libvui.a

libvui.a:	vui.o statemachine.o
		$(AR) $@ $^
		$(RANLIB) $@

vui.o:		vui.h statemachine.h

statemachine.o:	statemachine.h

%.o:		%.c
		$(CC) $(CCFLAGS) -c $< -o $@

.PHONY:		all clean
