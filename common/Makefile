CC=g++
INCLUDES=../common
CFLAGS=-std=c++11 -g -pthread -I $(INCLUDES)
OBJ=lookup3.o sendall.o

all: $(OBJ)

%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS) 

clean: 
	rm -rf *.o
