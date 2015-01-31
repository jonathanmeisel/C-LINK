/*
 * socket_link2.h
 *
 *  Created on: Dec 31, 2014
 *      Author: jmeisel
 */

#ifndef SOCKET_LINK2_H_
#define SOCKET_LINK2_H_

#include <string>
#include <vector>
#include <memory>

class SockServer;

class SockWrapper
{
public:
	friend class SockServer;

	SockWrapper(std::string const & address, std::string const & port, int timeout);
	SockWrapper(SockWrapper const & wrapper) = delete;
	SockWrapper(SockWrapper&& wrapper);
	~SockWrapper();

	// Send, recv
	int recvM(std::vector<char>& recv) const;
	int recvAll(std::vector<char>& recv, int size) const;
	int sendM(std::vector<char> const & message) const;

	// Get the address and port of this wrapper
	std::string const & getAddress() const;
	std::string const & getPort() const;

	void closeSock();

private:
	struct impl;
	std::unique_ptr<impl> m_impl;
	SockWrapper();
};

class SockServer
{
public:
	SockServer(std::string const & port, int timeout);
	SockServer(SockServer const & server) = delete;
	SockServer(SockServer&& server);
	~SockServer();

	SockWrapper acceptConnection();
private:
	std::unique_ptr<SockWrapper::impl> m_impl;
};

struct SocketError
{
	std::string m_message;

	SocketError(std::string const & message)
	: m_message{message}
	{}
};


#endif /* SOCKET_LINK2_H_ */
