/* this is the main file for the user daemon. It runs as root and handles
   requests for pages from the other machines in the village as well as
   incoming pages form the city
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <math.h>
#include <map>
#include <iostream>
#include <fstream>
#include <string.h>

#include "sendall.h"
#include "remotehandle.h"
#include "defines.h"
#include "userserver.h"



using namespace std;

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[])
{
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct sockaddr_in my_addr;    // my address information
    struct sockaddr_in their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    int numbytes;
    
    char buf[MAXDATASIZE];
    char sendbuf[MAXDATASIZE];
    stringtofullloc ipmap;
    fullloc temp;

    map <string,int> usersizes;
    stringtostring configuration;
    stringtostringqueue LRUfile;
    ifstream file;
    int len, mysize;

    string response,filename=DEFAULTUSERFILE;

    if (argc!=1){
      filename=argv[1];
    }
    
    file.open(filename.c_str());
    while(!file.fail()){
      file.getline(buf,sizeof(buf));
      response=buf;
      if((response[0]!='#') && (numbytes=response.find('='))!=string::npos){
	configuration[response.substr(0,numbytes)]=response.substr(numbytes+1);;
      }
    }

    file.close();

    checkcompleteness(&configuration);
    LRUfile=read_LRU_file(configuration);

    //    string filename=DEFAULTFILE; 

    /* set up a socket to listen for remote connections */

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }
    
    my_addr.sin_family = AF_INET;         // host byte order
    my_addr.sin_port = htons(FILEPORT);     // short, network byte order
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY); // automatically fill with my IP
    memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof my_addr) == -1) {
        perror("bind");
        exit(1);
    }
    
    if (listen(sockfd, BACKLOG) == -1) {
      perror("listen");
      exit(1);
    }
    
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
      perror("sigaction");
      exit(1);
    }
    
    while(1) {  // main accept() loop
      sin_size = sizeof their_addr;
      if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr,	\
			   &sin_size)) == -1) {
	perror("accept");
	continue;
      }

      usersizes=readusersizes(configuration, &LRUfile);
      
      //             printf("server: got connection from %s\n",	\
      //                  inet_ntoa(their_addr.sin_addr));
      if (!fork()) { // this is the child process
	close(sockfd); // child doesn't need the listener

	/* receive the first three bytes of the message to 
	   determine the type code */
	  len = 3;
	  mysize=len;
	  if (receiveall(new_fd, buf, &len, 0) == -1) {
	    perror("recv");
	    printf("We still have space to receive %d more bytes\n", len);
	    close(new_fd);
	    exit(0);
	  }
	  buf[mysize-len] = '\0';

	  if(!strcmp(buf,"201")){
	    /* a village machine has requested a local file */
	    handle_remote_request(new_fd, configuration, usersizes);
	  } else if(!strcmp(buf,"200")){
	    /* A file that we have requested is ready for delivery */
	    handle_new_file(new_fd, &usersizes, configuration, LRUfile );
	  } else if(!strcmp(buf,"301")){
	    /* Looking for the emptiest, send file utalization */
	    send_user_sizes(new_fd, usersizes, atoi(configuration["quota"].c_str()));
	  } else if(!strcmp(buf,"505")){
	    /*delete the specified file from the specified user*/
	    deletedupes(new_fd, usersizes, configuration);
	  }


	close(new_fd);
	exit(0);
      }
      close(new_fd);  // parent doesn't need this
    }
    
    return 0;
}

/*checkcompleteness - checks to see that all configuration fields are
  filled in, either with values read in of defaults

  input:
  filenames - map into which to put configuration values
*/
void checkcompleteness(stringtostring * filenames){
  
  if(filenames->find("kioskip")==filenames->end()){
    (*filenames)["kioskip"]=DEFAULTKIOSKIP;
  }

  if(filenames->find("quota")==filenames->end()){
    (*filenames)["quota"]=DEFAULTQUOTA;
  }

  if(filenames->find("free")==filenames->end()){
    (*filenames)["free"]=DEFAULTSPACETOFREE;
  }

  if(filenames->find("sizefile")==filenames->end()){
    (*filenames)["sizefile"]=DEFAULTSIZEFILE;
  }

  if(filenames->find("deletion")==filenames->end()){
    (*filenames)["deletion"]=DEFAULTUSERDELFILE;
  }
  
  if(filenames->find("used")==filenames->end()){
    (*filenames)["used"]=DEFAULTUSEDFILE;
  }
 
  if(filenames->find("LRU")==filenames->end()){
    (*filenames)["LRU"]=DEFAULTLRUFILE;
  }
  

}
