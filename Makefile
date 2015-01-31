CC=g++
CFLAGS=-std=c++11 -g -pthread -lrt
BOOSTFLAGS=-lboost_system -lboost_filesystem

COMMONDIR=common
USERDIR=userdaemon
CLIENTDIR=userprog
ARRIVALDIR=kioskprog
SERVERDIR=kioskdaemon
PROXYDIR=fileproxy
LINKDIR=clink2dtn
SOCKDIR=socketlink

COMMONOBJ=$(COMMONDIR)/sendall.o $(COMMONDIR)/lookup3.o
USEROBJ=$(USERDIR)/userserver.o $(USERDIR)/remotehandle.o
CLIENTOBJ=$(CLIENTDIR)/clientprog.o $(CLIENTDIR)/pageretreaval.o 
ARRIVALOBJ=$(ARRIVALDIR)/kioskfile.o
PROXYOBJ=$(PROXYDIR)/cityside2.o $(PROXYDIR)/http.o
SERVEROBJ=$(SERVERDIR)/serverprog.o $(SERVERDIR)/handle.o
LINKOBJ=$(LINKDIR)/clink2dtnFileHandler.o $(LINKDIR)/clink2dtnUtils.o $(LINKDIR)/failure_recovery.o $(LINKDIR)/ProcessManager.o
LINKMAINOBJ=$(LINKDIR)/linkprog.o
SOCKETLINK=$(SOCKDIR)/socket_link2.o $(SOCKDIR)/socket_utils2.o

all: 
	cd $(COMMONDIR); make
	cd $(SOCKDIR); make
	cd $(LINKDIR); make
	cd $(PROXYDIR); make
	cd $(SERVERDIR); make
	cd $(CLIENTDIR); make
	cd $(USERDIR); make
	cd $(ARRIVALDIR); make
	$(CC) $(SERVEROBJ) $(COMMONOBJ) -o server $(CFLAGS)
	$(CC) $(CLIENTOBJ) $(COMMONOBJ) -o client $(CFLAGS)	
	$(CC) $(USEROBJ) $(COMMONOBJ) -o user $(CFLAGS)
	$(CC) $(PROXYOBJ) $(COMMONOBJ) $(SOCKETLINK) $(LINKOBJ) -o proxy $(CFLAGS) $(BOOSTFLAGS)
	$(CC) $(ARRIVALOBJ) $(COMMONOBJ) -o arrival $(CFLAGS)
	$(CC) $(LINKMAINOBJ) $(LINKOBJ) $(COMMONOBJ) $(SOCKETLINK) -o dtnconnect $(CFLAGS) $(BOOSTFLAGS)

clean:
	cd $(COMMONDIR); rm -rf *.o
	cd $(SOCKDIR); rm -rf *.o
	cd $(LINKDIR); rm -rf *.o
	cd $(PROXYDIR); rm -rf *.o
	cd $(SERVERDIR); rm -rf *.o
	cd $(CLIENTDIR); rm -rf *.o
	cd $(USERDIR); rm -rf *.o
	cd $(ARRIVALDIR); rm -rf *.o
	rm -rf client server user proxy arrival dtnconnect
