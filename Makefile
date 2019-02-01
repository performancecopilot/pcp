CC=gcc 
CFLAGS=-Wall -Wextra

all: main
main: main.o
main.o: main.c

clean: 
	rm -f main main.o
run: main
	./main