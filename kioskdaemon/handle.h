#ifndef _HANDLE_H_
#define _HANDLE_H_

#include <map>
#include <string>
#include <deque>
#include "defines.h"

using namespace std;

string handle_request(char *buf, char *returnip, stringtofullloc myips, string receiveruser, stringtostring lastknown, char *socketbuf, int sizeofbuf, stringtostring filenames, stringtostringqueue temploc);

int writefile (stringtostring filenames, stringtofullloc mylocs, stringtostring known);
string handle_force(char *buf, char *returnip, string receiveuser, stringtofullloc myips, char *socketbuf, int sizeofbuf, stringtostring filenames);
int checkdeletion( stringtofullloc *myips, stringtostring *lastknown, bool startup, stringtostring filenames, stringtostringqueue *temploc);
int add_pending_user(stringtofullloc myips, char *buf, string receiveuser, string returnip, stringtostring filenames);
int whereispage(char *buf, char *returnip, stringtofullloc myips);
string handle_incoming(char *buf, string filename, stringtofullloc ipmap, stringtostring lastknown, char *socketbuf, int sizeofbuf, stringtostring filenames);
int updatelastknown(string receiveruser,char *returnip, stringtostring filenames);

int sendtouser(stringtofullloc *myips, string page,string owner,string ip,string tag, string cacheable);

int sendpagetouser(string page, string user, string ip, string owner, string ownerip);
int send_local_file(int sockfd, char *buf);
int send_temp_storage(stringtostringqueue pagequeue,string requsr,stringtostring filenames, string ipaddress);
string handle_switch(string user, string freespace, stringtostring lastknown, stringtostring filenames, string currip);
void changeowner(int sockfd, stringtostring config);
string handle_info(char *page, char *ip, string user, stringtofullloc myips, stringtostring filenames, char *socketbuf, int sizeofbuf, string cacheable, stringtostring lastknown);
int write_req_file(stringtostring filenames, string receiveruser, string buf, char *socketbuf, int sizeofbuf);
int record(stringtostring config,string file);

#endif
