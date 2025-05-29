CC = gcc
CFLAGS = -Wall -g
BINARY = icsh
OBJS = icsh.o job.o kirby.o

all: $(BINARY)

$(BINARY): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -f $(BINARY) *.o
