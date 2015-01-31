/*
 * clink2dtnUtils.cpp
 *
 *  Created on: Nov 9, 2014
 *      Author: jmeisel
 */

#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include "clink2dtn.h"

using namespace std;
namespace fs = boost::filesystem;

// Checks that a file with "path" exists and whether or not it's a directory
// If it is a directory, makes sure it has the final slash
bool checkFile(string & fullpath, bool directory)
{
	// The path is an empty string or null
	if (fullpath.empty())
	{
		return false;
	}

	if (!boost::filesystem::exists(fullpath))
	{
		return false;
	}

	// Should end with a slash
	if (directory && boost::filesystem::is_directory(fullpath))
	{
		char last = *fullpath.rbegin();
		if (last != '/')
		{
			fullpath = fullpath + "/";
		}
		return true;
	}

	else if (!directory && boost::filesystem::is_regular_file(fullpath))
	{
		return true;
	}

	return false;
}

void monitorDir(clink2dtnFileHandler &handler)
{
	vector<string> dirs = handler.getMonitorDirs();
	for (string directory : dirs)
	{
		if (!checkFile(directory, true))
		{
			continue;
		}

		fs::directory_iterator it(directory), eod;

		BOOST_FOREACH(fs::path const &p, make_pair(it, eod))
		{
			if(is_regular_file(p))
			{
				handler.handleFile(p.filename().string(), directory);
			}
		}
	}
}

