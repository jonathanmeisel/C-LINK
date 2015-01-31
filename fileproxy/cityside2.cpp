/* This is the city side */
#include <string>
#include <iostream>
#include <fstream>
#include "socket_link2.h"
#include "socket_utils2.h"
#include "http.h"
#include "../clink2dtn/ProcessManager.h"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include "clink2dtnUtils.h"
#include "sendall.h"

string IP = "127.0.0.1";
string PORT = "8118";
int TIMEOUT = 10000;

// Gets file based on the parsed request; gets links for scraping, if request
void getFile(string outputfiledir, string outputfilename, string username, string hash, HttpRequest& request, ProcessManager& pm, bool link)
{
	// Write out the meta file
	string metafilename = outputfilename + ".desc";
	string metafilepath = outputfiledir + "/" + metafilename;
	ofstream metafile(metafilepath);
	metafile.write(username.c_str(), strlen(username.c_str()));
	metafile.put('\n');
	metafile.close();

	// Open connection to HTTP proxy
	SockWrapper connection(IP, PORT, TIMEOUT);
	connection.sendM(request.unparse());

	string outputfilefullpath = outputfiledir + "/" + outputfilename;
	ofstream outputfile(outputfilefullpath);
	outputfile.write(hash.c_str(), strlen(hash.c_str()));
	outputfile.put('\n');

	// Write out response to output file
	while (true)
	{
		std::vector<char> response;
		int numbytes = connection.recvM(response);
		if (numbytes < 1)
		{
			break;
		}
		outputfile.write(response.data(), response.size());
	}
	outputfile.close();

	// Create tar file with the .desc and output
	string command = "tar --create --file=" + outputfilename + ".tar " + outputfilename + " " + outputfilename + ".desc";
	pm.startNewCommand(command, true, outputfiledir);

	// Call python script to parse out the links
	if (link)
	{
		// Get the host / path from the parsed header
		string hoststring = request.m_host;
		string pathstring = request.m_path;

		string base_url = hoststring + pathstring;

	    // go down a level and scrape
	    string outputfilepath = outputfiledir + "/" + outputfilename;
	    string command = "python2 fileproxy/scrape.py " + outputfilepath + " " + outputfilepath + ".link " + base_url;
	    pm.startNewCommand(command, true);
	}

	// Remove all but the tar files
	command = "rm " + outputfilename + " " + metafilename;
	pm.startNewCommand(command, true, outputfiledir);
}

void evaluateLinks(string linkfilename, HttpRequest parsed, string outputfilename, string outputfiledir, string username, ProcessManager& pm)
{
	//Now read the links and get them
	ifstream linkfile;
	linkfile.open(linkfilename);

	char buffer[1024];

	int count = 0;

	while (!linkfile.fail() && count < 10)
	{
		linkfile.getline(buffer, sizeof(buffer), '\n');
		string line = string{buffer};

		// First, split the command into components
		std::vector<string> splitUrl;
		boost::split(splitUrl, line, boost::is_any_of(" "));

		if (splitUrl.size() != 2)
		{
			continue;
		}

		string hostName = string{splitUrl[0]};
		string pathName = string{splitUrl[1]};

		if (pathName.empty() || hostName.empty())
		{
			continue;
		}

		if (pathName.compare("/") == 0)
		{
			pathName += "index.html";
		}

		string urlToHash = "http://" + hostName + pathName;
		string hash = urlhash(urlToHash.c_str(), urlToHash.size());

		//ParsedHeader_set(parsed, "Host", hostName.c_str());
		//ParsedHeader_set(parsed, "Path", pathName.c_str());
		parsed.changeHost(hostName);
		parsed.m_path = pathName;

		string outputfilenamenew{outputfilename};
		outputfilenamenew += "_" + boost::lexical_cast<std::string>(count);
		getFile(outputfiledir, outputfilenamenew, username, hash, parsed, pm, false);

		count++;
	}
}

int main(int argc, char *argv[])
{
	string outputfiledir;

	// output file in same dir
    if(argc==2)
    {
      outputfiledir=".";
    }
    else if (argc==3)
    {
      outputfiledir=argv[2];
    }
    else
    {
    	std::cerr << "Wrong number of arguments. Usage: fileproxy file [destination]" << std::endl;;
    	return 1;
    }

    ifstream inputfile;
    inputfile.open(argv[1]);

    char buffer[1024];

    // Get the name for the output file
    inputfile.getline(buffer ,sizeof(buffer),'\r');
    inputfile.ignore();
    string outputfilename = string{buffer};

    //Now get the username
    inputfile.getline(buffer, sizeof(buffer), '\r');
	inputfile.ignore();
	string username = string{buffer};

	// This is the hash of the filename
	inputfile.getline(buffer, sizeof(buffer),'\r');
	string hash = string{buffer};

	inputfile.ignore();

	// Read request from the input file
    std::vector<char> request;
    while(!inputfile.fail())
    {
    	inputfile.read(buffer, sizeof(buffer));
    	int numbytes = inputfile.gcount();

    	for (int i = 0; i < numbytes; i++)
    	{
    		request.push_back(buffer[i]);
    	}
    }

    HttpRequest parsed = parseRequest(request, true);

    ProcessManager pm("/");
    getFile(outputfiledir, outputfilename, username, hash, parsed, pm, false);

    // Can close the request file
    inputfile.close();

    //Now read the links and get them
    string linkfilename = outputfiledir + "/" + outputfilename + ".link";

    //evaluateLinks(linkfilename, parsed, outputfilename, outputfiledir, username, pm);
    //ParsedRequest_destroy(parsed);

    string command = "rm " + linkfilename;
	pm.startNewCommand(command, true);

    return 0;
}
