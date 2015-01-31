/*
 * socket_link2.cpp
 *
 *  Created on: Dec 31, 2014
 *      Author: jmeisel
 */

#include "socket_link2.h"
#include <iostream>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>


#define DEFAULT_TIMEOUT 10;
#define RECV_BUFFER 10000

// Set the socket to block o rnot to block
static bool fd_set_blocking(int fd, bool blocking)
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

static bool fd_set_timeout(int fd, int seconds, TimeoutOptions option)
{
	if (seconds < 0)
	{
		return true;
	}

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

class SockWrapper::impl
{
public:
	std::string address;
	std::string port;

	int m_fd;
	int m_timeout;

	bool server;

	impl()
	: m_fd{-1}, m_timeout{-1}, server{false}
	{}

	impl(impl const & copy) = delete;

	impl(impl&& nocopy)
	: m_fd{std::move(nocopy.m_fd)}, m_timeout{std::move(nocopy.m_timeout)}, server{std::move(nocopy.server)}
	{}

	~impl()
	{
		safeClose();
	}

	void safeClose()
	{
		if (m_fd > 0)
		{
			close(m_fd);
		}
		m_fd = -1;
	}

	void init()
	{
		try
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
			if (server)
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
			if (server)
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

				int numchanges = poll(&ufds, 1, m_timeout * 1000);

				freeaddrinfo(res);
				fd_set_blocking(m_fd, true);

				fd_set_timeout(m_fd, m_timeout, SEND);
				fd_set_timeout(m_fd, m_timeout, RECV);

				// This all means the connection isn't successful
				if ((numchanges < 1) || (ufds.revents & (POLLERR | POLLHUP | POLLNVAL)))
				{
					throw SocketError("Can't connect");
				}
			}
		}
		catch (SocketError& e)
		{
			safeClose();
			throw e;
		}
	}

	void check()
	{
		if (m_fd <= 0)
		{
			throw SocketError("Attempt to use a closed socket.");
		}
	}
};

/********************* SOCKET WRAPPER *******************/

SockWrapper::SockWrapper(std::string const & address, std::string const & port, int timeout)
: m_impl{std::unique_ptr<impl>{new impl{}}}
{
	m_impl->address = address;
	m_impl->port = port;
	m_impl->m_timeout = timeout;
	m_impl->init();
}

SockWrapper::SockWrapper(SockWrapper&& wrapper)
: m_impl{std::move(wrapper.m_impl)}
{}

SockWrapper::SockWrapper()
: m_impl{std::unique_ptr<impl>{new impl{}}}
{}

SockWrapper::~SockWrapper()
{}

// Send the entire message out over the socket
int SockWrapper::sendM(std::vector<char> const & message) const
{
	m_impl->check();

	int this_sent = 0;
	int sent = 0;
	int len = message.size();

	while (sent < len)
	{
		this_sent = send(m_impl->m_fd, message.data() + sent, len - sent, 0);
		if (sent == -1)
		{
			return -1;
		}
		sent += this_sent;
	}
	return sent;
}

int SockWrapper::recvM(std::vector<char>& r) const
{
	m_impl->check();

	char receive[RECV_BUFFER];
	int numbytes = 0;

	numbytes = recv(m_impl->m_fd, receive, RECV_BUFFER, 0);

	if (numbytes < 1)
	{
		return numbytes;
	}

	r.insert(r.end(), receive, receive + numbytes);
	return numbytes;
}

int SockWrapper::recvAll(std::vector<char>& r, int size) const
{
	m_impl->check();
	int total_recvd = 0;
	int offset = r.size();
	r.resize(size + offset);
	int this_recvd = 0;

	while (total_recvd < size)
	{
		this_recvd += recv(m_impl->m_fd, r.data() + total_recvd + offset, size - total_recvd, 0);
		if (this_recvd <= 0)
		{
			return -1;
		}
		total_recvd += this_recvd;
	}
	return size;
}

std::string const & SockWrapper::getAddress() const
{
	return m_impl->address;
}

std::string const & SockWrapper::getPort() const
{
	return m_impl->port;
}

void SockWrapper::closeSock()
{
	m_impl->safeClose();
}
/********************* SOCKET SERVER *******************/

SockServer::SockServer(std::string const & port, int timeout)
: m_impl{std::unique_ptr<SockWrapper::impl>{new SockWrapper::impl{}}}
{
	signal(SIGPIPE, SIG_IGN);
	m_impl->server = true;
	m_impl->port = port;
	m_impl->m_timeout = timeout;
	m_impl->init();
}

SockServer::SockServer(SockServer&& server)
: m_impl{std::move(server.m_impl)}
{}

SockServer::~SockServer()
{}

SockWrapper SockServer::acceptConnection()
{
	m_impl->check();

	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size = sizeof their_addr;

	struct sockaddr_in *sin = (struct sockaddr_in *)&their_addr;
	int new_fd = accept(m_impl->m_fd, (struct sockaddr *)sin, &sin_size);

	if (new_fd == -1)
	{
		throw SocketError("Couldn't accept");
	}

	std::string ip = std::string(inet_ntoa(sin->sin_addr));
	std::string port = std::to_string(sin->sin_port);

	fd_set_timeout(new_fd, m_impl->m_timeout, SEND);
	fd_set_timeout(new_fd, m_impl->m_timeout, RECV);

	int yes;
	if (setsockopt(new_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
	{
		throw SocketError("setsockopt");
	}

	//Create the wrapper, impl, and populate them
	SockWrapper wrapper{};
	wrapper.m_impl = std::unique_ptr<SockWrapper::impl>{new SockWrapper::impl{}};
	wrapper.m_impl->m_fd = new_fd;
	wrapper.m_impl->address = ip;
	wrapper.m_impl->port = port;
	return std::move(wrapper);
}
