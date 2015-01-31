/*
 * socket_utils2.h
 *
 *  Created on: Jan 1, 2015
 *      Author: jmeisel
 */

#ifndef SOCKET_UTILS2_H_
#define SOCKET_UTILS2_H_

#include "socket_link2.h"

// Receive byte by byte until reaching "delimiter", a timeout, or hitting "maxsize"
// Will not receive any more bytes from the socket after reaching the delimiter
int recvTo(SockWrapper const & wrapper, std::vector<char>& r,
		std::vector<char> const & delimiter, int maxsize);


// Receive a string from the socket (assumed null terminated)
// If there's a timeout, or no null terminator before maxsize, all the received data is lost
std::string recvString(SockWrapper const & wrapper, int maxsize);

// Receive a string from socket (null terminated)
// If there's a timeout, or no null terminator before maxsize, received data will be in r
std::string recvString(SockWrapper const & wrapper, std::vector<char>& r, int maxsize);

// Send a string over the socket (automatically null terminates)
void sendString(SockWrapper const & wrapper, std::string const & message);

// Send a file meant to be recvd into a file
void sendFile(SockWrapper const & wrapper, std::string const & filename);
void recvFile(SockWrapper const & wrapper, std::string const & path);

// Just dump the file into a socket
void sendRawFile(SockWrapper const & wrapper, std::string const & filename);

struct SendError
{
	std::string m_message;

	SendError(std::string message)
	: m_message{message}
	{}
};


#endif /* SOCKET_UTILS2_H_ */
