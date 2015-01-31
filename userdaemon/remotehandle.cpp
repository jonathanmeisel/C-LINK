/* This file contains functions to be used on the USER DAEMON */

#include <string.h>
#include <map>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <stdlib.h>

#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <map>

#include "defines.h"
#include "remotehandle.h"
#include "sendall.h"

using namespace std;

/* handle_remote_request - determine the existance of the requested file
   and if present send it to the requester, otherwise, sends a 404 message

   inputs: sockfd - socket number

   return:
   1 if file does not exist else 0
*/

int handle_remote_request(int sockfd, stringtostring configuration, map<string,int> sizes){
  int len,mysize;
  char buf[MAXDATASIZE];
  string filename, user, page;
  ifstream file;
  ofstream outfile;
  int tildapos, movedpage;
  char *charpoint, *charpoint2;
  string moveloc;

  /* The incoming message is formated as:
   user pagename
  read in and store the username
  */
  len = sizeof(buf)-1;
  mysize=len;
  if (receiveall(sockfd, buf, &len, 0) == -1) {
    perror("recv");
    printf("We still have space to receive %d more bytes\n", len);
  }
  buf[mysize-len] = '\0';
  user = buf;

  filename="/home/";
  filename+=buf;
  filename+="/localweb/";

  /* read in the page name */
  len = sizeof(buf)-1;
  mysize=len;
  if (receiveall(sockfd, buf, &len, 0) == -1) {
    perror("recv");
    printf("We still have space to receive %d more bytes\n", len);
  }
  buf[mysize-len] = '\0';
  
  /* the localweb directory contains all of the websites owned by this node
     so get the full file and directory name and then translate the ~ into _
     because of httrack weirdness
  */

  filename+=buf;
  page =buf;

  while((tildapos = filename.find('~'))!=string::npos){
    filename[tildapos]='_';
  }    

  /* open the file, if it is not foud, return a 404 to the requestor,
     otherwise, read and send the contents of the file over to the requester 
  */

  /*  struct stat s;
  if( stat(filename.c_str(),&s) == 0 ){
    
        if( s.st_mode & S_IFDIR ){
      sprintf(buf," 200");	    
      buf[0]=3;
      len=4;
      sendall(sockfd,buf,&len);

      sprintf(buf,"HTTP/1.1 301 Moved Permanently\r\nLocation: http://%s/\r\nConnection: close\r\nContent-Type: text/html; charset=iso-8859-1\r\n\r\n<HTML><HEAD>\n<TITLE>301 Moved Permanently</TITLE>\n</HEAD><BODY>\n<H1>Moved Permanently</H1>\nThe document has moved <A HREF=\"http://%s/\">here</A>.<P>\n<HR>\n</BODY></HTML>\r\n\r\n",page.c_str(),page.c_str());
      len=strlen(buf);
      sendall(sockfd,buf,&len);
      return 0;
      }
      }*/
  file.open(filename.c_str(),ios::binary);
  
  if(!file){
    sprintf(buf," 404");
    buf[0]=3;
    len=4;
    sendall(sockfd,buf,&len);
    return 1;
  }
  sprintf(buf," 200");	    
  buf[0]=3;
  len=4;
  sendall(sockfd,buf,&len);
  
  movedpage=0;
  /*  if(strstr(buf,"301 Moved Permanently") != NULL){
    if((charpoint=strstr(buf, "Location: http://"))!=NULL){
      charpoint2=strstr(charpoint, "\r\n");
      moveloc=charpoint+17;
      moveloc.resize(charpoint2-(charpoint+17));
      if(moveloc[moveloc.length()-1]=='/') moveloc+="index.html";
      if(!moveloc.compare(page)) movedpage=1;
    } else if ((charpoint=strstr(buf, "Location: https://"))!=NULL){
      charpoint2=strstr(charpoint, "\r\n");
      moveloc=charpoint+18;
      moveloc.resize(charpoint2-(charpoint+18));
      if(moveloc[moveloc.length()-1]=='/') moveloc+="index.html";
      if(!moveloc.compare(page)) movedpage=1;
    } else {
      movedpage=1;
    }
   
  }
  */
  len=file.gcount();
  sendall(sockfd,buf,&len);

  while(!file.fail()){
    
    file.read(buf,sizeof(buf));
    len=file.gcount();
    sendall(sockfd,buf,&len);
  }
  
  file.close();

  while((tildapos = page.find('~'))!=string::npos){
    page[tildapos]='_';
  }

  if(movedpage){
    file.open(filename.c_str());
    file.seekg(0,ios::end);
    mysize=file.tellg();
    file.close();

    remove(filename.c_str());
    
    sizes[user]-=mysize;
    
    lock_file(configuration["sizefile"]);
    outfile.open(configuration["sizefile"].c_str());
    for(map<string,int>::const_iterator it = sizes.begin(); 
	it != sizes.end(); ++it){
      outfile << it->first << ' ' << it->second << endl;
    }
    outfile.close();

    unlock_file(configuration["sizefile"]);

    lock_file(configuration["deletion"]);
    outfile.open(configuration["deletion"].c_str(), ios::app);
    outfile << user << ' ' << page << endl;
    outfile.close();
    unlock_file(configuration["deletion"]);

  } else {
    
    lock_file(configuration["used"]);
    outfile.open(configuration["used"].c_str(), ios::app);
    outfile << user << ' ' << page << endl;
    outfile.close();
    unlock_file(configuration["used"]);
  }

  return 0;

}


/* handle_new_file - a file that we have previously requested has been delivered
   as a tar ball. Untar it in the appropriate directory

   input:
   new_fd - socket number
   usersizes - a mapping of the usernames and cache storage space
   sizefile - file back up of the map above

   return:
   number of bytes read in
*/
int handle_new_file(int new_fd, map<string,int> *usersizes, stringtostring config, stringtostringqueue LRUqueue){

  ofstream file;
  char buf[MAXDATASIZE];
  int numbytes;
  int len=0;
  int mysize;
  string extend,sysstring;
  string username;
  int slashpos;
  string name;
  ifstream test;
  struct stat s;
  /* read in the username and ensure the local web directory exists
   */
  len = sizeof(buf)-1;
  mysize=len;
  if (receiveall(new_fd, buf, &len, 0) == -1) {
    perror("recv");
    printf("We still have space to receive %d more bytes\n", len);
  }
  buf[mysize-len] = '\0';
  username=buf;
  extend="/home/";
  extend+=buf;
  extend+="/localweb/";
  slashpos=extend.length();

  /* read in the filename and open a file for writing */
  len = sizeof(buf)-1;
  mysize=len;
  if (receiveall(new_fd, buf, &len, 0) == -1) {
    perror("recv");
    printf("We still have space to receive %d more bytes\n", len);
  }
  buf[mysize-len] = '\0';  
  
  extend+=buf;
  lock_file(config["used"]);
  file.open(config["used"].c_str(), ios::app);
  file << username << ' ' << buf << endl;
  file.close();
  unlock_file(config["used"]);
  
  slashpos=extend.find('/', slashpos+1);
  while (slashpos != string::npos) {
    name=extend.substr(0,slashpos);
    if( stat(name.c_str(),&s) == 0 ){
      
      if( s.st_mode & S_IFREG ){
	change_file_to_dir(name);
      }
    } else {
  /* SIBREN: ownership problems exist here since the daemon is running as root*/
      mkdir (name.c_str(),S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    }
    slashpos = extend.find('/', slashpos+1);
  }


  test.open(extend.c_str(), ios::binary);
  if(!test.fail()){
    test.seekg(0,ios::end);
    (*usersizes)[username]-=test.tellg();
  }
  test.close();


  /* continue to read from the socket into the file 
     until the connection is closed
   */
  file.open(extend.c_str(), ios::binary);
  mysize=0;
  while (numbytes=recv(new_fd, buf, sizeof(buf),0)){
    mysize+=numbytes;
    file.write(buf,numbytes);
  }

  file.close();

  //update the user/size mapping
  if(usersizes->find(username) != usersizes->end()){
    (*usersizes)[username]+=mysize;
  } else {
    (*usersizes)[username]=mysize;
  }

  if((*usersizes)[username]>atoi(config["quota"].c_str())){
    (*usersizes)[username]-=changeownership(username, config, LRUqueue, usersizes);
  }


  lock_file(config["sizefile"]);
  file.open(config["sizefile"].c_str());
  for(map<string,int>::const_iterator it = usersizes->begin(); 
      it != usersizes->end(); ++it){
    file << it->first << ' ' << it->second << endl;
  }
  file.close();
  unlock_file(config["sizefile"]);
  

  return numbytes;
}

/* readusersizes reads in the file containue the cache utilization of each user
   when a user exceeds the quota, a message to that effect is sent to the kiosk
   
   inputs:
   configuration - the map of the configuration info

   return:
   map of users and their cahe utilization
*/
map<string,int> readusersizes(stringtostring configuration, stringtostringqueue *LRUqueue){
  map<string,int> sizemapping;
  ifstream file;
  ofstream outfile;
  char buf[MAXDATASIZE];
  int size,spaceloc1;
  char *charpoint;
  string str;
  string user, page;
  int modified=0;


  lock_file(configuration["used"]);
  file.open(configuration["used"].c_str());
  while(!file.fail()){
    getline(file,str);
    if((spaceloc1=str.find(' ')) > 0){
      modified=1;
      user=str.substr(0,spaceloc1);
      page=str.substr(spaceloc1+1);
      deque<string>::iterator it=(*LRUqueue)[user].begin();
      while(it!=(*LRUqueue)[user].end()){ 
	if(!((*it).compare(page))){

	  (*LRUqueue)[user].erase(it);

	  it--;
	}
	it++;
      }

      (*LRUqueue)[user].push_front(page);

    }
  }
  file.close();
  

  outfile.open(configuration["used"].c_str());
  outfile.close();
  unlock_file(configuration["used"]);


  lock_file(configuration["deletion"]);
  file.open(configuration["deletion"].c_str());
  while(!file.fail()){
    getline(file,str);
    if((spaceloc1=str.find(' ')) > 0){
      modified=1;
      user=str.substr(0,spaceloc1);
      page=str.substr(spaceloc1+1);
      for(deque<string>::iterator it=(*LRUqueue)[user].begin();
	  it!=(*LRUqueue)[user].end(); ++it){
	if(!((*it).compare(page))){
	  (*LRUqueue)[user].erase(it);
	}
      }
    }     
  }
  file.close();

  outfile.open(configuration["deletion"].c_str());
  outfile.close();
  unlock_file(configuration["deletion"]);

  lock_file(configuration["sizefile"]);
  file.open(configuration["sizefile"].c_str());
  while(!file.fail()){
    file.getline(buf,sizeof(buf));
    if((charpoint=strchr(buf,' '))!=NULL){

      size=atoi(charpoint);
      *charpoint='\0';
      sizemapping[buf]=size;
    }
  }

  file.close();
  unlock_file(configuration["sizefile"]);

  if(modified==1){
    lock_file(configuration["LRU"]);
    outfile.open(configuration["LRU"].c_str());
    for(stringtostringqueue::const_iterator it=LRUqueue->begin();
	it != LRUqueue->end();++it){
      for(deque<string>::const_iterator it2=it->second.begin();
	  it2 != it->second.end();++it2){
	outfile << it->first << ' ' << *it2 << endl;
      }
    }
    outfile.close();
    unlock_file(configuration["LRU"]);
  }

  return sizemapping;
}

/*changeownership uses the LRU stratagy to remove the least recently used
  item from the user indicated, attempting to place it on the emptiest available
  node. If no such node exists, the file may be deleted.

  inputs:
  user - the user from whom files must be removed
  configuration - the configuration information read in
  
  input/output:
  usersizes - the map of user to utilization
*/
int changeownership(string user, stringtostring configuration, stringtostringqueue LRUqueue, map<string,int> *usersizes) {

  struct hostent *he;
  struct sockaddr_in kiosk_addr;    // kiosk address information
  int sockfd, len, mysize;
  string sendbuf;
  char bytesize;
  char buf[MAXDATASIZE];
  int total;
  
  if ((he=gethostbyname(configuration["kioskip"].c_str())) == NULL) {
    herror("gethostbyname");
    exit(1);
  }
  
  if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }
  
  kiosk_addr.sin_family = AF_INET;    // host byte order 
  kiosk_addr.sin_port = htons(MYPORT);  // short, network byte order 
  kiosk_addr.sin_addr = *((struct in_addr *)he->h_addr);
  memset(kiosk_addr.sin_zero, '\0', sizeof kiosk_addr.sin_zero);
  
  if (connect(sockfd, (struct sockaddr *)&kiosk_addr,
	      sizeof kiosk_addr) == -1) {
    perror("connect");
    exit(1);
  }

  bytesize=3;
  sendbuf=bytesize;
  sendbuf+="300";
  bytesize=user.length();
  sendbuf+=bytesize;
  sendbuf+=user;
  bytesize=configuration["free"].length();
  sendbuf+=bytesize;
  sendbuf+=configuration["free"];
  sendbuf+="EMPTYSTRING";
  len=sendbuf.length();

  if(sendall(sockfd,sendbuf.c_str(),&len)==-1){
    perror("send");
    printf("We were only able to receive %d bytes\n",len);
  }

  len = 3;
  mysize=len;
  if (receiveall(sockfd, buf, &len, 0) == -1) {
    perror("recv");
    printf("We still have space to receive %d more bytes\n", len);
  }
  buf[mysize-len] = '\0';

  if(!strcmp(buf,"304")){
    total=deletefiles(user, LRUqueue, configuration);
  } else if(!strcmp(buf,"302")){
    total=movefiles(sockfd, user, LRUqueue, configuration, usersizes);
  }

  close(sockfd);

  return total;
}

void send_user_sizes(int sockfd, map<string,int> usersizes, int quota){

  char buf[MAXDATASIZE];
  string sendbuf;
  char bytesize;
  int len;

  sprintf(buf,"%d",usersizes.size());
  bytesize=strlen(buf);
  sendbuf=bytesize;
  sendbuf+=buf;

  for(map<string,int>::const_iterator it = usersizes.begin(); 
      it != usersizes.end(); ++it){
    bytesize=it->first.length();
    sendbuf+=bytesize;
    sendbuf+=it->first;
    sprintf(buf,"%d",quota-it->second);
    bytesize=strlen(buf);
    sendbuf+=bytesize;
    sendbuf+=buf;
  }
  
  len=sendbuf.length();
  if(sendall(sockfd,sendbuf.c_str(),&len)==-1){
    perror("send");
    printf("We were only able to receive %d bytes\n",len);
  }

  return;
}

int deletefiles(string user, stringtostringqueue LRUqueue, stringtostring configuration){
  int spacetobefreed=atoi(configuration["free"].c_str());
  int total=0;
  ifstream file;
  string filename,sendbuf;
  ofstream delfile;

  while(spacetobefreed>total){
    filename=LRUqueue[user].back();
    LRUqueue[user].pop_back();
    sendbuf="/home/";
    sendbuf+=user;
    sendbuf+="/localweb/";
    sendbuf+=filename;

    file.open(sendbuf.c_str());
    file.seekg(0,ios::end);
    total+=file.tellg();
    file.close();

    remove(sendbuf.c_str());
    
    lock_file(configuration["deletion"]);
    delfile.open(configuration["deletion"].c_str(), ios::app);
    delfile << user << ' ' << filename << endl;
    delfile.close();
    unlock_file(configuration["deletion"]);

  }  
  
  return total;

}


int movefiles(int sockfd, string user, stringtostringqueue LRUqueue, stringtostring configuration, map<string,int> *usersizes){

  int spacetobefreed=atoi(configuration["free"].c_str());
  string filename, remoteuser, sendbuf;
  int len,mysize,slashpos,total=0;
  char buf[MAXDATASIZE];
  struct hostent *he;
  struct sockaddr_in their_addr; // connector's address information
  int new_fd;  // listen on sock_fd
  char bytesize;
  ifstream file;
  ofstream outfile;
  string kiosk;

  len = sizeof(buf)-1;
  mysize=len;
  if (receiveall(sockfd, buf, &len, 0) == -1) {
    perror("recv");
    printf("We still have space to receive %d more bytes\n", len);
  }
  buf[mysize-len] = '\0';
  remoteuser=buf;

  len = sizeof(buf)-1;
  mysize=len;
  if (receiveall(sockfd, buf, &len, 0) == -1) {
    perror("recv");
    printf("We still have space to receive %d more bytes\n", len);
  }
  buf[mysize-len] = '\0';
  kiosk=buf;
  while(spacetobefreed>total){
    filename=LRUqueue[user].back();
    LRUqueue[user].pop_back();

    bytesize=3;
    sendbuf=bytesize;
    sendbuf+="305";
    bytesize=remoteuser.length();
    sendbuf+=bytesize;
    sendbuf+=remoteuser;
    bytesize=filename.length();
    sendbuf+=bytesize;
    sendbuf+=filename;
    len=sendbuf.length();

    if (sendall(sockfd, sendbuf.c_str(), &len) == -1) {
      perror("send");
      printf("We only sent %d bytes because of the error!\n", len);
    }

    if(kiosk.compare("you")!=0){

      sendbuf.replace(1,3,"200");

      if ((he=gethostbyname(kiosk.c_str())) == NULL) {  // get the host info 
	herror("gethostbyname");
	exit(1);
      }
      
      if ((new_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
	perror("socket");
	exit(1);
      }
      
      their_addr.sin_family = AF_INET;    // host byte order 
      their_addr.sin_port = htons(FILEPORT);  // short, network byte order 
      their_addr.sin_addr = *((struct in_addr *)he->h_addr);
      memset(their_addr.sin_zero, '\0', sizeof their_addr.sin_zero);
      
      /* attempt to connect to last known address for user */
      if (connect(new_fd, (struct sockaddr *)&their_addr,
		  sizeof their_addr) == -1) {
	perror("connect");
	exit(1);
      }
	
      if (sendall(new_fd, sendbuf.c_str(), &len) == -1) {
	perror("send");
	printf("We only sent %d bytes because of the error!\n", len);
      }
    } else {

      sendbuf="/home/";
      sendbuf+=remoteuser;
      sendbuf+="/localweb/";
      slashpos=sendbuf.length();

      sendbuf+=filename;
      slashpos=sendbuf.find('/', slashpos+1);

      while (slashpos != string::npos) {
	/* SIBREN: ownership problems exist here too*/
	mkdir (sendbuf.substr(0,slashpos).c_str(),
	       S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	slashpos = sendbuf.find('/', slashpos+1);
      }
      
      outfile.open(sendbuf.c_str(), ios::binary);

    }    
    
    sendbuf="/home/";
    sendbuf+=user;
    sendbuf+="/localweb/";
    sendbuf+=filename;
    
    file.open(sendbuf.c_str(), ios::binary);
    while(!file.fail()){
      file.read(buf, sizeof(buf));
      len=file.gcount();
      total+=len;
      
      if(kiosk.compare("you")!=0){
	if (sendall(new_fd,buf,&len) == -1) {
	  perror("send");
	  printf("We only sent %d bytes because of the error!\n", len);
	}
      } else {
	outfile.write(buf,len);
      }
	
    }
    
    file.close();
    outfile.close();
    remove(sendbuf.c_str());

    lock_file(configuration["deletion"]);
    outfile.open(configuration["deletion"].c_str(), ios::app);
    outfile << user << ' ' << filename << endl;
    outfile.close();
    unlock_file(configuration["deletion"]);

    if(kiosk.compare("you")!=0){
      close(new_fd);
    } else {
      lock_file(configuration["used"]);
      outfile.open(configuration["used"].c_str(), ios::app);
      outfile << user << ' ' << filename << endl;
      outfile.close();
      unlock_file(configuration["used"]);
    }

   
    
  }

  sendbuf=" 333";
  sendbuf[0]=3;
  len=sendbuf.length();

  if (sendall(sockfd, sendbuf.c_str(), &len) == -1) {
    perror("send");
    printf("We only sent %d bytes because of the error!\n", len);
  }
  if(!kiosk.compare("you")){
    (*usersizes)[remoteuser]+=total;
  }
  return total;
}

stringtostringqueue read_LRU_file(stringtostring config){
  ifstream file;
  stringtostringqueue output;
  string str,user,page;
  int spaceloc1;

  lock_file(config["LRU"]);
  file.open(config["LRU"].c_str());
  while(!file.fail()){
    getline(file,str);
    if((spaceloc1=str.find(' ')) > 0){;
      user=str.substr(0,spaceloc1);
      page=str.substr(spaceloc1+1);
      
      output[user].push_back(page);
    }
  }

  file.close();
  unlock_file(config["LRU"]);

  return output;
}

int deletedupes(int fd, map<string,int> sizes, stringtostring config){
  
  int len, mysize;
  char buf[MAXDATASIZE];
  string user, page, filename;
  ifstream file;
  ofstream outfile;
  

  len = sizeof(buf)-1;
  mysize=len;
  if (receiveall(fd, buf, &len, 0) == -1) {
    perror("recv");
    printf("We still have space to receive %d more bytes\n", len);
  }
  buf[mysize-len] = '\0';
  user=buf;
  
  len = sizeof(buf)-1;
  mysize=len;
  if (receiveall(fd, buf, &len, 0) == -1) {
    perror("recv");
    printf("We still have space to receive %d more bytes\n", len);
  }
  buf[mysize-len] = '\0';
  page=buf;

  filename="/home/";
  filename+=user;
  filename+="/localweb/";
  filename+=page;

  file.open(filename.c_str());
  file.seekg(0,ios::end);
  mysize=file.tellg();
  file.close();
  
  remove(filename.c_str());

  sizes[user]-=mysize;

  lock_file(config["deletion"]);
  outfile.open(config["deletion"].c_str(), ios::app);
  outfile << user << ' ' << page << endl;
  outfile.close();
  unlock_file(config["deletion"]);

  lock_file(config["sizefile"]);
  outfile.open(config["sizefile"].c_str());
  for(map<string,int>::const_iterator it = sizes.begin(); 
      it != sizes.end(); ++it){
    outfile << it->first << ' ' << it->second << endl;
  }
  outfile.close();
  unlock_file(config["sizefile"]);

  return 1;

}
