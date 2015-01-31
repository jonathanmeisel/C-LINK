/* This file contains the funtions to be used by the KIOSK SERVER*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <map>
#include <deque>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <netdb.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <list>

#include "sendall.h"
#include "defines.h"
#include "handle.h"

using namespace std;

/* handle_request - takes care of normal requests. Checks if file exists in
   village and returns the location. If it does not exist, the request is then
   made to the city.

   buf - the page being requested
   returnip - the ipaddress of the requester
   myips - kiodk daemon map mapping page names to location and pending users
   reveiveruser - the username of the requester
   lastknown - mappping of users to their last known ipaddress
   socketbuf - the data on the incoming socket, in case it needs to be requested
   int sizeofbuf - size of socketbuf
   filenames - map of filenames read in from config file

   return - the string to send back to the user, instructing user how to proceed
*/

string handle_request(char *buf, char *returnip, stringtofullloc myips, string receiveruser, stringtostring lastknown, 
		      char *socketbuf, int sizeofbuf, stringtostring filenames, stringtostringqueue temploc){
  time_t second;
  string tempuser="root";
  char sizebyte;
  string returnstring=buf;
  ofstream reqfile;
  ifstream test;
  int index=0;
  string debugfile;

  if(filenames["debugdir"].compare(NODEBUG)!=0){
    debugfile=filenames["debugdir"];
    debugfile+='/';
    debugfile+="requests";
    lock_file(debugfile);
    reqfile.open(debugfile.c_str(),ios::app);
    reqfile << receiveruser << ' ' << returnip <<  ' ' 
	    << buf << ' ' << time(NULL) << endl;
    reqfile.close();
    unlock_file(debugfile);
  }
  

  /* check if the user has moved since the last time we saw them */
  if(lastknown[receiveruser].compare(returnip)!=0){
    /* they have moved, so update the mapping (and assoicated file) */
    updatelastknown(receiveruser, returnip, filenames);
  }

  /* find the page */
  switch (whereispage(buf,returnip, myips)){
  case NOWHERE:
    /* The page doesn't exist in the village, make a request for the page.
       A request is made by writing a file into the kiosknet upload directory
       the request expected in the city is a file formated as:
       timestamp
       username.hostname
       pagename
       socketdump
    */
    write_req_file(filenames, receiveruser, buf, socketbuf, sizeofbuf);
  case PENDING:
    /* The page doesn't exist in the village but a request for the page already
       exists or has just been made (note that the above case falls here too)
       return the "pending" string and add a user to the queue
    */
    returnstring="100";
    add_pending_user(myips, buf, receiveruser, returnip, filenames);
    record(filenames,"miss");
    break;
    
  case YOU:
    /* the page exists on the machine from which the request is coming.
       return a string with the 200 code and the username of the owner
    */

    if(!myips[buf].user.compare(receiveruser)){
      //I thought YOU had it!

      lock_file(filenames["deletion"]);
      reqfile.open(filenames["deletion"].c_str(), ios::app);
      reqfile << buf << endl;
      reqfile.close();
      unlock_file(filenames["deletion"]);

      returnstring="100";
      add_pending_user(myips, buf, receiveruser, returnip, filenames);
      record(filenames,"miss");
      break;
    }


    sizebyte=myips[buf].user.length();
    returnstring="200";
    returnstring+=sizebyte;
    returnstring+=myips[buf].user;

    if(!myips[buf].cacheable.compare("no")){
      write_req_file(filenames, myips[buf].user, buf, socketbuf, sizeofbuf);
      record(filenames,"hit-nocache-you");
    } else {
      record(filenames,"hit-you");
    }
    break;

  case HERE:
    /* the page exists in temperary storage here return a 202 code */
    returnstring="202";

    if(!myips[buf].cacheable.compare("no")){
      for(stringtostringqueue::const_iterator it=temploc.begin(); 
	  it!=temploc.end(); it++){
	for(deque<string>::const_iterator it2=it->second.begin(); 
	    it2!=it->second.end(); it2++){
	  if(!it2->compare(buf)) {
	    tempuser=it->first;
	    break;
	  }
	}
	if(tempuser.compare("root")) break;
      }
      
      write_req_file(filenames, tempuser, buf, socketbuf, sizeofbuf);
      record(filenames,"hit-nocache-kiosk");
    } else {
      record(filenames,"hit-kiosk");
    }


    break;
    
  case VILLAGE:
    /* The page exists in the village on a remote machine. Return the 201 code
      as well as the ipaddress and username of the owner of the page*/

    sizebyte=myips[buf].user.length();
    returnstring="201";
    returnstring+=sizebyte;
    returnstring+=myips[buf].user;
    sizebyte=myips[buf].ipaddress.length();
    returnstring+=sizebyte;
    returnstring+=myips[buf].ipaddress;

    if(!myips[buf].cacheable.compare("no")){
      write_req_file(filenames, myips[buf].user, buf, socketbuf, sizeofbuf);
      record(filenames,"hit-nocache-village");
    } else {
      record(filenames,"hit-village");
    }

    break;
    
  default:
    /*oh dear*/
    returnstring="404";
    break;
  }

  /*return the appropriate response, as deduced above.*/
  return returnstring;
}

/*handle_force - make a request to the city for a page, regardless of whether
  the kiosk believes it to be in the village. Mark the page for deletion from
  the internal kiosk tables.

  buf - the name of the page to be fetched
  returnip - the ipaddress of the machine requesting the page
  receiveuser - the username of the requester
  myips - the mapping of the locations of pages
  socketbuf - the data on the socket since we are making a request
  sizeofbuf - the size of socketbuf
  filenames - map of filenames from config file

  return - the string to send back to the user
*/
string handle_force(char *buf, char *returnip, string receiveuser, stringtofullloc myips, char *socketbuf, int sizeofbuf, stringtostring filenames){
  
  time_t second;
  char filename[MAXFILESIZE];
  char sizebyte;
  string returnstring=buf;
  ofstream deletefile;
  ofstream reqfile;
  ifstream test;
  int index=0;

  whereispage(buf,returnip,myips); //actually just translate...

  /* open the deletions file and insert the page name to be deleted on the
     next update
  */
  lock_file(filenames["deletion"]);
  deletefile.open(filenames["deletion"].c_str(), ios::app);
  deletefile << buf << endl;
  deletefile.close();
  unlock_file(filenames["deletion"]);
  
  /* add the user as pending for this page */
  add_pending_user(myips, buf, receiveuser, returnip, filenames);

  /*
       A request is made by writing a file into the kiosknet upload directory
       the request expected in the city is a file formated as:
       timestamp
       username.hostname
       socket dump
    */

  /*second = time(NULL);   
  sprintf(filename,"%s/%s.%s/dtnweb/upload/%ld_%ld.req",
	  filenames["basedir"].c_str(),receiveuser.c_str(),
	  filenames["HOSTNAME"].c_str(),second,index++);

  test.open(filename);
  test.close();
  while(!test.fail()){
    sprintf(filename,"%s/%s.%s/dtnweb/upload/%ld_%ld.req",
	    filenames["basedir"].c_str(),receiveuser.c_str(),
	    filenames["HOSTNAME"].c_str(),second,index++);
    test.open(filename);
    test.close();
  }

  reqfile.open(filename, ios::binary);
    
  reqfile << second << '_' << --index << "\r\n" << receiveuser 
	  << '.' << filenames["HOSTNAME"] << "\r\n" << buf << "\r\n"; 
    
  reqfile.write(socketbuf, sizeofbuf);
  reqfile.close();
  */
  write_req_file(filenames, receiveuser, buf, socketbuf, sizeofbuf);
  returnstring="100";
  record(filenames,"force");

  return returnstring;
}

/* check deletion - update internal tables by reading files in the workspace 
   directory.
   files to be checked are:
   mapping.txt - contains a mapping of pagenames to ipaddresses and username
   lastknown - contains a mapping of usernames to last seen ipaddresses
   pending - contains a mapping of pagenames to users waiting for the page
   deletions - contains a list of pages to be deleted from the mapping of
               pages to ipaddresses
   newfiles - a list of newly received files from the city
   temploc - files temporarily stored in the city

   input/output:
   myips - mapping of pagename to ipaddress and penging users
   lastknown - mapping of username to last valid ipaddress
   temploc - mapping of user files temporarily in the city
   
   input:
   startup - true on initialization of kioskdaemon, false otherwise
   filenames - map of filenames from config file

*/
int checkdeletion( stringtofullloc *myips , stringtostring *lastknown, bool startup, stringtostring filenames, stringtostringqueue *temploc) {
  ifstream deletefile;
  char buf[MAXDATASIZE];
  int count=0;
  ofstream remove;
  ifstream fp;
  int found;
  int spaceloc1,spaceloc2,spaceloc3, spaceloc4;
  string str;
  waiter *nextwaiter;
  waiter *lastwaiter;
  string field1, field2, field3, field4;
  string thisip;
  


  /* on startup, read in the mapping.txt file and update myips with pagename,
     ipadress, username, and tag. Pending is set to false as it is read in
     with the pending file. Thus, pending users are NULL*/
  if(startup){
    lock_file(filenames["mapping"]);
    fp.open(filenames["mapping"].c_str());
    while (!fp.fail()) {
      getline(fp, str);
      if ((spaceloc1=str.find(' ')) > 0){
	spaceloc2=str.find(' ',spaceloc1+1);
	spaceloc3=str.find(' ',spaceloc2+1);
	spaceloc4=str.find(' ',spaceloc3+1);
	thisip=str.substr(0,spaceloc1);
	(*myips)[thisip].ipaddress = str.substr(spaceloc1+1,spaceloc2-spaceloc1-1);
	(*myips)[thisip].user = str.substr(spaceloc2+1,spaceloc3-spaceloc2-1);
	(*myips)[thisip].tag = str.substr(spaceloc3+1,spaceloc4-spaceloc3-1);
	(*myips)[thisip].cacheable = str.substr(spaceloc4+1);
	(*myips)[thisip].pending=false;
	(*myips)[thisip].pendingpeople=NULL;
      }
    }
    fp.close();
    unlock_file(filenames["mapping"]);
    (*lastknown)["root"]="root";
  }

  /* simply read in and update the last known locations of the users.
  */
  lock_file(filenames["lastknown"]);
  fp.open(filenames["lastknown"].c_str());
  while(!fp.fail()){
    getline(fp,str);
    if ((spaceloc1=str.find(' ')) > 0){
      thisip=str.substr(0,spaceloc1);
      field1=str.substr(spaceloc1+1);
      (*lastknown)[thisip]=field1;
    }
  }
  fp.close();
  unlock_file(filenames["lastknown"]);

  /*read in the pending file */
  lock_file(filenames["pending"]);
  fp.open(filenames["pending"].c_str());
  while(!fp.fail()){
    getline(fp, str);
    found=0;
    if ((spaceloc1=str.find(' ')) > 0){

      /* this is a valid line (we assume) so read in:
	 pagename ipaddress username tag
	 from the pending file. where username and ipaddress refer to the 
	 location of the requester
      */

      spaceloc2=str.find(' ',spaceloc1+1);
      spaceloc3=str.find(' ',spaceloc2+1);
      thisip=str.substr(0,spaceloc1);
      field1=str.substr(spaceloc1+1,spaceloc2-spaceloc1-1);
      field2=str.substr(spaceloc2+1,spaceloc3-spaceloc2-1);
      field3=str.substr(spaceloc3+1);

      /* if we have never seen this page before, initilize the pending list */
      if(myips->find(thisip)==myips->end()){

	(*myips)[thisip].pendingpeople=NULL;
      }

      /* if this is on the pending list, we don't know where it is
	 we need to zero everything out
      */
      (*myips)[thisip].ipaddress = "";
      (*myips)[thisip].user = "";
      (*myips)[thisip].tag = "";
      (*myips)[thisip].pending=true;

      /* traverse the linked list looking for the entry */
      lastwaiter=(*myips)[thisip].pendingpeople;
      nextwaiter=(*myips)[thisip].pendingpeople;
      while(nextwaiter!=NULL){
	if(!nextwaiter->ipaddress.compare(field1) &&
	   !nextwaiter->user.compare(field2) &&
	   !nextwaiter->tag.compare(field3))
	  {
	  found=1;
	  break;
	}
	lastwaiter=nextwaiter;
	nextwaiter=nextwaiter->next;
      }
  
      if(found==0){
	/* the user does not exist, add them as a pending user */
	nextwaiter=new waiter;
	nextwaiter->ipaddress=field1;
	nextwaiter->user=field2;
	nextwaiter->tag=field3;
	nextwaiter->next=NULL;
	
	/*correctly add them onto the end of the list
	 lastwaiter will be NULL if there are no members (i.e. this is the
	first user waiting
	*/
	if(lastwaiter==NULL){
	  (*myips)[thisip].pendingpeople=nextwaiter;
	} else {
	  lastwaiter->next=nextwaiter;
	}
      }
    }
  }

  fp.close();
  unlock_file(filenames["pending"]);

  /* take care of all files that we have been told have been deleted */
  lock_file(filenames["deletion"]);
  deletefile.open(filenames["deletion"].c_str());
  while(!deletefile.fail()){
    deletefile.getline(buf, sizeof(buf));
    if(!(*myips)[buf].pending){
      /* don't delete a file that is pending */
      myips->erase(buf);
      count++;
    }
  }
  deletefile.close();

  /*opening and closing an outfile file empties the contents */
  
  remove.open(filenames["deletion"].c_str());
  remove.close();
  unlock_file(filenames["deletion"]);

  /* Note that deletions must come before new files
     this is because if a file is in both, it have been transfered (i.e. has
     been deleted from it's old location and inserted into a new one)
  */

  /* if any new files have come in, handle them */
  lock_file(filenames["newfiles"]);
  fp.open(filenames["newfiles"].c_str());
  while(!fp.fail()){
    getline(fp,str);
    if ((spaceloc1=str.find(' ')) > 0){
      
      /* assume this is a valid line of the file:
	 pagename username tag
      */
      
      spaceloc2=str.find(' ',spaceloc1+1);
      spaceloc3=str.find(' ',spaceloc2+1);
      thisip=str.substr(0,spaceloc1);
      field1=str.substr(spaceloc1+1,spaceloc2-spaceloc1-1);
      field2=str.substr(spaceloc2+1,spaceloc3-spaceloc2-1);
      field3=str.substr(spaceloc3+1);
      
      
      if((*myips).find(thisip)==(*myips).end()){
	/* we have no entry for this page (it has come in with another page
	   and has never been requested) so we need to add it to the list
	   of pages in the village
	*/
	(*myips)[thisip].pending=false;
	(*myips)[thisip].pendingpeople=NULL;
	(*myips)[thisip].user=field1;
	(*myips)[thisip].tag=field2;
	(*myips)[thisip].ipaddress=(*lastknown)[field1];
	(*myips)[thisip].cacheable=field3;
      } else {
	/* we have seen this page before so somebody must want it */
	sendtouser(myips, thisip,field1,(*lastknown)[field1],field2, field3);
      }
    }
  }
  fp.close();
  
  
  remove.open(filenames["newfiles"].c_str());
  remove.close();
  unlock_file(filenames["newfiles"]);
  
  /* add in and files being stored in temperary storage */
  lock_file(filenames["temploc"]);
  fp.open(filenames["temploc"].c_str());
  temploc->clear();
  while(!fp.fail()){
    getline(fp,str);
    if ((spaceloc1=str.find(' ')) > 0){
      field1=str.substr(0,spaceloc1);
      field2=str.substr(spaceloc1+1);
      (*temploc)[field1].push_front(field2);
    }
  }
  fp.close();
  unlock_file(filenames["temploc"]);  
  
  if(!startup){
    /* if this is not the startup phase, rewite the mapping file incase of
       power failure
    */
    writefile(filenames,*myips, *lastknown );
  }

  return count;
}


/*writefile - write the contents of the various mappings into files

  input:
  filenames - filenames read in from configuration file
  mylocs - a mapping of pagenames and their owners
  knownloc - mapping of users and there last known ipaddresses

*/

int writefile (stringtostring filenames, stringtofullloc mylocs, stringtostring knownloc){
  ofstream fp,fp2;
  waiter *nextwaiter;

  /* iterate over the myloc mapping and write any non-pending pages into
     the mapping.txt file.
     pagename ipaddress username tag
  */
  lock_file(filenames["mapping"]);
  lock_file(filenames["pending"]);
  fp.open(filenames["mapping"].c_str());
  fp2.open(filenames["pending"].c_str());
  for(stringtofullloc::const_iterator it = mylocs.begin(); it != mylocs.end(); ++it)
    {
      if (!it->second.pending){
	fp << it->first << ' ' << it->second.ipaddress << ' ' 
	   << it->second.user << ' ' << it->second.tag << ' ' 
	   << it->second.cacheable << endl;
      } else {
	nextwaiter=it->second.pendingpeople;
	while(nextwaiter!=NULL){
	  fp2 << it->first << ' ' << nextwaiter->ipaddress << ' ' 
	      << nextwaiter->user << ' ' << nextwaiter->tag << endl;
	  nextwaiter=nextwaiter->next;
	}
      }
    }

  fp.close();
  fp2.close();
  unlock_file(filenames["mapping"]);
  unlock_file(filenames["pending"]);

  /* iterate over the knownloc mapping and write the contents to a file
     username ipaddress
  */
  lock_file(filenames["lastknown"]);
  fp.open(filenames["lastknown"].c_str());
  for(stringtostring::const_iterator it2 = knownloc.begin(); it2 != knownloc.end(); ++it2)
    {
      fp << it2->first << ' ' << it2->second << endl;
      
    }

  fp.close();
  unlock_file(filenames["lastknown"]);
  return 1;
}

/* whereispage - finds and returns the location of a page in the village
   
   input:
   buf - pagename
   returnip - ipaddress of the requester
   myips - mapping of pagename to ipadresses

   return:
   page location (one of the enum in define.h)
*/
int whereispage(char *buf, char* returnip, stringtofullloc myips) {
  stringtofullloc::iterator it;
  string translated=buf;
  int tildapos;

  /* httrack weirdness means that ~ is turned into _ */
  while((tildapos = translated.find('~'))!=string::npos){
    translated[tildapos]='_';
    buf[tildapos]='_';
  }

  /* if the page does not appear in myips, it is nowhere */
  if (myips.find(translated)==myips.end()) return NOWHERE;

  /* the page has been requested */
  if (myips[translated].pending) return PENDING;

  if (!myips[translated].ipaddress.compare("root")) return HERE;

  /* the page is located at the same ipaddress of the requestor */
  if (!strcmp(myips[translated].ipaddress.c_str(),returnip)) return YOU;
  
  /* the page is somewhere else */
  return VILLAGE;
  
}

/* add_pending_user - make sure that this user is not already waiting for
   the page and if not, add it to the pending file

   input: 
   myips - mapping of pagenames to ownders and pending users
   buf - pagename
   receiveruser - new pending user
   returnip - ipaddress of new pending user
   filenames - map of filenames read in from configuration
*/
int add_pending_user(stringtofullloc myips, char *buf, string receiveruser, string returnip, stringtostring filenames){
  ofstream pendingfile;
  waiter *nextwait;
  int found=0;
  char intbuf[MAXDATASIZE];
  
  /* traverse the linked list to make sure this user is not already on it */
  if(myips.find(buf) != myips.end()){
    nextwait=myips[buf].pendingpeople;
    while(nextwait!=NULL){
      if(!nextwait->user.compare(receiveruser)){
	/* this user is waiting already */
	found=1;
	break;
      }
      nextwait=nextwait->next;
    }
  }
  
  /* if we foudn the user in the list, return */
  if(found==1) return 1;

  /* add the user to the bottom of the pending file
     pagename ipaddress username tag
  */
  lock_file(filenames["pending"]);
  pendingfile.open(filenames["pending"].c_str(),ios::app);
  pendingfile << buf << " " << returnip << " " << receiveruser << " "
	      << time(NULL) << endl;
    
  pendingfile.close();
  unlock_file(filenames["pending"]);

  return 1;
}

/* handle_incoming - forwards a file from the kiosk to the user when it has
   arrived. If the user is not at the last known address, the file is stored
   locally

   inputs: 
   buf - the user requestiong the file
   filename - the file to be sent to the user
   ipmap - mapping of pagename to owners and pending users
   lastknown - mapping of username to lastknown ipaddress
   socketbuf - the contents of the incoming file
   filenames - map of filenames read from config file

   return:
   string to return to kiosk program
*/

string handle_incoming(char *buf, string filename, stringtofullloc ipmap, stringtostring lastknown, char *socketbuf, int sizeofbuf, stringtostring filenames){
  struct hostent *he;
  struct sockaddr_in their_addr; // connector's address information 
  int remotesockfd;
  string sendbuf, name;
  char bytesize;
  char outbuf[MAXDATASIZE];
  int len, slashpos;
  ifstream file;
  struct stat s;

  int indexpoint;
  string noindex;

  ofstream newpagefile;

  if(filenames["debugdir"].compare(NODEBUG)!=0){
    noindex=filenames["debugdir"];
    noindex+='/';
    noindex+="returns";
    lock_file(noindex);
    newpagefile.open(noindex.c_str(),ios::app);
    newpagefile << filename << ' ' << time(NULL) << ' ' << sizeofbuf << endl;
    newpagefile.close();
    unlock_file(noindex);
  }

  /* open socket to remote user */
  if ((he=gethostbyname(lastknown[buf].c_str())) == NULL) {  // get the host info 
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
  
  /* attempt to connect to last known address for user */
  if (connect(remotesockfd, (struct sockaddr *)&their_addr,
	      sizeof their_addr) == -1) {

    //bad address for the user, store locally until user returns
    sendbuf="/localweb/";
    slashpos=sendbuf.length();
    sendbuf+=filename;
    slashpos=sendbuf.find('/', slashpos+1);
    while (slashpos != string::npos) {
      name=sendbuf.substr(0,slashpos);
      if( stat(name.c_str(),&s) == 0 ){
	
	if( s.st_mode & S_IFREG ){
	  change_file_to_dir(name);
	}
      } else {
	mkdir (name.c_str(),S_IRWXU | S_IRGRP | 
	       S_IXGRP | S_IROTH | S_IXOTH);
      }
      slashpos = sendbuf.find('/', slashpos+1);
    }

    newpagefile.open(sendbuf.c_str(),ios::binary);
    newpagefile.write(socketbuf,sizeofbuf);
    newpagefile.close();

    lock_file(filenames["temploc"]);
    newpagefile.open(filenames["temploc"].c_str(), ios::app);
    newpagefile << buf << ' ' << filename << endl;
    newpagefile.close();
    unlock_file(filenames["temploc"]);

    lock_file(filenames["newfiles"]);
    newpagefile.open(filenames["newfiles"].c_str(), ios::app);
    newpagefile << filename << ' ' << "root" << ' ' << "taggoeshere"
		<< ' ' << is_page_cacheable(socketbuf) << endl;

    if((indexpoint=filename.find("/index.html"))!=string::npos){
      noindex=filename;
      noindex.erase(indexpoint);
      newpagefile << filename << ' ' << "root" << ' ' << "taggoeshere"
		  << ' ' << is_page_cacheable(socketbuf) << endl;
    }
    newpagefile.close();
    unlock_file(filenames["newfiles"]);
    

  } else {

    //successful connection to the user, send the file

    /* inform the user daemon that we have a file waiting for it with code
       200 username pagename
    */
    bytesize=3;
    sendbuf=bytesize;
    sendbuf+="200";
    bytesize=strlen(buf);
    sendbuf+=bytesize;
    sendbuf+=buf;
    bytesize=filename.length();
    sendbuf+=bytesize;
    sendbuf+=filename;
    len=sendbuf.length();

    if (sendall(remotesockfd, sendbuf.c_str(), &len) == -1) {
      perror("send");
      printf("We only sent %d bytes because of the error!\n", len);
    }

    /* socketbuf has the returned data, send it over to the client
    */
    len=sizeofbuf;
    if (sendall(remotesockfd,socketbuf,&len) == -1){
      perror("send");
      printf("We only sent %d bytes because of the error!\n", len);
    }

    lock_file(filenames["newfiles"]);
    newpagefile.open(filenames["newfiles"].c_str(), ios::app);
    newpagefile << filename << ' ' << buf << ' ' << "taggoeshere" << ' '  
		<< is_page_cacheable(socketbuf)<< endl;

    if((indexpoint=filename.find("/index.html"))!=string::npos){
      noindex=filename;
      noindex.erase(indexpoint);
      newpagefile << filename << ' ' << buf << ' ' << "taggoeshere"
		  << ' ' << is_page_cacheable(socketbuf) << endl;
    }

    newpagefile.close();
    unlock_file(filenames["newfiles"]);


  }

  close(remotesockfd);

  /* tell the kiosk program we are done (return 100) */
  return "100";
}

/* updatelastknown - write the last known ipadress for a user 
   to the lastknown file
   filenames - map of the filenames read in from the config file

   input:
   receiveruser - username
   returnip - lastknown ipaddress for the username
*/

int updatelastknown(string receiveruser,char *returnip, stringtostring filenames){
  ofstream fp;

  lock_file(filenames["lastknown"]);
  fp.open(filenames["lastknown"].c_str(), ios::app);
  fp << receiveruser << " " << returnip << endl;
  fp.close();
  unlock_file(filenames["lastknown"]);
  
  return 0;
}

/* sendtouser - send a page to each user in its pending list

   input:
   myips - mapping of pagenames to pending list and file owners
   page - pagename
   owner - owner of the page
   ip - ipaddress of the owner 
   tag - tag of the page
   cacheable - "yes" is the page is cacheable, no else
*/

int sendtouser(stringtofullloc *myips, string page,string owner,string ip,string tag, string cacheable){
  waiter *nextwaiter;
  waiter *lastwaiter;

  /* if there is no one waiting, return */
  if(!(*myips)[page].pending) return 1;
  
  /* update the map to reflect the owner and the fact that the page is
   no longer pending 
  */
  (*myips)[page].ipaddress=ip;
  (*myips)[page].user=owner;
  (*myips)[page].tag=tag;
  (*myips)[page].pending=false;
  (*myips)[page].cacheable=cacheable;

  /* walk through the linked list and send the page to each user in turn 
   after the page is sent, delete the pending user
  */
  lastwaiter=(*myips)[page].pendingpeople;
  nextwaiter=lastwaiter->next;
  while (nextwaiter!=NULL){

    if (lastwaiter->user != owner && lastwaiter->ipaddress != ip){
      // we are the owner, we have it, don't send it
      //SIBREN: I'm not sure this is what we really want to do
      //sendpagetouser(page,lastwaiter->user,lastwaiter->ipaddress, owner, ip);
    }
    delete lastwaiter;   
    lastwaiter=nextwaiter;
    nextwaiter=lastwaiter->next;
  }  

  if (lastwaiter->user != owner && lastwaiter->ipaddress != ip){
    //we are the owner, we have it, don't send it
    //SIBREN:As above, I'm not sure this is right any more
    //sendpagetouser(page,lastwaiter->user,lastwaiter->ipaddress, owner, ip);
  }
  delete lastwaiter;

  return 0;
}

/* sendpagetouser - tell a userdaemon to retreave the page from another user
   that is now the owner of the page
   
   input:
   page - name of page to send
   user - username to send the page to
   ip - ippadress to send the page to
   owner - username of the page owner
   ownerip - ipaddress of the owner of the page
*/
   
int sendpagetouser(string page, string user, string ip, string owner, string ownerip) {
  struct hostent *he;
  struct sockaddr_in their_addr; // connector's address information 
  int remotesockfd;

  int len;

  string sendbuf;
  char bytesize;

  /* set up a socket on which to connect */

  if ((he=gethostbyname(ip.c_str())) == NULL) {  // get the host info 
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
  

  /* connect to the user if possible */
  if (connect(remotesockfd, (struct sockaddr *)&their_addr,
	      sizeof their_addr) == -1) {
    //bad address for the user, add to the user's personal pending list;

    /* SIBREN: take care of this case...we don't have a personal pending list*/
  } else {
    
    /* send a message to the userdaemon telling it to get the page from the
       owner. Send a message formated as:
       120 username pagename ownerusername owneripaddress
    */
    bytesize=3;
    sendbuf=bytesize;
    sendbuf+="120";
    bytesize=user.length();
    sendbuf+=bytesize;
    sendbuf+=user;
    bytesize=page.length();
    sendbuf+=bytesize;
    sendbuf+=page;
    bytesize=owner.length();
    sendbuf+=bytesize;
    sendbuf+=owner;
    bytesize=ownerip.length();
    sendbuf+=bytesize;
    sendbuf+=ownerip;
    len=sendbuf.length();

    if (sendall(remotesockfd, sendbuf.c_str(), &len) == -1) {
      perror("sendall");
      printf("We only sent %d bytes because of the error!\n", len);
    }
    close(remotesockfd);
  }

  return 0;

}

/*send_local_file - send a file stored on the kiosk to a user, but only for
  1 time use

  input:
  sockfd - the socket number for the user machine
  buf - the page name to send
*/
int send_local_file(int sockfd, char *buf){
  string directory;
  char sendbuf[MAXDATASIZE];
  int lengthtosend;
  ifstream file;

  directory="/localweb/";
  directory+=buf;
  struct stat s;
  if( stat(directory.c_str(),&s) == 0 ){
	    
    if( s.st_mode & S_IFDIR ){
      directory+="/index.html";
    }
    
    
    file.open(directory.c_str());
    if(!file) return 0;

    while(!file.fail()){
      file.read(sendbuf,sizeof(sendbuf));
      lengthtosend=file.gcount();
      sendall(sockfd,sendbuf,&lengthtosend);
    }

    file.close();
    return 1;
  } else {
    return 0;
  }
  return -1;
}

int send_temp_storage(stringtostringqueue pagequeue,string requsr,stringtostring filenames, string ipaddress){
  
  struct hostent *he;
  struct sockaddr_in their_addr; // connector's address information
  int sockfd;  // listen on sock_fd
  char *headerend, *cache;
  string cacheable="yes";

  char buf[MAXDATASIZE];
  char bytesize;
  int len;
  deque<string> stringqueue=pagequeue[requsr];
  //deque<string>::iterator it=stringqueue.begin();

  string sendbuf;

  struct stat s;

  ifstream file;
  ofstream outfiles;

  while (!stringqueue.empty())
  {
    /* open socket to remote user */
    if ((he=gethostbyname(ipaddress.c_str())) == NULL) {  // get the host info 
      herror("gethostbyname");
      exit(1);
    }
    
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
      perror("socket");
      exit(1);
    }
    
    their_addr.sin_family = AF_INET;    // host byte order 
    their_addr.sin_port = htons(FILEPORT);  // short, network byte order 
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(their_addr.sin_zero, '\0', sizeof their_addr.sin_zero);
    
    /* attempt to connect to last known address for user */
    connect(sockfd, (struct sockaddr *)&their_addr,
	    sizeof their_addr);

    bytesize=3;
    sendbuf=bytesize;
    sendbuf+="200";
    bytesize=requsr.length();
    sendbuf+=bytesize;
    sendbuf+=requsr;
    bytesize=stringqueue.front().length();
    sendbuf+=bytesize;
    sendbuf+=stringqueue.front();
    len=sendbuf.length();
    
    if (sendall(sockfd, sendbuf.c_str(), &len) == -1) {
      perror("send");
      printf("We only sent %d bytes because of the error!\n", len);
    }

 
    sendbuf="/localweb/";
    sendbuf+=stringqueue.front();

    if( stat(sendbuf.c_str(),&s) == 0 ){
      
      if( s.st_mode & S_IFDIR ){
	sendbuf+="/index.html";
      }
    }

    file.open(sendbuf.c_str());
 
    file.read(buf, sizeof(buf));
    len=file.gcount();

    cacheable=is_page_cacheable(buf);

    sendall(sockfd,buf,&len);

    while(!file.fail()){
      file.read(buf, sizeof(buf));
      len=file.gcount();
      sendall(sockfd,buf,&len);
    }
    file.close();
    remove(sendbuf.c_str());

    lock_file(filenames["deletion"]);
    outfiles.open(filenames["deletion"].c_str(), ios::app);
    outfiles << stringqueue.front() << endl;
    outfiles.close();
    unlock_file(filenames["deletion"]);

    lock_file(filenames["newfiles"]);
    outfiles.open(filenames["newfiles"].c_str(), ios::app);
    outfiles << stringqueue.front() << ' ' << requsr << ' ' 
	     << "taggoeshere" << ' ' << cacheable << endl;
    outfiles.close();
    unlock_file(filenames["newfiles"]);

    stringqueue.pop_front();
    close(sockfd);
    
  }
  pagequeue.erase(requsr);

  lock_file(filenames["temploc"]);
  outfiles.open(filenames["temploc"].c_str());
  for(stringtostringqueue::const_iterator it2 = pagequeue.begin(); it2 != pagequeue.end(); ++it2)
    {
      deque<string>::const_iterator it = it2->second.begin();
      while (it != it2->second.end()){
	outfiles << it2->first << ' ' << *it++ << endl;
      }
      
    }
  outfiles.close();
  unlock_file(filenames["temploc"]);
  
  return 1;
}


string handle_switch(string user, string freespace, stringtostring lastknown, stringtostring filenames, string currip){
  struct hostent *he;
  struct sockaddr_in their_addr; // connector's address information
  int sockfd;  // listen on sock_fd
  list<string> iplist;
  int len,mysize,count,smallsize=0,currsize;
  char bytesize;
  string sendbuf,smallusr, smallip;

  string currusr;
  
  char buf[MAXDATASIZE];


  for(stringtostring::const_iterator it=lastknown.begin();it!=lastknown.end();
      it++){
    iplist.push_front(it->second);
  }
  iplist.sort();
  iplist.unique();
  
  for(list<string>::const_iterator it2=iplist.begin();it2!=iplist.end();it2++){
    if(!(*it2).compare("root"))continue;
    if ((he=gethostbyname((*it2).c_str())) == NULL) {  // get the host info 
      herror("gethostbyname");
      exit(1);
    }
    
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
      perror("socket");
      exit(1);
    }
    
    their_addr.sin_family = AF_INET;    // host byte order 
    their_addr.sin_port = htons(FILEPORT);  // short, network byte order 
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(their_addr.sin_zero, '\0', sizeof their_addr.sin_zero);
    
    /* attempt to connect to last known address for user */
    connect(sockfd, (struct sockaddr *)&their_addr,
	    sizeof their_addr);

    bytesize=3;
    sendbuf=bytesize;
    sendbuf+="301";
    len=sendbuf.length();
    if(sendall(sockfd,sendbuf.c_str(),&len)==-1){
      perror("send");
      printf("We were only able to receive %d bytes\n",len);
    }


    len=sizeof(buf)-1;
    mysize=len;
    receiveall(sockfd,buf,&len,0);
    buf[mysize-len]='\0';

    count=atoi(buf);

    while(count > 0){
      len=sizeof(buf)-1;
      mysize=len;
      receiveall(sockfd,buf,&len,0);
      buf[mysize-len]='\0';
      currusr=buf;
    
      len=sizeof(buf)-1;
      mysize=len;
      receiveall(sockfd,buf,&len,0);
      buf[mysize-len]='\0';
      currsize=atoi(buf);
    
      if(currsize>smallsize){
	smallsize=currsize;
	smallip=(*it2).compare(currip)?*it2:"you";
	smallusr=currusr;
      }
      
      count--;
    }

  }

  if(smallsize<atoi(freespace.c_str())){
    sendbuf="304";
  } else {
    sendbuf="302";
    bytesize=smallusr.length();
    sendbuf+=bytesize;
    sendbuf+=smallusr;
    bytesize=smallip.length();
    sendbuf+=bytesize;
    sendbuf+=smallip;
  }

  return sendbuf;
 
}

void changeowner(int sockfd, stringtostring config){
  char buf[MAXDATASIZE];
  int len, mysize;
  string page, user;
  ofstream file;
  
  len=3;
  mysize=len;
  if (receiveall(sockfd, buf, &len, 0) == -1) {
    perror("recv");
    exit(1);
  }
  buf[mysize-len]='\0';

  while(!strcmp(buf,"305")){
    len=sizeof(buf)-1;
    mysize=len;
    if (receiveall(sockfd, buf, &len, 0) == -1) {
      perror("recv");
      exit(1);
    }
    buf[mysize-len]='\0';

    user=buf;
    len=sizeof(buf)-1;
    mysize=len;
    if (receiveall(sockfd, buf, &len, 0) == -1) {
      perror("recv");
      exit(1);
    }
    buf[mysize-len]='\0';

    page=buf;
    lock_file(config["deletion"]);
    file.open(config["deletion"].c_str(),ios::app);
    file << page << endl;
    file.close();
    unlock_file(config["deletion"]);

    lock_file(config["newfiles"]);
    file.open(config["newfiles"].c_str(),ios::app);
    file << page << ' ' << user << ' ' << "taggoeshere" << endl;
    file.close();
    unlock_file(config["newfiles"]);

    len=3;
    mysize=len;
    if (receiveall(sockfd, buf, &len, 0) == -1) {
      perror("recv");
      exit(1);
    }
    buf[mysize-len]='\0';
  }
}

string handle_info(char *page, char *ip, string user, stringtofullloc myips, stringtostring filenames, char *socketbuf, int sizeofbuf, string cacheable, stringtostring lastknown){

  string returnstring;
  ofstream output;
  string owner;
  string debugfile;

  if(filenames["debugdir"].compare(NODEBUG)!=0){
    debugfile=filenames["debugdir"];
    debugfile+='/';
    debugfile+="requests";
    lock_file(debugfile);
    output.open(debugfile.c_str(),ios::app);
    output << user << ' ' << ip <<  ' ' << page << ' ' << time(NULL) << endl;
    output.close();
    unlock_file(debugfile);
  }

  /* check if the user has moved since the last time we saw them */
  if(lastknown[user].compare(ip)!=0){
    /* they have moved, so update the mapping (and assoicated file) */
    updatelastknown(user, ip, filenames);
  }


  switch(whereispage(page,ip, myips)){
  case NOWHERE:
  case PENDING:
    lock_file(filenames["newfiles"]);
    output.open(filenames["newfiles"].c_str(), ios::app);
    output << page << ' ' << user << ' ' << "taggoeshere" << ' '
	   << cacheable << endl;
    output.close();
    unlock_file(filenames["newfiles"]);
    owner=user;
    returnstring="101";
    break;
  case YOU:
    owner=myips[page].user;
    if(!owner.compare(user)){
      returnstring="101";
      break;
    }
  case VILLAGE:
  case HERE:
    owner=myips[page].user;
    returnstring="102";
    break;
  default:
    returnstring="404";
    break;
  }

  if(!cacheable.compare("no")){
    write_req_file(filenames, owner, page, socketbuf, sizeofbuf);
    record(filenames,"hit-nocache-local");
  } else {
    record(filenames,"hit-local");
  }

  return returnstring;
}

// Need to modify this function to write out to the DTN server
int write_req_file(stringtostring filenames, string receiveruser, string buf, char *socketbuf, int sizeofbuf){
  int second, index=0;
  char filename[MAXDATASIZE], realname[MAXDATASIZE];
  ofstream reqfile;
  char basedir[MAXDATASIZE];
  struct stat s, s2;

  ifstream test, test2;
  
  second = time(NULL);

  if(!filenames["celldir"].compare(NOCELL)){
    sprintf(basedir,"%s",filenames["basedir"].c_str());
  } else {
    sprintf(basedir, "%s", filenames["celldir"].c_str());
  }
    
  sprintf(filename,"%s/dtnweb/%ld_%ld_%ld.req",basedir,second,index, getpid());
  sprintf(realname,"%s/dtnweb/upload/%ld_%ld_%ld.req",basedir,second,index++, getpid());

  cout << "the filename is" << filename << endl;

   while ((stat(filename,&s) == 0) || (stat(realname,&s2)==0)){
     sprintf(filename,"%s/dtnweb/%ld_%ld_%ld.req", basedir,second,index,getpid());
     sprintf(realname,"%s/dtnweb/upload/%ld_%ld_%ld.req", basedir,second,index++,getpid());
  } 

  reqfile.open(filename, ios::binary);
    
  reqfile << second << '_' << --index << '_' << getpid() 
	  << "\r\n" << receiveruser 
	  << '.' << filenames["HOSTNAME"] << "\r\n" << buf << "\r\n"; 
  
  reqfile.write(socketbuf, sizeofbuf);
  reqfile.close();

  rename(filename,realname);
  

  // Right here, send the file out over dtn
  //send_over_dtn(filename);

  return 1;
  
}

int record(stringtostring config,string file){
  
  string fullfilename;
  ofstream output;
  ifstream input;
  string fileline;

  if (!config["debugdir"].compare(NODEBUG)) return 1;

  fullfilename=config["debugdir"];
  fullfilename+='/';
  fullfilename+=file;

  lock_file(fullfilename);
  input.open(fullfilename.c_str());
  if(!input){
    fileline="0";
  } else {
    getline(input,fileline);
  }
  input.close();
  output.open(fullfilename.c_str());
  output << atoi(fileline.c_str())+1 << endl;
  output.close();
  unlock_file(fullfilename);

  return 1;
}
  
  
