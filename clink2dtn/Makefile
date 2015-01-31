CC=g++
INCLUDES=../common
CFLAGS=-std=c++11 -g -pthread -I $(INCLUDES) -I ../socketlink
OBJ=clink2dtnFileHandler.o clink2dtnUtils.o linkprog.o failure_recovery.o ProcessManager.o

%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS) 

all: $(OBJ)
	
clean: 
	rm -rf *.o
