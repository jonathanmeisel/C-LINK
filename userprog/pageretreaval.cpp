/* This file contains fuctions to be used by the USER */

#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>

#include "defines.h"
#include "pageretreaval.h"
#include "sendall.h"

using namespace std;

/*handle_local_page -- move a page already located on the local machine into
the current users local web. Eventuall, this will just dump into the correct
socket.

sockfd - socket on which we comunicate with the kiosk
kioskip - unused
pagename - page to retreave
new_fd - socket number for the connection to the browser
dynamicbuf - buffer containing information from the browser
sizeofbuf - number of bytes in the dynamicbuf

*/
unsigned long int handle_local_page(int sockfd, stringtostring config, string pagename, int new_fd, char *dynamicbuf, int sizeofbuf, unsigned long int *pretime){
  int len,mysize;
  char buf[MAXDATASIZE];
  string from,to,trans;
  ifstream read;
  ofstream write;
  string user;
  unsigned long int done;
  struct timespec ts;

  /*the incoming message on the socket gives the username that currently
    owns the page, read it out
  */
  len = sizeof(buf);
  mysize=len;
  if (receiveall(sockfd, buf, &len, 0) == -1) {
    perror("recv");
    printf("We still have space to receive %d more bytes\n", len);
  }
    
  buf[mysize-len] = '\0';
  
  /*translate ~ into _ because of httrack weirdness */
  trans=translate(pagename);

  /* open and copy the file from one user to another*/
  from="/home/";
  from+=buf;
  from+="/localweb/";
  from+=trans;

  user=buf;

  to="/home/";
  to+=getenv("USER");
  len=to.length();
  to+="/localweb/";
  to+=trans;
  

  read.open(from.c_str());

  if (!read) {
    // we don't have that page locally any more, update the kiosk
    return forcerequest(config["kioskip"], pagename, new_fd, dynamicbuf, sizeofbuf, pretime);
  }
  //write.open(to.c_str(),ios::binary);
  clock_gettime(CLOCK_REALTIME, &ts);
  *pretime=ts.tv_nsec;

  while(!read.fail()){
    read.read(buf,sizeof(buf));
    // write.write(buf,read.gcount());
    len=read.gcount();
    sendall(new_fd,buf,&len);
  }
  
  clock_gettime(CLOCK_REALTIME, &ts);
  done=ts.tv_nsec;

  lock_file(config["used"]);
  write.open(config["used"].c_str(),ios::app);
  write << user << ' ' << trans << endl;
  write.close();
  unlock_file(config["used"]);

  read.close();
  // write.close();
  
  return done;
}

/*handle_remote_page -- move a page already located on the local machine into
the current users local web. Eventually, this will just dump into the correct
socket.

sockfd - socket on which we comunicate with the kiosk
kioskip - the ip of the kiosk, incase we need a new connection
pagename - page to retreave
new_fd - socket number for the connection to the browser
dynamicbuf - buffer containing information from the browser
sizeofbuf - number of bytes in the dynamicbuf
*/
unsigned long int handle_remote_page(int sockfd, string kioskip, string pagename, int new_fd, char *dynamicbuf, int sizeofbuf, unsigned long int *pretime){
  int len, mysize;
  string user, ipad;
  char buf[MAXDATASIZE];
  int remotesockfd;  
  struct hostent *he;
  struct sockaddr_in their_addr; // connector's address information 

  string sendreq;
  char bytesize=3;

  unsigned long int done;

  /*The incoming message has two peices of data:
    remoteusername remoteipaddress
    make sure they are both read in using recvall
  */
  len = sizeof(buf);
  mysize=len;
  if (receiveall(sockfd, buf, &len, 0) == -1) {
    perror("recv");
    printf("We still have space to receive %d more bytes\n", len);
  }
  
  buf[mysize-len] = '\0';
  user=buf;



  len = sizeof(buf);
  mysize=len;
  if (receiveall(sockfd, buf, &len, 0) == -1) {
    perror("recv");
    printf("We still have space to receive %d more bytes\n", len);
  }
  
  buf[mysize-len] = '\0';
  
  ipad = buf;

  /*create a new connection to the village machine specified*/
  if ((he=gethostbyname(ipad.c_str())) == NULL) {  // get the host info 
    cout << "ip addr: " << ipad << endl;
    cout << "page name: " << pagename << endl;
    cout << "user: " << user << endl;
    herror("gethostbyname");
    exit(1);
  }
  
  if ((remotesockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }
  
  their_addr.sin_family = AF_INET;    // host byte order 
  their_addr.sin_port = htons(FILEPORT);  // short, network byte order 
  their_addr.sin_addr = *((struct in_addr *)he->h_addr);
  memset(their_addr.sin_zero, '\0', sizeof their_addr.sin_zero);
  
  if (connect(remotesockfd, (struct sockaddr *)&their_addr,
	      sizeof their_addr) == -1) {

    /* We can't find the remote machine so we have to force the request.
     */
    close(remotesockfd);
    return forcerequest(kioskip, pagename, new_fd, dynamicbuf, sizeofbuf, pretime);
    
  }

  /*send a request for the page to the USERdaemon. The format of a 201:
   201 user pagename
  we will read this with a receiveall, so include the length of the pieces*/
  sendreq=bytesize;
  sendreq+="201";
  bytesize=user.length();
  sendreq+=bytesize;
  sendreq+=user;
  bytesize=pagename.length();
  sendreq+=bytesize;
  sendreq+=pagename;

  len=sendreq.length();

  if (sendall(remotesockfd, sendreq.c_str(), &len) == -1) {
    perror("sendall");
    printf("We only sent %d bytes because of the error!\n", len);
  }

  /*get the reply form the remote machine*/
  len = 3;
  mysize=len;
  if (receiveall(remotesockfd, buf, &len, 0) == -1) {
    perror("recv");
    printf("We still have space to receive %d more bytes\n", len);
  }
  
  buf[mysize-len] = '\0';

  if(!strcmp(buf,"404")){
    /*The remote machine no longer (or never had) the page, inform the kiosk
      that the page must be retreaved and to update its tables
    */
   
    done=forcerequest(kioskip, pagename, new_fd, dynamicbuf, sizeofbuf, pretime);
  } else {
    
    /* The page was found, prepare to receive the page */
    done=writedata(remotesockfd,pagename, new_fd, pretime);
  }

  close(remotesockfd);
  return done;
}

/*forcerequest - sends a message to the kiosk forcing a request to be sent out
  to the city. This causes the kiosk to delete the page's location in internal
  tables. This adds the current user to the poending queue for this page.

kioskip - ip address of the kiosk
pagename - pagename to force and delete
new_fd - socket number of the browser
dynamicbuf - buffer containing information from the browser
sizeofbuf - number of bytes in the dynamicbuf

output:
pretime - the time before data is sent back

*/

unsigned long int forcerequest(string kioskip,string pagename, int new_fd, char* dynamicbuf, int sizeofbuf, unsigned long int *pretime){
  struct hostent *he;
  struct sockaddr_in their_addr; // connector's address information 
  int sockfd;
  string sendreq;
  char bytesize=3;
  int len, mysize;
  char buf[MAXDATASIZE];
  unsigned long int done;
  struct timespec ts;

  /* connect to kiosk */
  if ((he=gethostbyname(kioskip.c_str())) == NULL) {  // get the host info 
    cout << "Page, line 242" << endl;
    herror("gethostbyname");
    exit(1);
  }
  
  if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }
  
  their_addr.sin_family = AF_INET;    // host byte order 
  their_addr.sin_port = htons(MYPORT);  // short, network byte order 
  their_addr.sin_addr = *((struct in_addr *)he->h_addr);
  memset(their_addr.sin_zero, '\0', sizeof their_addr.sin_zero);
  
  if (connect(sockfd, (struct sockaddr *)&their_addr,
	      sizeof their_addr) == -1) {
    perror("connect");
    exit(1);
  }

  /*send a message to the kiosk that the page was not found. format of the 404:
    404 pagename usernametoaddtoqueue
  */
  sendreq=bytesize;
  sendreq+="404";
  bytesize=pagename.length();
  sendreq+=bytesize;
  sendreq+=pagename;  
  bytesize=strlen(getenv("USER"));
  sendreq+=bytesize;
  sendreq+=getenv("USER");
  len= sendreq.length();

  if (sendall(sockfd, sendreq.c_str(), &len) == -1) {
    perror("sendall");
    printf("We only sent %d bytes because of the error!\n", len);
  }

  if (sendall(sockfd, dynamicbuf, &sizeofbuf) == -1) {
    perror("sendall");
    printf("We only sent %d bytes because of the error!\n", sizeofbuf);
  }
  
  len = 3;
  mysize=len;

  /* check the reply form the kiosk */
  if (receiveall(sockfd, buf, &len, 0) == -1) {
    perror("recv");
    printf("We still have space to receive %d more bytes\n", len);
  }
  buf[mysize-len] = '\0';
  
  if(!strcmp(buf,"100")){
    //do something if the force is successful.

    clock_gettime(CLOCK_REALTIME, &ts);
    *pretime=ts.tv_nsec;

    sprintf(buf,"HTTP/1.1 200 OK\r\nProxy-Agent: Custom\r\nCache-Control:no-cache\r\n\r\n<h1>Please wait</h1>Your page, %s, has been sent to the city and will be ready after the next bus returns.\r\n\r\n", pagename.c_str());
    len=strlen(buf);
    sendall(new_fd, buf, &len);
      } else {
    //the force failed!
  }
  clock_gettime(CLOCK_REALTIME, &ts);
  done=ts.tv_nsec;
 
  close(sockfd);
  return done;
}

/* writedata - reads all the data from a socket into a specified file

sockfd - the socket from which to read
pagename - the directory and file that need to be written
new_fd - socket number for the browser

*/
unsigned long int writedata(int sockfd, string pagename, int new_fd, unsigned long int *pretime){

  ofstream file;
  char buf[MAXDATASIZE];
  int numbytes;
  int placeholder;
  int tildapos=0;
  string extend;
  struct timespec ts;
  
  clock_gettime(CLOCK_REALTIME, &ts);
  *pretime = ts.tv_nsec;

  /* keep reading and writing until there is nothing left */
  while (numbytes=recv(sockfd, buf, sizeof(buf),0)){
    sendall(new_fd,buf,&numbytes);
  }

clock_gettime(CLOCK_REALTIME, &ts);
  return ts.tv_nsec;
}

/* handle_page_on_kiosk - forward page data from the kiosk stright
   to the browser

   input:
   kioskfd - socket number for the kiosk
   sockfd - socket number for the browser
*/
unsigned long int handle_page_on_kiosk(int kioskfd, int sockfd, unsigned long int *pretime){

  char buf[MAXDATASIZE];
  int numbytes;
  struct timespec ts;

  clock_gettime(CLOCK_REALTIME, &ts);
  *pretime = ts.tv_nsec;

  while (numbytes=recv(kioskfd, buf, sizeof(buf),0)){
    sendall(sockfd,buf,&numbytes);
  }

  clock_gettime(CLOCK_REALTIME, &ts);
  return ts.tv_nsec;
}

/* translate - change  ~ into _ in strings
   name - string to be translated

   return - translation 
*/
string translate(string name) {
  int tildapos;

  while((tildapos = name.find('~'))!=string::npos){
    name[tildapos]='_';
  }
  while((tildapos = name.find("%7E"))!=string::npos){
    name.replace(tildapos,3,"_");
  }
  return name;
}

int handle_duped_page(string page){
 
  struct hostent *he;
  struct sockaddr_in their_addr; // connector's address information 
  int remotesockfd;
  string sendreq;
  char bytesize;
  string user=getenv("USER");
  int len;

    /*create a new connection to out local daemon*/
  if ((he=gethostbyname("127.0.0.1")) == NULL) {  // get the host info 
    cout << "page: line 398" << endl;
    herror("gethostbyname");
    exit(1);
  }
  
  if ((remotesockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }
  
  their_addr.sin_family = AF_INET;    // host byte order 
  their_addr.sin_port = htons(FILEPORT);  // short, network byte order 
  their_addr.sin_addr = *((struct in_addr *)he->h_addr);
  memset(their_addr.sin_zero, '\0', sizeof their_addr.sin_zero);
  
  if (connect(remotesockfd, (struct sockaddr *)&their_addr,
	      sizeof their_addr) == -1) {

    close(remotesockfd);
    return 0;
    
  }

  /*send a request for the deletion to the USERdaemon. The format of a 505:
   505 user pagename
  we will read this with a receiveall, so include the length of the pieces*/
  sendreq=3;
  sendreq+="505";
  bytesize=user.length();
  sendreq+=bytesize;
  sendreq+=user;
  bytesize=page.length();
  sendreq+=bytesize;
  sendreq+=page;

  len=sendreq.length();

  if(sendall(remotesockfd, sendreq.c_str(), &len)==-1){
    perror("sendall");
    exit(1);
  }

  return 1;
}
