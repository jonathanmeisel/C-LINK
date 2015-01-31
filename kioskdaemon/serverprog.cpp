#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#include "sendall.h"
#include "handle.h"
#include "defines.h"
#include "serverprog.h"


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
    int numbytes, oldsize, content, newline, total, sizeofbuf;

    int post=0;
    
    char buf[MAXDATASIZE];
    char sendbuf[MAXDATASIZE];
    char usrbuf[MAXDATASIZE];
    string socketbuf;
    char *socketbuf2, *charpoint, *charpoint2;

    stringtofullloc *ipmap=new stringtofullloc;
    fullloc temp;
    stringtostring lastknown;
    stringtostring filenames;

    stringtostringqueue tempstore;

    string response,tempresponse;
    string requesttype;
    string filename=DEFAULTKIOSKFILE; 
    string requsr, user;

    char lenbyte;

    ifstream file;

    if (argc!=1){
      filename=argv[1];
    }
    
    file.open(filename.c_str());
    while(!file.fail()){
      file.getline(buf,sizeof(buf));
      response=buf;
      if((response[0]!='#') && (numbytes=response.find('='))!=string::npos){
	filenames[response.substr(0,numbytes)]=response.substr(numbytes+1);;
      }
    }

    checkcompleteness(&filenames);

    //*ipmap = readfileloc(filename);

    /* upon initialization, read in all outstanding files. This is a 
       safety feature to protect against power failures in the middle of
       execution
    */
    checkdeletion(ipmap, &lastknown, true, filenames, &tempstore);


    /* set up socket for connection and listen on it */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }
    
    my_addr.sin_family = AF_INET;         // host byte order
    my_addr.sin_port = htons(MYPORT);     // short, network byte order
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

      /* as soon as we have a connection, make sure that all files are
	 up to date. File locking will need to be added. */

      checkdeletion(ipmap, &lastknown, false, filenames, &tempstore);
      //                    printf("server: got connection from %s\n",	\
      //            inet_ntoa(their_addr.sin_addr));


      if (!fork()) { // this is the child process
	close(sockfd); // child doesn't need the listener


	/*read in the request type and the two following fields.
	  This is safe because each request to the kioskdaemon will have at
	  information on the pagename and the user */

	numbytes=3;
	oldsize=numbytes;
	if (receiveall(new_fd, buf, &numbytes, 0) == -1) {
	  perror("recv");
	  exit(1);
	}
	buf[oldsize-numbytes]='\0';

	requesttype=buf; //save the type (000,300,404,500)


	numbytes=sizeof(buf)-1;
	oldsize=numbytes;
	if (receiveall(new_fd, buf, &numbytes, 0) == -1) {
	  perror("recv");
	  exit(1);
	}
	
	buf[oldsize-numbytes]='\0'; //keep second field in buf, switch to usrbuf
	numbytes=sizeof(usrbuf)-1;
	oldsize=numbytes;
	if (receiveall(new_fd, usrbuf, &numbytes, 0) == -1) {
	  perror("recv");
	  exit(1);
	}

	usrbuf[oldsize-numbytes]='\0';
	requsr=usrbuf; // store third field (usually the username)


	/*if this is a 000 or 404 the data on the browser's socket is coming in
	  we need to grab that. If this is a 500, the field will be the returned
	  file. Further, since this is likely to be more then 256 characters,
	  we cannot use receive all
	  SIBREN: all of this is input specific and should be moved to the
	  appropriate function below
	*/
	if ((numbytes=recv(new_fd, usrbuf, sizeof(usrbuf), 0)) == -1) {
	  perror("recv");
	  exit(1);
	}
	if(numbytes < sizeof(usrbuf)){
	  usrbuf[numbytes]='\0';
	}

	//socketbuf[numbytes]='\0';
	//socketbuf=usrbuf;
	if(requesttype.compare("500")!=0 && requesttype.compare("300")!=0){
	  if(strstr(usrbuf,"POST ")!=NULL){
	    post=1;
	    //there might be more then just 1024 bytes in a post request
	    charpoint=strstr(usrbuf,"Content-Length: ");
	    content= atoi(charpoint+16);
	    charpoint=strstr(usrbuf,"\r\n\r\n");
	    content = content - (numbytes - (charpoint - usrbuf + 4));
	    socketbuf2 = new char[numbytes+content];
	    sizeofbuf=numbytes+content;
	    memcpy(socketbuf2,usrbuf,numbytes);
	    total=numbytes;
	    while(content > 0){
	      if ((newline = recv(new_fd, usrbuf,sizeof(usrbuf), 0))== -1){
		perror("recv");
		close(new_fd);
		exit(0);
	      }
	      
	      content-=newline;
	      memcpy(socketbuf2+total,usrbuf,newline);
	      total+=newline;
	      /*usrbuf[newline]='\0';
	      socketbuf+=usrbuf;
	      */
	    }
	  } else {
	    socketbuf2=new char[numbytes];
	    memcpy(socketbuf2,usrbuf,numbytes);
	    sizeofbuf=numbytes;
	  }
	} else if(!requesttype.compare("500")){
	  content=atoi(usrbuf);
	  socketbuf2=new char[content];
	  charpoint=strstr(usrbuf,"\r\n\r\n")+4;
	  total=numbytes-(charpoint-usrbuf);
	  memcpy(socketbuf2,charpoint, total);
	  while((numbytes=recv(new_fd, usrbuf, sizeof(usrbuf), 0)) != 0){
	    if(numbytes==-1){
	      perror("recv");
	      exit(1);
	    }
	    memcpy(socketbuf2+total,usrbuf,numbytes);
	    total+=numbytes;
	  }
	  sizeofbuf=content;
	}
	
	
	if(!requesttype.compare("000")){
	  /* This is an inital request for a page, handle it, but don't force*/
	  
	  tempresponse = handle_request(buf, inet_ntoa(their_addr.sin_addr), 
					*ipmap, requsr, lastknown, socketbuf2, 
					sizeofbuf, filenames, tempstore);
	  
	} else if(!requesttype.compare("001")){
	  /* This is an informational message stating that a page has been 
	     accessed,
	     this is a chance to resych. The page is cacheable
	  */
	  tempresponse = handle_info(buf, inet_ntoa(their_addr.sin_addr), requsr, *ipmap, filenames, socketbuf2, sizeofbuf, "yes", lastknown);
	} else if(!requesttype.compare("002")){
	  /* This is an informational message stating that a page has been 
	     accessed,
	     this is a chance to resych. The page is not cacheable
	  */
	  tempresponse = handle_info(buf, inet_ntoa(their_addr.sin_addr), requsr, *ipmap, filenames, socketbuf2, sizeofbuf, "no", lastknown);
	}else if(!requesttype.compare("404")){
	  /* This is a request to force retreaval from the city */
	  tempresponse = handle_force(buf, inet_ntoa(their_addr.sin_addr), requsr, *ipmap, socketbuf2, sizeofbuf, filenames);
	} else if(!requesttype.compare("300")){
	  /* This is a request to force retreaval from the city */
	 
	  tempresponse = handle_switch(buf, requsr, lastknown, filenames, inet_ntoa(their_addr.sin_addr));
	} else if(!requesttype.compare("500")){
	  /* This is an incoming page from the city, distribute it */
	  tempresponse = handle_incoming(buf, requsr, *ipmap, lastknown, socketbuf2, sizeofbuf, filenames);
	}
	delete[] socketbuf2;
	if(requesttype.compare("500")!=0){
	  /* send the appropriate response back to the client 
	   a 500 message originates from the kiosk and does not require a 
	   response
	  */
	  lenbyte = tempresponse.length();
	  response=lenbyte;
	  response+=tempresponse;
	  numbytes= response.length();
	  if (sendall(new_fd, response.c_str(), &numbytes) == -1) {
	    perror("sendall");
	    printf("We only sent %d bytes because of the error!\n", numbytes);
	  }
	  if (!tempresponse.compare("202")){
	    /* a 202 requires more then simply sending a string, it is a
	       full dump of a socket that needs to be sent */
	    send_local_file(new_fd,buf);
	  }
	  if (!tempresponse.compare(0,3,"302")){
	    changeowner(new_fd, filenames);
	  }
	  if(tempstore.find(requsr)!=tempstore.end()){
	    send_temp_storage(tempstore,requsr,filenames,
			      inet_ntoa(their_addr.sin_addr));
	     
	  }
	}
	
	close(new_fd);// parent doesn't need this
	exit(0); 
      } // closes if{ !fork() }

      
      close(new_fd);
    }
    free(ipmap);
    return 0;
}

/*checkcompleteness - checks to see that all configuration fields are
  filled in, either with values read in of defaults

  input:
  filenames - map into which to put configuration values
*/
void checkcompleteness(stringtostring * filenames){
  
  if(filenames->find("pending")==filenames->end()){
    (*filenames)["pending"]=DEFAULTPENDINGFILE;
  }

  if(filenames->find("newfiles")==filenames->end()){
    (*filenames)["newfiles"]=DEFAULTNEWFILESFILE;
  }

  if(filenames->find("mapping")==filenames->end()){
    (*filenames)["mapping"]=DEFAULTMAPPINGFILE;
  }

  if(filenames->find("deletion")==filenames->end()){
    (*filenames)["deletion"]=DEFAULTDELETIONFILE;
  }

  if(filenames->find("lastknown")==filenames->end()){
    (*filenames)["lastknown"]=DEFAULTLASTKNOWNFILE;
  }

  if(filenames->find("HOSTNAME")==filenames->end()){
    (*filenames)["HOSTNAME"]=DEFAULTHOSTNAME;
  }

  if(filenames->find("basedir")==filenames->end()){
    (*filenames)["basedir"]=DEFAULTBASEDIR;
  }

  if(filenames->find("temploc")==filenames->end()){
    (*filenames)["temploc"]=DEFAULTTEMPLOCFILE;
  }

  if(filenames->find("debugdir")==filenames->end()){
    (*filenames)["debugdir"]=NODEBUG;
  }

  if(filenames->find("celldir")==filenames->end()){
    (*filenames)["celldir"]=NOCELL;
  }

}
