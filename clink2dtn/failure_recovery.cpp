/*
 * failure_recovery.cpp
 *
 * This file contains functions that can be used to detect failed nodes
 * and handles logic for passing messages related to detecting and recovering from failures
 *
 *  Created on: Nov 24, 2014
 *      Author: jmeisel
 */

#include "defines.h"
#include <fstream>
#include "ProcessManager.h"
#include "failure_recovery.h"
#include <thread>
#include <memory>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <wait.h>
#include <iostream>

string KIOSK = "kiosk";
string KIOSK_KEEPALIVE = "kioskkeepalive";
string USER = "user";

string PING = "Hello";
string REQUEST_MAPPING = "map";
string REQUEST_LASTKNOWN = "lastknown";
string PROMPT = "prompt";
string GETPROMPT = "getprompt";

int string_size = 5;

int timeout = 3;

FailureRecover::FailureRecover(std::string kioskIp, bool kiosk)
: currentKiosk{kioskIp}, m_kiosk{kiosk}, prompted{false}
{}

// Returns true if the node has failed
bool FailureRecover::checkFailure()
{
        if (currentKiosk.compare("127.0.0.1") == 0)
        {
          return false;
        }
	try
	{
		SockWrapper wrapper(currentKiosk, MONITORPORT, timeout);
		sendString(wrapper, PING);
		string receive = recvString(wrapper, 20);

		if (receive.compare(KIOSK) == 0)
		{
			return false;
		}
	}
	catch (SocketError& e)
	{}
	/*catch (std::logic_error& e)
	{}*/
	return true;
}

// Checks if a non-kiosk node is up
KioskStatus FailureRecover::checkFailure(std::string ipAddr)
{
	try
	{
		SockWrapper wrapper(ipAddr, MONITORPORT, timeout);
		sendString(wrapper, PING);
		string receive = recvString(wrapper, 20);

		if (receive.compare(USER) == 0)
		{
			return NOKIOSK_ONLINE;
		}
		else if (receive.compare(KIOSK) == 0)
		{
			return KIOSK_ONLINE;
		}
	}
	catch (SocketError& e)
	{
		std::cout << e.m_message << std::endl;
	}

	return OFFLINE;
}

bool FailureRecover::requestMappingFiles()
{
	try
	{
		SockWrapper mapping{currentKiosk, MONITORPORT, timeout};
		sendString(mapping, REQUEST_MAPPING);
		recvFile(mapping, WORKSPACE);

		// Socket has been closed after receiving the first file, so need to create a new one
		SockWrapper lastknown{currentKiosk, MONITORPORT, timeout};
		sendString(lastknown, REQUEST_LASTKNOWN);
		recvFile(lastknown, WORKSPACE);
	}
	catch (SocketError& e)
	{
		return false;
	}
	return true;
}

// Handles pings and requests for the mapping files
void FailureRecover::handleMessage(SockWrapper&& wrapper)
{
	try
	{
		string message = recvString(wrapper, 20);
		if (message.compare(PING) == 0)
		{
			//string send = m_kiosk ? KIOSK : USER;
                        string send;
                        if (currentKiosk.compare("127.0.0.1") == 0)
                        {
                          send = KIOSK;
                        }
                        else
                        {
                          send = USER;
                        }
			sendString(wrapper, send);
		}
		else if (message.compare(REQUEST_LASTKNOWN) == 0)
		{
			sendFile(wrapper, DEFAULTLASTKNOWNFILE);
		}
		else if (message.compare(REQUEST_MAPPING) == 0)
		{
			sendFile(wrapper, DEFAULTMAPPINGFILE);
		}
		else if (message.compare(PROMPT) == 0)
		{
			prompted = true;
		}
        std::ofstream ifs{"out.txt", std::ios::app};

        ifs << message << std::endl;
	}
	catch (...)
	{}
}

void FailureRecover::sendPrompt(std::string ip)
{
	try
	{
		SockWrapper wrapper{ip, MONITORPORT, timeout};
		sendString(wrapper, PROMPT);
	}
	catch (...)
	{

	}
}

void FailureRecover::acceptorThread()
{
	SockServer server{MONITORPORT, timeout};
	while (true)
	{
		try
		{
			handleMessage(server.acceptConnection());
		}
		catch (...)
		{
		}
	}
}

void FailureRecover::generateConfig()
{
	std::string filename = "config";
	std::ofstream file;
	file.open(filename);
	file << "# Configuration file" << endl;
	file << "kioskip=" << currentKiosk << endl;
}
