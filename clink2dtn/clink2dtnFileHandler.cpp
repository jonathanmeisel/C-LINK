/*
 * clink2dtnFileHandler.cpp
 *
 *  Created on: Nov 9, 2014
 *      Author: jmeisel
 */
#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include "ProcessManager.h"
#include "clink2dtn.h"
#include "clink2dtnUtils.h"
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

namespace fs = boost::filesystem;

/* -------------------------------- clink2dtnFileHandler ------------------------------------------ */

clink2dtnFileHandler::clink2dtnFileHandler(string const & origin, vector<string> const & destination, string const & monitorDir, ProcessManager &pm, bool monitorIn)
: m_origin(origin), m_destination(destination), m_pm(pm), m_monitorDirectory(monitorDir), m_bMonitorIn{monitorIn}
{
	//First, check if origin and destination are valid strings
	if (m_origin.empty() || m_monitorDirectory.empty())
	{
		throw std::invalid_argument("Origin, Destination, and Directory must all not be null");
	}

	if (!checkFile(m_monitorDirectory, true))
	{
		fs::path dir(m_monitorDirectory);
		if (!boost::filesystem::create_directory(dir))
		{
			throw std::invalid_argument("Specified directory does not exist, and cannot be created.");
		}
	}

	if (monitorIn)
	{
		// Make sure directory exists for every possible "them"
		for (string destination : m_destination)
		{
			string ppath = m_monitorDirectory + destination;
			fs::path dir(ppath);
			if (!checkFile(ppath, true))
			{
				if (!boost::filesystem::create_directory(dir))
				{
					throw std::invalid_argument("Can't create dir");
				}
			}
			allMonitorDirs.push_back(ppath);
		}
	}
	else
	{
		allMonitorDirs.push_back(m_monitorDirectory);
	}
}

clink2dtnFileHandler::~clink2dtnFileHandler()
{}

vector<string> const & clink2dtnFileHandler::getDestination()
{
	return m_destination;
}

string clink2dtnFileHandler::getOrigin()
{
	return m_origin;
}

vector<string> const & clink2dtnFileHandler::getMonitorDirs()
{
	return allMonitorDirs;
}

/* -------------------------------- clink2dtnOutgoing --------------------------------------- */

clink2dtnOutgoing::clink2dtnOutgoing(string const & origin, vector<string> const & destination, string const & monitorDir, ProcessManager &pm)
: clink2dtnFileHandler(origin, destination, monitorDir, pm, false)
{}

clink2dtnOutgoing::~clink2dtnOutgoing()
{}

// Take the given file and send it over the DTN
void clink2dtnOutgoing::handleFile(string file, string directory)
{
	m_pm.pollOpenProcesses();
	file = directory + file;

	//First, check that the file is a real string and that it exists
	if (!checkFile(file, false))
	{
		cout << "The file " << file << " does not exist.  Not sending." << endl;
		return;
	}

	cout << "sending file " << file << endl;

	string command;

	for (string destination : m_destination)
	{
		command = "+base+DTN2/apps/dtncp/dtncp " + file + " dtn://" + destination;
		cout << "sending file: " << file << " over DTN to the city " << endl;
		m_pm.startNewCommand(command, true);
	}

	command = "rm " + file;
	m_pm.startNewCommand(command, true);
}

/* -------------------------------- clink2dtnIncoming --------------------------------------- */

clink2dtnIncoming::clink2dtnIncoming(string const & origin, vector<string> const & destination, string const & monitorDir, ProcessManager &pm)
: clink2dtnFileHandler(origin, destination, monitorDir, pm, true)
{}

clink2dtnIncoming::~clink2dtnIncoming()
{}

// Take the given file and then untar and do arrival stuff to it
void clink2dtnIncoming::handleFile(string file, string directory)
{
	m_pm.pollOpenProcesses();
	string fullpath = directory + file;

	//Check that the file is a tar file (has to end with .tar)
	if (fullpath.length() < 4)
	{
		cout << "The file is not  a tar file" << endl;
		string command = "rm " + fullpath;
		m_pm.startNewCommand(command, true);
		return;
	}

	string basename = fullpath.substr(0, fullpath.length() - 4);
	string suffix = fullpath.substr(fullpath.length() - 4, fullpath.length());

	if (suffix.compare(".tar") != 0)
	{
		cout << "The file is of the wrong format.  Must be a tar file." << endl;
		string command = "rm " + fullpath;
		m_pm.startNewCommand(command, true);
		return;
	}

	//Now, check that the file exists
	if (!checkFile(fullpath, false))
	{
		cout << "The file " << fullpath << " does not exist.  Not sending." << endl;
		return;
	}

	cout << "receiving file " << fullpath << endl;

	// Untar the file (should be two files that are tarred)
	string command = "tar xf " + file;
	m_pm.startNewCommand(command, true, directory);

	//check that a file with no extension and a file with a ".desc" extension exist
	string meta = basename + ".desc";
	if (!checkFile(basename, false) || !checkFile(meta, false))
	{
		cout << "The tar archive does not contain the necessary files.  Aborting receiving file: " << basename << endl;
		return;
	}

	// So now we know that the correct files exist.  Extract the username from the .desc file
	ifstream metastream;
	metastream.open(meta.c_str());
	string line;

	if (getline(metastream, line) <= 0)
	{
		cout << "Cannot determing usename from file.  Aborting. " << endl;
		return;
	}

	// Now get the username (which is up to the first . )
	string delimiter = ".";
	int loc = line.find(delimiter);

	if (loc < 0 || loc > line.length())
	{
		cout << "Cannot parse username.  Aborting." << endl;
	}

	string username = line.substr(0, loc);

	//Now, finalllllly, run arrival on the file
	command = "+base+/C-LINK/arrival " + username + " " + basename;
	m_pm.startNewCommand(command, true);

	command = "rm " + basename + " " + basename + ".desc" + " " + basename + ".tar";
	m_pm.startNewCommand(command, true, directory);
}

/* ------------------------------------- city ----------------------------------- */

void clink2dtnCity::sendFile(string filename)
{
	for (string destination : m_destination)
	{
		string command = "+base+DTN2/apps/dtncp/dtncp " + filename + " dtn://" + destination;
		cout << command << endl;
		m_pm.startNewCommand(command, true);
	}
}

clink2dtnCity::clink2dtnCity(string const & origin, vector<string> const & destination, string const & monitorDir, ProcessManager &pm, string const & outDir)
: clink2dtnFileHandler(origin, destination, monitorDir, pm, true), m_outDirectory(outDir)
{
	if (!checkFile(m_outDirectory, true))
	{
		throw std::invalid_argument("Specified directory does not exist.  Please create it.");
	}
}

clink2dtnCity::~clink2dtnCity()
{}

// Take the given file and send it over the DTN
void clink2dtnCity::handleFile(string file, string directory)
{
	m_pm.pollOpenProcesses();
	string fullpath = directory + file;

	//Check that the file is a .req file (has to end with .req)
	if (file.length() < 4)
	{
		cout << "The file is not  a req file" << endl;
		string command = "rm " + fullpath;
		m_pm.startNewCommand(command, true);
		return;
	}

	string basename = file.substr(0, file.length() - 4);
	string suffix = file.substr(file.length() - 4, file.length());

	if (suffix.compare(".req") != 0)
	{
		cout << "The file is of the wrong format.  Must be a req file." << endl;
		return;
	}

	//First, check that the file is a real string and that it exists

	if (!checkFile(fullpath, false))
	{
		cout << "The file " << file << " does not exist.  Not Fetching." << endl;
		return;
	}

	cout << "fetching file " << file << endl;
	string outdir = m_outDirectory + basename;
	string command = "mkdir " + outdir;
	m_pm.startNewCommand(command, true);

	command = "+base+C-LINK/proxy " + fullpath + " " + outdir;
	m_pm.startNewCommand(command, true);

	// Next, remove the file
	command = "rm " + directory + file;
	m_pm.startNewCommand(command, true);

	// Next, remove the file
	command = "rm " + directory + file;
	m_pm.startNewCommand(command, true);

	// Send all of the files
	fs::directory_iterator it(outdir), eod;

	BOOST_FOREACH(fs::path const &p, make_pair(it, eod))
	{
		if(is_regular_file(p))
		{
			sendFile(p.string());
		}
	}

	command = "rm -rf " + outdir;
	m_pm.startNewCommand(command, true);
}

