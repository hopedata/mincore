
CC=gcc
CFLAGS=-Wall -Wpedantic
LIBS=
DEPS = 
OBJ = mincore.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

mincore: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o *~ core mincore 
