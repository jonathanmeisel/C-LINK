CC=g++
INCLUDES=../common
CFLAGS=-std=c++11 -g -pthread -I $(INCLUDES)
OBJ=userserver.o remotehandle.o

%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS) 

all: $(OBJ)
	
clean: 
	rm -rf *.o
