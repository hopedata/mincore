
CC=gcc
CFLAGS=-Wall -Wpedantic
LIBS=
DEPS = 
OBJ = mincore.o

BINDIR=/usr/bin

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

mincore: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

install: mincore
	mkdir -p ${DESTDIR}${BINDIR}
	cp mincore ${DESTDIR}${BINDIR}

.PHONY: clean
clean:
	rm -f *.o *~ core mincore 
