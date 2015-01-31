/*
 * httpresponse.h
 *
 *  Created on: Dec 15, 2014
 *      Author: jonathanmeisel
 */
#include <string>
#include <vector>
#include <memory>
#include <iostream>

enum ResponseType
{
	TEXT, IMAGE, HTML, JAVASCRIPT, CSS
};

enum ResponseStatus
{
	OK, NOT_FOUND
};

enum RequestType
{
	GET, POST
};

struct HttpHeader
{
	std::string m_key;
	std::string m_value;
	HttpHeader(std::string key, std::string value);
};

struct HttpWrapper
{
	std::vector<std::shared_ptr<HttpHeader>> m_headers;
	std::vector<char> m_body;

	HttpWrapper();
	virtual ~HttpWrapper();

	void addHeader(HttpHeader header); // add a header
	std::shared_ptr<HttpHeader> getHeader(std::string headerName);
	void removeHeader(std::string headerName);

	std::vector<char> unparse(); // unparse into buffer
	virtual std::string unparseFirstLine() = 0;
};

struct HttpResponse
: public HttpWrapper
{
	ResponseType m_type;
	ResponseStatus m_status;

	HttpResponse(ResponseType type, ResponseStatus status);
	virtual std::string unparseFirstLine();
};

struct HttpRequest
: public HttpWrapper
{
	RequestType m_type;
	std::string m_path;
	std::string m_host;
	int headerlen;

	HttpRequest(RequestType type, std::string host, std::string path);
	virtual std::string unparseFirstLine();

	void parse(std::vector<char> buf, bool proxy);
	void changeHost(std::string host);
};

struct HttpException
{
	std::string m_cause;
	HttpException(std::string cause)
	: m_cause{cause}
	{}
};

HttpRequest parseRequest(std::vector<char> parse, bool proxy);

