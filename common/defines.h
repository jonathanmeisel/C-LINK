#ifndef _DEFINES_H_
#define _DEFINES_H_

#include <string>
#include<map>
#include <deque>

#define MYPORT 3490    // the port users will be connecting to
#define FILEPORT 2903  // the port to use to do file transfers
#define FFPORT 55555   // the port to use to connect to the browser 
#define MONITORPORT "3234"

#define BACKLOG 10     // how many pending connections queue will hold
#define MAXDATASIZE 1024 // max number of bytes we can get at once 
#define MAXFILESIZE 1024 // max number of characters in a filename

#define DEFAULTKIOSKFILE ".kioskconfig" //default kiosk configuration file
#define DEFAULTUSERFILE ".userconfig"   //default user configuration file

#define DEFAULTKIOSKIP "127.0.0.1" //default address for the 
                                   //user to search for the kiosk

//default file names
#define WORKSPACE "/localweb/.workspace/"
#define DEFAULTPENDINGFILE "/localweb/.workspace/pending"     //pending pages
#define DEFAULTNEWFILESFILE "/localweb/.workspace/newfiles"   //new pages
#define DEFAULTMAPPINGFILE "/localweb/.workspace/mapping.txt" //current mappings
#define DEFAULTDELETIONFILE "/localweb/.workspace/deletion"   //deleted pages
#define DEFAULTLASTKNOWNFILE "/localweb/.workspace/lastknown" //lastknown ip
#define DEFAULTTEMPLOCFILE "/localweb/.workspace/temploc" //temparily storage
#define DEFAULTSIZEFILE "/localweb/.userspace/usersizes" //user cache use
#define DEFAULTUSERDELFILE "/localweb/.userspace/deletion" //deleted user pages
#define DEFAULTUSEDFILE "/localweb/.userspace/used" //used user pages
#define DEFAULTLRUFILE "/localweb/.userspace/LRU" //full LRU list

//default hostname of kiosk machine
#define DEFAULTHOSTNAME "jonathanmeisel"
#define DEFAULTBASEDIR "/localweb/requests" //default KioskNet directory

#define DEFAULTQUOTA "1073741824" //1 GB default storage per user

#define DEFAULTSPACETOFREE "1048576" //space to free when quota is exceeded

#define ROLLOVER 0xFFFFFFFF //rollover for checksum for posts

#define NODEBUG "NODEBUG" //default directory (or none) for debug data
#define NOCELL "NOCELL" //default directory (or none) for the cellphone to watch

enum {NOWHERE, PENDING, HERE, YOU, VILLAGE};

using namespace std;

struct waiter{
  waiter *next;
  string ipaddress;
  string user;
  string tag;
};

struct fullloc{
  bool pending;
  waiter *pendingpeople;
  string ipaddress;
  string user;
  string tag;
  string cacheable;
};

typedef map<string,fullloc> stringtofullloc;
typedef map<string,string> stringtostring;
typedef map<string,deque<string> > stringtostringqueue;

#endif
