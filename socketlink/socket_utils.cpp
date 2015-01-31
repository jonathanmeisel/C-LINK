/*
 * socket_utils.cpp
 *
 *  Created on: Nov 24, 2014
 *      Author: jmeisel
 */

#include "socket_utils.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#define BUF_SIZE 1024

std::vector<char> convert(std::string const & copy)
{
	std::vector<char> message(copy.begin(), copy.end());
	message.push_back('\0');
	return message;
}

void sendString(std::string message, SockWrapper const & sock)
{
	std::vector<char> vMessage = convert(message);
	sock.sendM(vMessage);
}

std::string recvString(int size, SockWrapper const & sock)
{
	int s = 0;
	std::vector<char> ret;

	while (ret.size() < size)
	{
		std::vector<char> message;
		sock.recvM(message, size);
		s += message.size();
		if (message.size() == 0)
		{
			break;
		}
		std::copy(message.begin(), message.end(), std::back_inserter(ret));
	}
	return std::string(ret.data());
}

void sendFileM(std::string filename, SockWrapper const & sock)
{
	char buffer[BUF_SIZE];

	std::vector<char> send;

	std::ifstream stream(filename, std::ios::binary);

	while (!stream.fail())
	{
		stream.read(buffer, BUF_SIZE);
		int numread = stream.gcount();

		for (int i = 0; i < numread; i++)
		{
			send.push_back(buffer[i]);
		}
		sock.sendM(send);
		send.clear();
	}
}

int recvFileM(std::string const & recvFileName, SockWrapper const & sock)
{
	std::ofstream stream(recvFileName, std::ios::binary);
	std::vector<char> recvFile;

	int size = 0;

	while (sock.recvM(recvFile) > 0)
	{
		stream.write(recvFile.data(), recvFile.size());
		size += recvFile.size();
		recvFile.clear();
	}
	return size;
}


