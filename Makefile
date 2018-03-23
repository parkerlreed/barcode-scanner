CC      := gcc
CFLAGS  := -Wall -Wextra -O2
LDFLAGS := -lrt -lmpg123 -lao

.PHONY: all clean

all: clean example

clean:
	rm -f example *.o

%.o: %.c
	$(CC) $(CFLAGS) -c $^

example: example.o barcode.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o example
