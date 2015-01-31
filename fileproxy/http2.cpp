/*
 * httpresponse.cpp
 *
 *  Created on: Dec 15, 2014
 *      Author: jonathanmeisel
 */
#include <iostream>
#include "http.h"
#include "string.h"

#define MIN_REQ_LEN 4
#define MAX_REQ_LEN 65535

// Utility method to unparse every header into the buffer
static void unparseHeaders(std::vector<char>& unparsed, std::vector<std::shared_ptr<HttpHeader>>& headers)
{
	for (std::shared_ptr<HttpHeader> header : headers)
	{
		std::string line = header->m_key + ": " + header->m_value;
		std::copy(line.begin(), line.end(), back_inserter(unparsed));
		unparsed.push_back('\r');
		unparsed.push_back('\n');
	}
}

HttpHeader parseHeader(char * header)
{
	std::string key;
	std::string value;

	if (header == NULL)
	{
		throw HttpException{"Cannot Parse Headers"};
	}

	char * cvalue = NULL;
	char * ckey = strtok_r(header, ": ", &cvalue);

	if (ckey == NULL || cvalue == NULL)
	{
		throw HttpException{"Cannot Parse Headers"};
	}

	cvalue++;

	key = std::string{ckey};
	value = std::string{cvalue};

	return HttpHeader{key, value};
}

/********************** HttpWrapper Functions *********************/
HttpWrapper::HttpWrapper()
{}

HttpWrapper::~HttpWrapper()
{}

void HttpWrapper::addHeader(HttpHeader header)
{
	std::shared_ptr<HttpHeader> existingHeader = getHeader(header.m_key);
	if (existingHeader != nullptr)
	{
		existingHeader->m_value = header.m_value;
	}
	else
	{
		m_headers.push_back(std::shared_ptr<HttpHeader>{new HttpHeader{header}});
	}
}

std::vector<char> HttpWrapper::unparse()
{
	std::vector<char> unparsed;

	// Get the first line of the header
	std::string firstLine = unparseFirstLine();
	std::copy(firstLine.begin(), firstLine.end(), back_inserter(unparsed));

	unparsed.push_back('\r');
	unparsed.push_back('\n');

	// Unparse all of the headers
	unparseHeaders(unparsed, m_headers);

	//Finish the request
	unparsed.push_back('\r');
	unparsed.push_back('\n');

	// Copy anything that's in the body
	std::copy(m_body.begin(), m_body.end(), back_inserter(unparsed));

	return unparsed;
}

std::shared_ptr<HttpHeader>  HttpWrapper::getHeader(std::string headerName)
{
	std::vector<std::shared_ptr<HttpHeader>>::iterator header;
	for (header = m_headers.begin(); header != m_headers.end(); ++header)
	{
		if ((*header)->m_key.compare(headerName) == 0)
		{
			return *header;
		}
	}
	return nullptr;
}

/************************ Functions for HttpResponse class ************/

HttpResponse::HttpResponse(ResponseType type, ResponseStatus status)
: HttpWrapper{}, m_type{type}, m_status{status}
{}

std::string HttpResponse::unparseFirstLine()
{
	std::string status;
	std::string statusNum;

	switch(m_status)
	{
	case OK:
		status = "OK";
		statusNum = "200";
		break;
	case NOT_FOUND:
		status = "Not Found";
		statusNum = "404";
		break;
	case LENGTH_REQUIRED:
		status = "Bad HTTP request: length is required";
		statusNum = "404";
	}

	std::string contentType;

	switch(m_type)
	{
	case TEXT:
		contentType = "text/plain";
		break;
	case HTML:
		contentType = "text/html";
		break;
	case IMAGE:
		contentType = "image/jpeg";
		break;
	case JAVASCRIPT:
		contentType = "text/javascript";
		break;
	case CSS:
		contentType = "text/css";
		break;
	}

	addHeader(HttpHeader{"Content-Type", contentType});

	std::string headerline = "HTTP/1.1 " + statusNum + " " + status;
	return headerline;
}

/****************** Functions for HttpRequest class ***************************/

HttpRequest::HttpRequest(RequestType type, std::string host, std::string path)
: HttpWrapper{}, m_host{host}, m_path{path}, m_type{type}, headerlen{-1}
{
	addHeader(HttpHeader{"Host", m_host});
}

std::string HttpRequest::unparseFirstLine()
{
	std::string method;

	switch (m_type)
	{
	case GET:
		method = "GET";
		break;
	case POST:
		method = "POST";
		break;
	}

	std::string firstline = method + " " + m_path + " HTTP/1.1";
	return firstline;
}

void HttpRequest::changeHost(std::string host)
{
	// Change the host instance variable
	m_host = host;

	// Also change the host header
	std::shared_ptr<HttpHeader> hostHeader = getHeader("Host");
	if (hostHeader == nullptr)
	{
		return;
	}
	hostHeader->m_value = host;
}

HttpRequest parseRequest(std::vector<char> parse, bool proxy)
{
	if (parse.size() < MIN_REQ_LEN || parse.size() > MAX_REQ_LEN)
	{
		throw HttpException{"Invalid Length"};
	}

	// Make it null terminated
	parse.push_back('\0');
	char * buf = parse.data();

	char * index = strstr(buf, "\r\n\r\n");
	if (index == NULL)
	{
		throw HttpException{"Invalid Request Line, no end of header"};
	}

	int len = index - buf;

	// This is where the headers start
	char * headers;

	// Now parse out the request line
	char * headerline = strtok_r(buf, "\r\n", &headers);

	char * next;
	char * method = strtok_r(buf, " ", &next);

	if (method == NULL)
	{
		throw HttpException{"Cannot Parse Method"};
	}

	RequestType type;

	if (strcmp(method, "GET") == 0)
	{
		type = GET;
	}
	else if (strcmp(method, "POST") == 0)
	{
		type = POST;
	}
	else
	{
		throw HttpException{"Invalid Request Type"};
	}

	std::string path = "";
	std::string host = "";

	char * cpath = strtok_r(NULL, " ", &next);
	if (cpath == NULL || next == NULL)
	{
		throw HttpException{"Cannot Parse Path"};
	}

	if (!proxy)
	{
		path += std::string{cpath};
		char * protocol = next;
		if (strcmp(protocol, "HTTP/1.1") != 0)
		{
			throw HttpException{"Invalid Protocol"};
		}
	}
	else
	{
		strtok_r(cpath, "://", &next);

		if (next == NULL)
		{
			throw HttpException{"Cannot Parse Path"};
		}

		cpath = next + strlen("://") - 1;

		if (cpath - buf >= len)
		{
			throw HttpException{"Cannot Parse Path"};
		}

		// Get the host
		char * chost = strtok_r(cpath, "/", &next);
		if (chost == NULL)
		{
			throw HttpException{"No host"};
		}

		host = std::string{chost};

		// Now get the path
		cpath = next;
		path += "/";
		if (cpath != NULL)
		{
			path += cpath;
		}
	}

	HttpRequest request{type, host, path};
	request.headerlen = len;

	// Now, need to parse the headers
	char * cheader = strtok_r(NULL, "\r\n", &headers);
	headers++;
	while (cheader != NULL)
	{
		HttpHeader header = parseHeader(cheader);
		request.addHeader(header);
		cheader = strtok_r(NULL, "\r\n", &headers);
		headers++;
	}

	if (!proxy)
	{
		std::shared_ptr<HttpHeader> hostHeader = request.getHeader("Host");
		if (hostHeader == nullptr)
		{
			throw HttpException{"No Host Header"};
		}
		request.m_host = hostHeader->m_value;
	}
	return request;
}

/****************** Functions for HttpHeader class *****************************/

HttpHeader::HttpHeader(std::string key, std::string value)
: m_key{key}, m_value{value}
{}
