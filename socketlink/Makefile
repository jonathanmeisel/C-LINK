CC=g++
INCLUDES=../common
CFLAGS=-std=c++11 -g -pthread -I $(INCLUDES)
OBJ=socket_link2.o socket_utils2.o

%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS) 

all: $(OBJ)
	
clean: 
	rm -rf *.o
