#ifndef _SENDALL_H_
#define _SEND_ALL_H

#include <string>

using namespace std;

int sendall(int s, const char *buf, int *len);
int receiveall(int s, char *buf, int *len, int flags);
int unlock_file(string name);
int lock_file(string name);

string is_page_cacheable(char *socketbuf);
int change_file_to_dir(string name);

string urlhash(const char* url, int urlsize);
#endif
