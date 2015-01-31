#ifndef _PAGE_RETREAVAL_H_
#define _PAGE_RETREAVAL_H_

#include <string>

using namespace std;

unsigned long int handle_local_page(int sock, stringtostring config, string pagename, int new_fd, char *dynamicbuf, int sizeofbuf, unsigned long int *pretime);
unsigned long int handle_remote_page(int sock, string kioskip, string pagename, int new_fd, char* dynamicbuf, int sizeofbuf, unsigned long int *pretime);
unsigned long int handle_page_on_kiosk(int kioskfd, int sockfd, unsigned long int *pretime);
unsigned long int forcerequest(string kioskip, string pagename, int new_fd, char* dynamicbuf, int sizeofbuf, unsigned long int *pretime);
unsigned long int writedata(int sockfd, string pagename, int new_fd, unsigned long int *pretime);
string translate(string name);
int handle_duped_page(string page);

#endif
