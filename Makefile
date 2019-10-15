CC	?= cc
CFLAGS	 = `pkg-config --cflags libcurl` -g -D_GNU_SOURCE -D_DEFAULT_SOURCE -Icompat
LDFLAGS	 = `pkg-config --libs   libcurl`

.PHONY: all clean

all: crest

clean:
	rm -f *.o crest

OBJS	 = main.o repl.o io.o parse.o http.o svec.o
COMPAT	 = imsg.o imsg-buffer.o freezero.o recallocarray.o

${OBJS}: crest.h

crest: ${OBJS} ${COMPAT}
	${CC} ${OBJS} ${COMPAT} -o $@ ${LDFLAGS}

.SUFFIXES: .c .o
.c.o:
	${CC} ${CFLAGS} -c $< -o $@

# compat stuff:
imsg.o: compat/imsg.c
	${CC} ${CFLAGS} -c $< -o $@

imsg-buffer.o: compat/imsg-buffer.c
	${CC} ${CFLAGS} -c $< -o $@

freezero.o: compat/freezero.c
	${CC} ${CFLAGS} -c $< -o $@

recallocarray.o: compat/recallocarray.c
	${CC} ${CFLAGS} -c $< -o $@
