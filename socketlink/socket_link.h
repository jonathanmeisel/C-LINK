/*
 * socket_link.h
 *
 *  Created on: Nov 19, 2014
 *      Author: jmeisel
 */

#ifndef CLINK2DTN_SOCKET_LINK_H_
#define CLINK2DTN_SOCKET_LINK_H_

#include <string>
#include <vector>
#include <memory>

/*
 * Class that wraps a socket.  Contains "send" and "recv" functionality.
 * Successful construction ensures that the socket is open and connected.
 * Upon destruction the socket will be closed.
 */
class SockWrapper
{
protected:
	class SockWrapperImpl;
	std::unique_ptr<SockWrapperImpl> m_impl;

	SockWrapper(std::string port); // for the server

public:
	SockWrapper(std::string address, std::string port);
	SockWrapper(std::string address, std::string port, int timeout);
	SockWrapper(SockWrapper const & copy) = delete;
	SockWrapper(SockWrapper&& nocopy);
	SockWrapper(std::unique_ptr<SockWrapperImpl> impl);
	~SockWrapper();

	int sendM(std::vector<char> const & message) const;
	int recvM(std::vector<char>& r) const;
	int recvM(std::vector<char>& r, int max) const;
	void closeSock();
};

/*
 * Class that implements a simple server.  Listens on port "port" for connections.
 * acceptConnection blocks until a connection arrives; it then returns a SockWrapper
 * for the connection
 */
class SockServer : private SockWrapper
{
public:
	SockServer(std::string port);
	SockWrapper acceptConnection();
	void closeSock();
};

class SocketError
{
public:
	SocketError(std::string error)
	: m_error{error}
	{}

	std::string m_error;
};

#endif /* CLINK2DTN_SOCKET_LINK_H_ */
