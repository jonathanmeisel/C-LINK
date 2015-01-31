/** clientprog.cpp -- This is the program used by the USER to request a page
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
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <sys/time.h>

#include <sendall.h>
#include <defines.h>
#include "pageretreaval.h"

/*checkcompleteness - checks to see that all configuration fields are
  filled in, either with values read in of defaults

  input:
  filenames - map into which to put configuration values
*/
void checkcompleteness(stringtostring * filenames){
  
  if(filenames ->find("kioskip")==filenames->end()){
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
  if(filenames->find("debugdir")==filenames->end()){
    (*filenames)["debugdir"]=NODEBUG;
  }
  

}

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[])
{
  int sockfd, numbytes, listenfd, new_fd;  
  char buf[MAXDATASIZE];
  struct hostent *he;
  struct sockaddr_in their_addr; // connector's address information 
  struct sockaddr_in my_addr;    // my address information
  struct sockaddr_in kiosk_addr;    // kiosk address information
  socklen_t sin_size;
  struct sigaction sa;

  int mysize, len, initsize, content, newline, total, sizeofbuf;
  string sendbuf;
  char *dynamicbuf;
  char *charpoint, *charpoint2;
  char *size=new char;
  int fieldsize;
  string page, response;
  ifstream fp;
  int yes=1;
  string argv2, argv1="NULL";
  ifstream file;
  int post=0,found=0;
  stringtostring configuration;
  ofstream output;
  int header;
  unsigned long int sum=0;
  unsigned long int sum2=0;
  
  char sumbuf[MAXDATASIZE];
  string cacheable;
  int movedpage=0;
  string moveloc;
  unsigned long int reqmade, reqserviced;
  unsigned long int pretime;

  string timingfile, timingfilepre;

  struct timespec ts,ts2;
  
  /* if an argument is given, it is the config file, otherwise, try the
     default config file
  */
  if (argc!=1){
    fp.open(argv[1]);
  } else {
    fp.open(DEFAULTUSERFILE);
  }
  
  while(!fp.fail()){
    fp.getline(buf,sizeof(buf));
    response=buf;
    if((response[0]!='#') && (numbytes=response.find('='))!=string::npos){
      configuration[response.substr(0,numbytes)]=response.substr(numbytes+1);;
    }
  }
  
  fp.close();
  
  checkcompleteness(&configuration);
  
  argv1=configuration["kioskip"];
  
  timingfile=configuration["debugdir"];
  timingfile+='/';
  timingfile+=getenv("USER");
  timingfilepre=timingfile;
  timingfile+="timingpost";
  timingfilepre+="timingpre";

  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }
  
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
    perror("setsockopt");
    exit(1);
  }
  
  my_addr.sin_family = AF_INET;         // host byte order
  my_addr.sin_port = htons(FFPORT);     // short, network byte order
  my_addr.sin_addr.s_addr = htonl(INADDR_ANY); // automatically fill with my IP
  memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);
  
  if (bind(listenfd, (struct sockaddr *)&my_addr, sizeof my_addr) == -1) {
    perror("bind");
    exit(1);
  }
  
  if (listen(listenfd, BACKLOG) == -1) {
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
    if ((new_fd = accept(listenfd, (struct sockaddr *)&their_addr,	\
			 &sin_size)) == -1) {
      perror("accept");
      continue;
    }
    
    if (!fork()) { // this is the child process
      clock_gettime(CLOCK_REALTIME, &ts);
      reqmade=ts.tv_nsec;
      close(listenfd); // child doesn't need the listener
      
      if ((initsize = recv(new_fd, buf,sizeof(buf), 0))== -1){
	perror("recv");
	close(new_fd);
	exit(0);
      }
      
      sizeofbuf=initsize;
     
      if((charpoint=strstr(buf,"GET "))==NULL){
	charpoint=strstr(buf,"POST ");
	post=1;
      }
      charpoint2=strstr(buf,"HTTP/");
      
      //	argv1="127.0.0.1";
      argv2=(charpoint+post+4);
      argv2.resize(charpoint2-charpoint-5-post);
      if(argv2[argv2.length()-1]=='/'){
	argv2+="index.html";
      }
      
      argv2=urlhash(argv2.c_str(), argv2.length());
      
      
      if(post){
	//there might be more then just 1024 bytes in a post request
	charpoint=strstr(buf,"Content-Length: ");
	content= atoi(charpoint+16);
	charpoint=strstr(buf,"\r\n\r\n");
	header = charpoint - buf +4;
	content = content - (initsize - header);
	dynamicbuf = new char[initsize+content];
	sizeofbuf=initsize+content;
	memcpy(dynamicbuf,buf,initsize);
	total=initsize;
	while(content > 0){
	  if ((newline = recv(new_fd, buf,sizeof(buf), 0))== -1){
	    perror("recv");
	    close(new_fd);
	    exit(0);
	  }
	  
	  content-=newline;
	  memcpy(dynamicbuf+total,buf,newline);
	  total+=newline;
	}
	argv2+='_';
	argv2+=urlhash(&dynamicbuf[header],initsize+content-header);
      } else {
	dynamicbuf = new char[initsize];
	memcpy(dynamicbuf,buf,initsize);
      }
      
      /* before doing anything, check to see if we already have the page 
	 This obviously only means something in the case of a GET, a POST
	 request has embedded data that does not show in the url
      */
      
      //	if(!post){
      found=0;
      page="/home/";
      page+=getenv("USER");
      page+="/localweb/";
      page+=translate(argv2.c_str());
      
      struct stat s;
      if( stat(page.c_str(),&s) == 0 ){

	if(s.st_mode & S_IFREG){
	  //it's a file
	  
	  fp.open(page.c_str());
	  
	  fp.read(buf,sizeof(buf));
	  
	  len=fp.gcount();
	  sendall(new_fd,buf,&len);
	  cacheable=is_page_cacheable(buf);
	  
	  while(!fp.fail()){
	  clock_gettime(CLOCK_REALTIME, &ts);
	    
	    fp.read(buf,sizeof(buf));
	    len=fp.gcount();
	    sendall(new_fd,buf,&len);
	  }
	  clock_gettime(CLOCK_REALTIME, &ts2);

	  if(configuration["debugbir"].compare(NODEBUG)!=0){
	    lock_file(timingfile);
	    lock_file(timingfilepre);
	    output.open(timingfile.c_str(), ios::app);
	    output << "user-local " << argv2 << " " << ts2.tv_nsec - reqmade << endl;
	    output.close();
	    output.open(timingfilepre.c_str(), ios::app);
	    output << "user-local " << argv2 << " " << ts.tv_nsec - reqmade << endl;
	    output.close();
	    unlock_file(timingfile);
	    unlock_file(timingfilepre);
	  }
	  
	  fp.close();
	  close(new_fd);
	  
	  if(movedpage==1){
	    //treat as if it were a duplicate page
	    handle_duped_page(translate(argv2.c_str()));
	    exit(1);
	  }
	  
	  lock_file(configuration["used"]);
	  output.open(configuration["used"].c_str(), ios::app);
	  output << getenv("USER") << ' ' << translate(argv2.c_str()) << endl;
	  output.close();
	  unlock_file(configuration["used"]);
	  found=1;
	  
	}
      }
      //	}
      
      /* establish a connection with the kiosk */
      if ((he=gethostbyname(argv1.c_str())) == NULL) {  // get the host info 
	herror("gethostbyname");
	cout << "This is the one that failed" << endl;
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
      
      /*send a message of type 000 (page request). The format is:
	000 pagename user
	we use sendall-recvall to ensure completness. However, recvall expects
	message lengths, hence the "size" thing
	If we have the page send a 001 if is it cacheable, 002 if it is not
      */
      
      *size=3;
      sendbuf=size;
      if(found==0){
	sendbuf+="000";
      } else {
	if(!cacheable.compare("no")){
	  sendbuf+="002";
	} else {
	  sendbuf+="001";
	}
      }
      *size=argv2.length();
      sendbuf+=size;
      sendbuf+=argv2;
      *size=strlen(getenv("USER"));
      sendbuf+=size;
      sendbuf+=getenv("USER");
      len = sendbuf.length();
      delete size;
      
      if (sendall(sockfd, sendbuf.c_str(), &len) == -1) {
	perror("sendall");
	printf("We only sent %d bytes because of the error!\n", len);
      }
      
      if (sendall(sockfd, dynamicbuf, &sizeofbuf) == -1) {
	perror("sendall");
	printf("We only sent %d bytes because of the error!\n", sizeofbuf);
      }
      
      /*get the reply*/
      len = 3;
      mysize=len;
      if (receiveall(sockfd, buf, &len, 0) == -1) {
	perror("recv");
	printf("We still have space to receive %d more bytes\n", len);
      }
      buf[mysize-len] = '\0';
      
      if(!strcmp(buf,"100")){
	/* We have not found the page. The request has been queued 
	   and we are done*/
	clock_gettime(CLOCK_REALTIME,&ts);
	sprintf(buf,"HTTP/1.1 200 OK\r\nProxy-Agent: Custom\r\nCache-Control:no-cache\r\n\r\n<h1>Please wait</h1>Your page, %s, has been sent to the city and will be ready after the next bus returns.\r\n\r\n", argv2.c_str());
	len=strlen(buf);
	sendall(new_fd, buf, &len);
	clock_gettime(CLOCK_REALTIME,&ts2);
	reqserviced=ts2.tv_nsec;
	
	if(configuration["debugbir"].compare(NODEBUG)!=0){
	  lock_file(timingfilepre);
	  lock_file(timingfile);
	  output.open(timingfile.c_str(), ios::app);
	  output << "pending " << argv2 << " " << reqserviced - reqmade << endl;
	  output.close();
	  output.open(timingfilepre.c_str(), ios::app);
	  output << "pending " << argv2 << " " << ts.tv_nsec - reqmade << endl;
	  output.close();
	  unlock_file(timingfile);
	  unlock_file(timingfilepre);
	}
	
      } else if (!strcmp(buf,"200")) {
	/*The page is somewhere on this very machine, handle the case*/
	reqserviced = handle_local_page(sockfd, configuration, argv2.c_str(), new_fd, dynamicbuf, sizeofbuf, &pretime);
	if(configuration["debugbir"].compare(NODEBUG)!=0){
	  lock_file(timingfile);
	  lock_file(timingfilepre);
	  output.open(timingfile.c_str(), ios::app);
	  output << "machine-local " << argv2 << " " << reqserviced - reqmade << endl;
	  output.close();
	  output.open(timingfilepre.c_str(), ios::app);
	  output << "machine-local " << argv2 << " " << pretime - reqmade << endl;
	  output.close();
	  unlock_file(timingfilepre);
	  unlock_file(timingfile);
	}
	
      } else if(!strcmp(buf,"201")) {
	/*We got a message telling us where the page can be found in the 
	  village use handle_remote_page to get it */
	reqserviced = handle_remote_page(sockfd,argv1.c_str(), argv2.c_str(), new_fd, dynamicbuf, sizeofbuf, &pretime);
	
	if(configuration["debugbir"].compare(NODEBUG)!=0){
	  lock_file(timingfile);
	  lock_file(timingfilepre);
	  output.open(timingfile.c_str(), ios::app);
	  output << "village " << argv2 << " " << reqserviced - reqmade << endl;
	  output.close();
	  output.open(timingfilepre.c_str(), ios::app);
	  output << "village " << argv2 << " " << pretime - reqmade << endl;
	  output.close();
	  unlock_file(timingfile);
	  unlock_file(timingfilepre);
	}
	
      } else if(!strcmp(buf,"202")) { 
	/*We have a message with the full page in it, the page was stored
	  on the kiosk */
	reqserviced = handle_page_on_kiosk(sockfd, new_fd, &pretime);
	if(configuration["debugbir"].compare(NODEBUG)!=0){
	  lock_file(timingfile);
	  lock_file(timingfilepre);
	  output.open(timingfile.c_str(), ios::app);
	  output << "kiosk " << argv2 << " " << reqserviced - reqmade << endl;
	  output.close();
	  output.open(timingfilepre.c_str(), ios::app);
	  output << "kiosk " << argv2 << " " << pretime - reqmade << endl;
	  output.close();
	  unlock_file(timingfile);
	  unlock_file(timingfilepre);
	}
	
      }else if(!strcmp(buf,"101")) { 
	/* purely informational */
      }else if(!strcmp(buf,"102")) {
	/* someone else owns the file now, delete this copy */
	handle_duped_page(translate(argv2.c_str()));
      }else {
	/*something has gone seriously wrong*/
	return 1;
      }
      delete[] dynamicbuf;
      close(sockfd);
      close(new_fd);
      
      exit(0);
    } //if (!fork())
    close(new_fd); //parent doesn't need this
  } //while(1)
  return 0;
}
