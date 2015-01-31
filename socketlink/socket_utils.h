/*
 * socket_utils.h
 *
 *  Created on: Nov 24, 2014
 *      Author: jmeisel
 */

#ifndef SOCKET_UTILS_H_
#define SOCKET_UTILS_H_

#include "socket_link.h"

std::vector<char> convert(std::string const & copy);
void sendFileM(std::string filename, SockWrapper const & sock);
int recvFileM(std::string const & recvFileName, SockWrapper const & sock);
void sendString(std::string message, SockWrapper const & sock);
std::string recvString(int maxSize, SockWrapper const & sock);

#endif /* SOCKET_UTILS_H_ */
