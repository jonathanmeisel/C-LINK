CC=g++
INCLUDES=-I ../common -I ../clink2dtn -I ../socketlink
CFLAGS=-std=c++11 -g -pthread $(INCLUDES)
OBJ=cityside2.o http.o

%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS) 

all: $(OBJ)
	
clean: 
	rm -rf *.o
