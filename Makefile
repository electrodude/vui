CCFLAGS+=-Wall -g -O2
LDFLAGS+=-lcurses
CC=gcc -std=c99
LD=gcc
AR=ar rcu
RANLIB=ranlib

all:		vuitest

clean:
		rm -vf vui*.o libvui.a vuitest

vuitest:	vuitest.o libvui.a
		${LD} $^ ${LDFLAGS} -o $@

libvui.a:	vui.o vui_fragment.o vui_combinator.o vui_translator.o vui_statemachine.o vui_gc.o vui_stack.o vui_string.o vui_utf8.o vui_graphviz.o
		${AR} $@ $^
		${RANLIB} $@

%.o:		%.c
		${CC} ${CCFLAGS} -c $< -o $@

depend:		
		${CC} ${CCFLAGS} -MM vui{test,,_fragment,_combinator,_translator,_statemachine,_gc,_stack,_string,_utf8,_graphviz}.c

.PHONY:		all depend clean

# output of `make depend`

vuitest.o: vuitest.c vui_debug.h vui.h vui_string.h vui_translator.h \
 vui_stack.h vui_statemachine.h vui_fragment.h vui_combinator.h vui_gc.h \
 vui_graphviz.h
vui.o: vui.c vui_debug.h vui_utf8.h vui_gc.h vui_stack.h \
 vui_statemachine.h vui.h vui_string.h vui_translator.h vui_fragment.h
vui_fragment.o: vui_fragment.c vui_fragment.h vui_stack.h \
 vui_statemachine.h
vui_combinator.o: vui_combinator.c vui_statemachine.h vui_stack.h \
 vui_combinator.h vui_fragment.h
vui_translator.o: vui_translator.c vui_utf8.h vui.h vui_string.h \
 vui_translator.h vui_stack.h vui_statemachine.h vui_fragment.h
vui_statemachine.o: vui_statemachine.c vui_debug.h vui_utf8.h \
 vui_string.h vui_gc.h vui_stack.h vui_statemachine.h
vui_gc.o: vui_gc.c vui_debug.h vui_gc.h vui_stack.h vui_statemachine.h
vui_stack.o: vui_stack.c vui_debug.h vui_stack.h
vui_string.o: vui_string.c vui_utf8.h vui_debug.h vui_string.h
vui_utf8.o: vui_utf8.c vui_debug.h vui_utf8.h
vui_graphviz.o: vui_graphviz.c vui_gc.h vui_stack.h vui_statemachine.h \
 vui_graphviz.h
