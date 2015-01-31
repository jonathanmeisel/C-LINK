/*
 * socket_utils2.cpp
 *
 *  Created on: Jan 1, 2015
 *      Author: jmeisel
 */

#include "socket_utils2.h"
#include <sys/stat.h>
#include <libgen.h>
#include <fstream>
#include <iostream>
#include <string>

// Receives until the delimiter, or until maxsize
int recvTo(SockWrapper const & wrapper, std::vector<char>& r,
		std::vector<char> const & delim, int maxsize)
{
	int current = 0;
	int i;

	for (i = 0; i < maxsize; i++)
	{
		// Receive one byte at a time
		int recvd = wrapper.recvAll(r, 1);

		// Error or timeout
		if (recvd < 1)
		{
			break;
		}

		// is the byte that we just received part of the delimiter?
		if (r.at(r.size() - 1) == delim.at(current))
		{
			current++;
		}
		else
		{
			current = 0;
		}

		// We have seen all the bytes in the delimiter
		if (current >= delim.size())
		{
			break;
		}
	}
	return i;
}

// Receive a string up to length "size."  If there's a timeout, or maxsize bytes
// without proper delimiter, no string is returned but data is appened to the vector
std::string recvString(SockWrapper const & wrapper, std::vector<char>& r, int maxsize)
{
	std::vector<char> delim;
	delim.push_back('\0');
	int size = recvTo(wrapper, r, delim, maxsize);

	if (!(size > 0 || r.at(r.size() - 1) == '\0'))
	{
		return "";
	}

	try
	{
		return std::string{r.data()};
	}
	catch (...)
	{

	}
	return "";
}

// Convert message to string.  String must be null terminated.
// If there's a timeout, or maxsize bytes without the null terminator,
// all the data will be lost
std::string recvString(SockWrapper const & wrapper, int maxsize)
{
	std::vector<char> r;
	return recvString(wrapper, r, maxsize);
}

// Send a string; size is the total length of the message (must be >= string size + 1)
void sendString(SockWrapper const & wrapper, std::string const & message)
{
	std::vector<char> messageVec{message.begin(), message.end()};
	messageVec.push_back('\0');
	wrapper.sendM(messageVec);
}

// Just dump a file out over the socket
void sendRawFile(SockWrapper const & wrapper, std::string const & filesend)
{
	struct stat fileInfo;
	if (stat(filesend.c_str(), &fileInfo) == -1)
	{
		throw SendError{"Invalid File to Send"};
	}

	std::ifstream file{filesend, std::ios::binary};
	std::vector<char> sendvec;
	sendvec.resize(10000);

	while (!file.fail())
	{
		file.read(sendvec.data(), 10000);

		int thisSent = file.gcount();
		sendvec.resize(thisSent);
		wrapper.sendM(sendvec);
	}
}

// Send a file.  Protocol:
// Sender sends: "<byte_size>", "<name>"
// Receiver sends: "accept" or "reject"
// If "accept", sender sends whole file over
void sendFile(SockWrapper const & wrapper, std::string const & filesend)
{
	// Does the file exist?
	struct stat fileInfo;
	if (stat(filesend.c_str(), &fileInfo) == -1)
	{
		throw SendError{"Invalid File to Send"};
	}

	// It exists, so get the size
	//Send the name and the size
	sendString(wrapper, std::string{basename((char*) filesend.c_str())});
	sendString(wrapper, std::to_string((long long) fileInfo.st_size));

	std::string recvd = recvString(wrapper, 10);
	if (recvd.compare("accept") != 0)
	{
		throw SendError{"Receiver Will Not Accept File"};
	}

	sendRawFile(wrapper, filesend);
}

void recvFile(SockWrapper const & wrapper, std::string const & path)
{
	//Receive the header string
	std::string name = recvString(wrapper, 256);
	if (name.empty())
	{
		return;
	}

	std::string ssize = recvString(wrapper, 256);
	if (ssize.empty())
	{
		return;
	}
	long long size = std::stol(ssize);

	name = path + name;

	sendString(wrapper, "accept");

	//Now receive the file
	int this_received;
	long long total_recvd = 0LL;
	std::ofstream fileout{name, std::ios::binary};

	while (total_recvd < size)
	{
		std::vector<char> file;
		// Receive 10000 at a time, but less if there are fewer to receive
		int to_receive = 10000;
		if (size - total_recvd < 10000LL)
		{
			to_receive = (int) (size - total_recvd);
		}

		// Receive the bytes, stop if the other side closes
		this_received = wrapper.recvAll(file, to_receive);
		if (this_received <= 0)
		{
			break;
		}

		fileout.write(file.data(), file.size());
		total_recvd += this_received;
	}

	fileout.close();
}
