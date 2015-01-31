/*
 * socket_link.cpp
 *
 *  Created on: Nov 19, 2014
 *      Author: jmeisel
 */

#include "socket_link.h"
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/poll.h>
#include <memory>

#define MAX_RECV 1024
#define DEFAULT_TIMEOUT 12000

// Set the socket to block o rnot to block
bool fd_set_blocking(int fd, bool blocking)
{
    /* Save the current flags */
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
    {
    	throw SocketError("Problem with setting non blocking");
    }

    if (blocking)
    {
        flags &= ~O_NONBLOCK;
    }
    else
    {
        flags |= O_NONBLOCK;
    }

    return fcntl(fd, F_SETFL, flags) != -1;
}

enum TimeoutOptions
{
	SEND, RECV
};

bool fd_set_timeout(int fd, int seconds, TimeoutOptions option)
{
	int timeoutType;
	switch (option)
	{
		case SEND:
			timeoutType = SO_SNDTIMEO;
			break;
		case RECV:
			timeoutType = SO_RCVTIMEO;
			break;
	}

	struct timeval tv;
	tv.tv_sec = seconds;
	tv.tv_usec = 0;

	int ret = setsockopt(fd, SOL_SOCKET, timeoutType, &tv, sizeof tv);

	return ret >= 0;
}

/*---------------------------- SockWrapperImpl ---------------------------*/
class SockWrapper::SockWrapperImpl
{
public:
	int m_fd; // file descriptor of this socket
	int m_timeout; // timeout for connect

	void init(std::string address, std::string port, bool con)
	{
		struct addrinfo hints;
		struct addrinfo *res, *p = NULL;
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		if (m_timeout < 0)
		{
			m_timeout = DEFAULT_TIMEOUT;
		}

		char const * c_address;
		if (address.empty())
		{
			c_address = NULL;
		}
		else
		{
			c_address = address.c_str();
		}

		// You are a server or initiating connection;
		// If server, need to set the passive flag
		if (!con)
		{
			hints.ai_flags = AI_PASSIVE;
		}

		// Do dhcp and convert to right byte order with getaddrinfo
		int status;
		if ((status = getaddrinfo(c_address, port.c_str(), &hints, &res)) != 0)
		{
			throw SocketError(gai_strerror(status));
		}

		if (res == NULL)
		{
			throw SocketError("Can't perform DHCP");
		}

		p = res;

		//open the socket
		if (p != NULL && ((m_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1))
		{
			p = p->ai_next;
		}

		if (m_fd == -1)
		{
			throw SocketError("Could not open socket");
		}

		//either bind or connect
		if (!con)
		{
			int yes;
			// bind
			if (setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
			{
				throw SocketError("setsockopt");
			}

			if (bind(m_fd, p->ai_addr, p->ai_addrlen) == -1)
			{
				throw SocketError("bind");
			}

			if (listen(m_fd, 10) == -1)
			{
				throw SocketError("Cannot Listen");
			}
			freeaddrinfo(res);
		}
		else
		{
			//set nonblocking (so we can timeout)
			fd_set_blocking(m_fd, false);
			connect(m_fd, p->ai_addr, p->ai_addrlen);

			struct pollfd ufds;
			ufds.fd = m_fd;
			ufds.events = POLLOUT;

			int numchanges = poll(&ufds, 1, m_timeout);

			freeaddrinfo(res);
			fd_set_blocking(m_fd, true);

			fd_set_timeout(m_fd, m_timeout / 1000, SEND);
			fd_set_timeout(m_fd, m_timeout / 1000, RECV);

			// This all means the connection isn't successful
			if ((numchanges < 1) || (ufds.revents & POLLERR))
			{
				throw SocketError("Can't connect");
			}
		}
	}

	// Close the socket if it hasn't been already closed, and set fd to -1
	void safeClose()
	{
		if (m_fd > 0)
		{
			close(m_fd);
			m_fd = -1;
		}
	}

	// Creates wrapper given a fd (used for accepting a connection)
	SockWrapperImpl(int fd)
	: m_fd{fd}, m_timeout{-1}
	{}

	// Create wrapper for initiating connection
	SockWrapperImpl(std::string address, std::string port, int timeout)
	: m_fd{-1}, m_timeout{timeout}
	{
		try
		{
			init(address, port, true);
		}
		catch (SocketError& e)
		{
			safeClose();
			throw e;
		}
	}

	// create wrapper for socket server
	SockWrapperImpl(std::string port)
	: m_fd{-1}, m_timeout{-1}
	{
		try
		{
			init("", port, false);
		}
		catch (SocketError& e)
		{
			safeClose();
			throw e;
		}
	}

	// close socket if not already closed in destructor
	~SockWrapperImpl()
	{
		safeClose();
	}
};

/*--------------------------SockWrapper -----------------------------------*/
//Send message over the socket
int SockWrapper::sendM(std::vector<char> const & message) const
{
	if (m_impl->m_fd < 1)
	{
		throw SocketError("Socket is Closed");
	}
	int size = message.size();
	int sent = 0;

	ssize_t this_sent = -1;
	while (sent < size)
	{
		this_sent = send(m_impl->m_fd, message.data() + sent, size - sent, 0);
		if (this_sent == -1)
		{
			return -1;
		}
		sent += this_sent;
	}

	return sent;
}

// recv message over the socket
int SockWrapper::recvM(std::vector<char>& r) const
{
	return recvM(r, -1);
}

int SockWrapper::recvM(std::vector<char>& r, int max) const
{
	if (max < 0 || max > MAX_RECV)
	{
		max = MAX_RECV;
	}

	if (m_impl->m_fd < 1)
	{
		throw SocketError("Socket is Closed");
	}

	char receive[MAX_RECV];
	int numbytes;

	numbytes = recv(m_impl->m_fd, receive, max - 1, 0);

	if (numbytes < 0 || numbytes >= MAX_RECV)
	{
		return -1;
	}

	for (int i = 0; i < numbytes; i++)
	{
		r.push_back(receive[i]);
	}

	return numbytes;
}

// Create sockwrapper from a sockwrapperimpl (used for accepts)
SockWrapper::SockWrapper(std::unique_ptr<SockWrapperImpl> impl)
: m_impl{move(impl)}
{}

// Create sockwrapper for client
SockWrapper::SockWrapper(std::string address, std::string port)
: m_impl{std::unique_ptr<SockWrapperImpl>{new SockWrapperImpl{address, port, -1}}}
{}

// Create sockwrapper for client with connect timeout
SockWrapper::SockWrapper(std::string address, std::string port, int timeout)
: m_impl{std::unique_ptr<SockWrapperImpl>{new SockWrapperImpl{address, port, timeout}}}
{}

// Create sockwrapper for server (protected)
SockWrapper::SockWrapper(std::string port)
: m_impl{std::unique_ptr<SockWrapperImpl>{new SockWrapperImpl{port}}}
{}

// Copy constructor is deleted, since don't want to destruct twice; can move, though
SockWrapper::SockWrapper(SockWrapper&& nocopy)
: m_impl{move(nocopy.m_impl)}
{}

SockWrapper::~SockWrapper()
{}

void SockWrapper::closeSock()
{
	m_impl->safeClose();
}

/* --------------------------------------- Server sock constructor ----*/
SockServer::SockServer(std::string port)
: SockWrapper{port}
{ }

SockWrapper SockServer::acceptConnection()
{
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size = sizeof their_addr;

	struct sockaddr_in *sin = (struct sockaddr_in *)&their_addr;
	int new_fd = accept(m_impl->m_fd, (struct sockaddr *)sin, &sin_size);

	std::string ip = std::string(inet_ntoa(sin->sin_addr));

	//std::cout << "Receiving connection from IP: " << ip << std::endl;

	if (new_fd == -1)
	{
		throw SocketError("Couldn't accept");
	}

	fd_set_timeout(new_fd, m_impl->m_timeout / 1000, SEND);
	fd_set_timeout(new_fd, m_impl->m_timeout / 1000, RECV);

	int yes;
	if (setsockopt(new_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
	{
		throw SocketError("setsockopt");
	}

	//first, create the impl
	return SockWrapper{std::unique_ptr<SockWrapperImpl>{new SockWrapperImpl{new_fd}}};
}

void SockServer::closeSock()
{
	SockWrapper::closeSock();
}
