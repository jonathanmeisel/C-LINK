#ifndef _REMOTE_HANDLE_H_
#define _REMOTE_HANDLE_H_

#include <map>
#include <string>
#include "defines.h"

int handle_remote_request(int sockfd, stringtostring configuration,  map<string,int> sizes);
int handle_new_file(int new_fd, map<string,int> *usersizes, stringtostring config, stringtostringqueue LRUqueue);
map<string,int> readusersizes(stringtostring configuration, stringtostringqueue *LRU);
int changeownership(string user, stringtostring configuration, stringtostringqueue LRU, map<string,int> *usersizes);
void send_user_sizes(int sockfd, map<string,int> usersizes, int quota);
int movefiles(int sockfd, string user, stringtostringqueue LRU, stringtostring config, map<string,int> *usersizes);
int deletefiles(string user, stringtostringqueue LRU, stringtostring config);
stringtostringqueue read_LRU_file(stringtostring config);
int deletedupes(int fd, map<string,int> sizes, stringtostring config);

#endif
