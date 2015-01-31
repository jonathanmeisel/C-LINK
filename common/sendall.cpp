#include <sys/types.h>
#include <sys/socket.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <unistd.h>

#include <stdio.h>
#include <string>
#include "defines.h"
#include "lookup3.h"

using namespace std;

/*sendall - repeatedly performs a send on a socket until the buffer is empty

  input:
  s - socket number
  buf - buffer to send
  len - number of bytes to send

  output:
  len - number of bytes sent

  return : -1 on failure else 0
*/
int sendall(int s, const char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    /* keep sending until total sent is equal to total expected */
    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
	/* n == -1 indicated a send failure, stop trying 
	 any other value of n indicates sent bits, incremenet the 
	 total sent and decrement what is left to send*/
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
} 

/* receiveall - continue receiving until there is nothing left to receive as
   indicated by a leading byte in the message or until there is no room in the
   buffer
   
   input:
   s - socket address
   buf - buffer in which to store message
   len - size of buffer (or max message size)
   flags - flags to send to receive

   output:
   buf - received message
   len - amount of space left in buffer

   return:
   0 on success, -1 on failure
*/
int receiveall(int s, char *buf, int *len, int flags){
  int total = 0;
  int n;
  int expectedsize = 0;
  int remainingspace = *len;
  char temp;
  int retreaveamt;

  /* do an initial 1 byte receive to get the manditory pack length */
  n = recv(s, buf, 1, 0);
  if (n == -1) { return -1; }
  expectedsize=buf[0];

  /* as long as the total number of bytes received is less then the 
     size in the header we keep receiving */
  while (total < expectedsize) {
    /* the amount to receive next has to be the lesser of the remaining
       space in the buffer and the remaining bytes to be received */
    retreaveamt = (expectedsize-total)<remainingspace?
      (expectedsize-total):remainingspace;
    n = recv(s, buf+total, retreaveamt, flags);
    if (n == -1) { break; }
    
    /*if n is -1 the receive failed, otherwise increment the number of bytes
      fetched and decrememnt the space remaining */
    total += n;
    remainingspace -=n;
    
    /* if we run out of space, break out */
    if (remainingspace==0) { break; }
  }
  
  *len = remainingspace; // output the amount of space left in the buffer

  return n==-1?-1:0; // return -1 on failure, 0 on success
}

int lock_file(string name){
  
  string lockfile;
  int fd;
  int wd;
  char buffer[MAXDATASIZE];
  struct stat st;
  ofstream file;

  lockfile=name;
  lockfile+=".lock";
  
  if(stat(lockfile.c_str(),&st) == 0){ 
    
    fd = inotify_init();
    
    if ( fd < 0 ) {
      perror( "inotify_init" );
    }
    
    
    wd = inotify_add_watch( fd, lockfile.c_str(), IN_DELETE_SELF );    
    read( fd, buffer, sizeof(buffer) ); 
  }

  file.open(lockfile.c_str());
  file.close();

  return 1;

}

int unlock_file(string name){
  
  string lockfile=name;

  lockfile+=".lock";

  remove(lockfile.c_str());

  return 1;
}

string is_page_cacheable(char *buf){
  
  char *headerend;
  char *cache;
  string cacheable="yes";
  
  headerend=strstr(buf,"\r\n\r\n");
  cache=strstr(buf,"no-cache");
  if(cache!=NULL && (headerend==NULL || cache < headerend)){
    cacheable="no";
  }

  cache=strstr(buf,"max-age");
  if(cache!=NULL && (headerend==NULL || cache < headerend)){
    cacheable="no";
  }

  return cacheable;
}

int change_file_to_dir(string name){
  ofstream file;
  ifstream oldfile;
  char buffer[MAXDATASIZE];
  string newname;
  string subdir;
  
  subdir=name.substr(0,name.find_last_of('/'));
  subdir+="/temp";
  
  file.open(subdir.c_str(), ios::binary);
  oldfile.open(name.c_str(), ios::binary);

  while(!oldfile.fail()){
    oldfile.read(buffer,sizeof(buffer));
    file.write(buffer,oldfile.gcount());
  }
  
  oldfile.close();
  file.close();
  
  remove(name.c_str());

  mkdir (name.c_str(),S_IRWXU | S_IRGRP | 
	       S_IXGRP | S_IROTH | S_IXOTH);

  name+="/index.html";

  file.open(name.c_str(), ios::binary);
  oldfile.open(subdir.c_str(), ios::binary);

  while(!oldfile.fail()){
    oldfile.read(buffer,sizeof(buffer));
    file.write(buffer,oldfile.gcount());
  }
  
  oldfile.close();
  file.close();

  remove(subdir.c_str());

  return 1;

}

string urlhash(const char* url, int urlsize) {
  uint32_t sum1=(urlsize+url[0]) % ROLLOVER;
  uint32_t sum2=((urlsize+url[0])*urlsize+url[1]) % ROLLOVER;
  
  char hash[MAXDATASIZE];

  /*  for(int i=0;i<urlsize;i++){
    sum1+=(url[i]*(i+1));
    if(sum1 > ROLLOVER) {
      sum2+=1;
      if(sum2 > ROLLOVER) {
	sum2=0;
	sum3++;
	if(sum3 > ROLLOVER) {
	  sum3=0;
	  sum4++;
	  if(sum4 > ROLLOVER) {
	    sum4=0;
	  }
	}
      } 
      sum1=sum1-ROLLOVER;
    }
  }
  sprintf(hash,"%.8X%.8X%.8X%.8X",sum1,sum2,sum3,sum4);*/

  hashlittle2(url, urlsize, &sum1, &sum2);
  sprintf(hash,"%.8X%.8X%", sum1, sum2);

  return hash;
}
  
