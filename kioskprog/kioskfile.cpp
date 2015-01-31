/* this is the main file for the kiosk program used to alert the daemon that a
   requested file has arrived. It is assumed that some preprocessing has 
   been performed by the calling script and thus the file is tar and 
   located appropriatly
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string>
#include <iostream>
#include <fstream>

#include "sendall.h"
#include "defines.h"

int main(int argc, char *argv[])
{
  int sockfd, numbytes;  
  char buf[MAXDATASIZE];
  struct hostent *he;
  struct sockaddr_in their_addr; // connector's address information 
  int mysize, len;
  string sendbuf;
  char *size=new char;
  int fieldsize;
  string page,user;

  ifstream file;
  
  /* we establish a connection with 127.0.0.1 on the kioskdaemon port */
  if (argc != 3) {
    fprintf(stderr,"usage: client hostname file\n");
    exit(1);
    }
  
  if ((he=gethostbyname("127.0.0.1")) == NULL) {  // get the host info 
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

  file.open(argv[2], ios::binary); // Open the file
  file.getline(buf,sizeof(buf));
  page=buf; // This is the HASHED name of the page; can CERTAINLY be in .desc file
  if((len=page.find("http://"))!=string::npos){
    page.erase(len,7);
  }

  /*oddly, for backwards compatability, we have to translate ~
    SIBREN: remove this from the system
  */
  while((len=page.find('~'))!=string::npos){ // WEIRD, don't do it
    page[len]='_';
  }

  /* inform the kioskdaemon that the file has arrived with a 500 message:
     500 user pagename
  */
  *size=3;
  sendbuf=size;
  sendbuf+="500";
  *size=strlen(argv[1]);
  sendbuf+=*size;
  sendbuf+=argv[1];
  *size=page.length();
  sendbuf+=*size;
  sendbuf+=page;

  len=file.tellg();
  file.seekg(0,ios::end);
  mysize=file.tellg();
  sprintf(buf,"%ld",mysize-len);
  sendbuf+=buf;
  file.seekg(len);
  sendbuf+="\r\n\r\n";
  len = sendbuf.length();
  delete size;
  
  // Here sends user / pagename / all sorts of weird information
  if (sendall(sockfd, sendbuf.c_str(), &len) == -1) {
    perror("send");
    printf("We only sent %d bytes because of the error!\n", len);
  }

  while(!file.fail()){
    file.read(buf,sizeof(buf));
    len=file.gcount();
    if (sendall(sockfd, buf, &len) == -1) {
      perror("send");
      printf("We only sent %d bytes because of the error!\n", len);
    }
  } 
  file.close();
  

  /* make sure the message was sent okay */
  /*len = 3;
  mysize=len;
  if (receiveall(sockfd, buf, &len, 0) == -1) {
    perror("recv");
    printf("We still have space to receive %d more bytes\n", len);
  }
  buf[mysize-len] = '\0';
  */
  
  //SIBREN: remove this eventually
  buf[0]='1';
  buf[1]='0';
  buf[2]='0';
  buf[3]='\0';
  
  if(!strcmp(buf,"100")){
    /* we're good */
  } else {
    /* not so good */
    cerr << "A major error has occured..." << endl;
  }
  close(sockfd);
  
  return 0;
} 

