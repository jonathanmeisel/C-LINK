/*
 * ProcessManager.cpp
 *
 *  Created on: Nov 9, 2014
 *      Author: jmeisel
 */

#include "ProcessManager.h"
#include "clink2dtnUtils.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <unistd.h>
#include <boost/algorithm/string.hpp>
#include <iostream>

// Initialize to no base directory
ProcessManager::ProcessManager()
: m_baseDirectory(""), m_runningProcesses(), m_maxProcesses(-1)
{ }

// Run all commands relative to the base directory
ProcessManager::ProcessManager(string basedirectory)
: m_baseDirectory(basedirectory), m_runningProcesses(), m_maxProcesses(-1)
{
	if (!checkFile(m_baseDirectory, true))
	{
		throw std::invalid_argument("Invalid base directory.");
	}
}

ProcessManager::~ProcessManager()
{
	//Kill all the processes
	for(unordered_set<pid_t>::iterator a = m_runningProcesses.begin(); a != m_runningProcesses.end(); ++a)
	{
	    kill(*a, false);
	}
}

void ProcessManager::setMaxProcesses(int maxProcesses)
{
	m_maxProcesses = maxProcesses;
}

pid_t ProcessManager::startNewCommand(string command, bool synchronous)
{
	return startNewCommand(command, synchronous, "");
}

pid_t ProcessManager::startNewCommand(string command, bool synchronous, string dir)
{
	ProcessRequest request(command);
	request.m_synchronous = synchronous;
	request.m_directory = dir;

	return startNewCommand(request);
}

// Start the new process (need to reap if synchronous is false)
pid_t ProcessManager::startNewCommand(ProcessRequest & request)
{
 	if (request.m_command.empty())
	{
		throw std::invalid_argument("The command is empty.");
	}

	// First, split the command into components
	vector<string> splitCommand;
	boost::split(splitCommand, request.m_command, boost::is_any_of(" "));

	if (splitCommand.size() < 1)
	{
		throw std::invalid_argument("Huh?");
	}

	// The executable and its arguments
	//char const * executable;
	char ** args = new char * [splitCommand.size() + 1];
	int i = 0;

	std::vector<string>::iterator it;
	// Split up the command, replace with relative paths if necessary
	for (string& scommand : splitCommand)
	{
		boost::replace_first(scommand, "+base+", m_baseDirectory);
		args[i] = (char * ) scommand.c_str();
		i++;
	}

	// Make sure last argument pointer is null
	args[splitCommand.size()] = NULL;

	// Make sure more than max processes aren't running
	while (!request.m_synchronous && (m_maxProcesses > 0) && (m_runningProcesses.size() >= m_maxProcesses))
	{
		waitForOne();
	}

	pid_t childPid = fork();

	// the child
	if (childPid == 0)
	{
		if (!request.m_directory.empty())
		{
			if (!checkFile(request.m_directory, true))
			{
				exit(1);
			}

			chdir(request.m_directory.c_str());
		}

		if (request.m_uid >= 0)
		{
			setuid(request.m_uid);
		}

		if (!request.m_env.empty())
		{
			execvpe((const char *)args[0], (char * const *)args, (char * const *) &request.m_env[0]);
		}
		else
		{
			execvp((const char *) args[0], (char * const * )args);
		}
	}
	else
	{
		int childStatus;

		// If it's synchronous, wait until the process exits
		if (request.m_synchronous)
		{
			waitpid(childPid, &childStatus, 0);
		}
		else
		{
			m_runningProcesses.insert(childPid);
			cout << "process: " << args[0] << " started with pid: " << childPid << endl;
		}
	}

	return childPid;
}

// Reaps zombies
void ProcessManager::pollOpenProcesses()
{
	int status;
	pid_t tpid;

	while((tpid = waitpid(-1, &status, WNOHANG)) > 0)
	{
		if (m_runningProcesses.count(tpid) > 0)
		{
			m_runningProcesses.erase(tpid);
		}
		cout << "Process: " << tpid << " exited with status: " << status << endl;
	}
}

// Waits for all running process to terminate
void ProcessManager::waitForAll()
{
	int status;
	pid_t tpid;

	while(m_runningProcesses.size() > 0)
	{
		tpid = wait(&status);

		if (m_runningProcesses.count(tpid) > 0)
		{
			m_runningProcesses.erase(tpid);
		}

		cout << "Process: " << tpid << " exited with status: " << status << endl;
	}
}

// Wait for a specific process to exit
void ProcessManager::waitFor(pid_t pid)
{
	int status;
	if (m_runningProcesses.count(pid) > 0)
	{
		waitpid(pid, &status, 0);
		m_runningProcesses.erase(pid);
		cout << "Process: " << pid << "exited with status: " << status << endl;
	}
}

void ProcessManager::waitForOne()
{
	int status;
	if (m_runningProcesses.size() > 0)
	{
		pid_t pid = waitpid(-1, &status, 0);
		m_runningProcesses.erase(pid);
		cout << "Process: " << pid << "exited with status: " << status << endl;
	}
}

// Kills a running process
void ProcessManager::killProcess(pid_t pid, bool force)
{
	if (m_runningProcesses.count(pid) > 0)
	{
		if (!force)
		{
			kill(pid, SIGTERM);
		}
		else
		{
			kill(pid, SIGKILL);
		}

		waitFor(pid);
		cout << "Killed Process " << pid << endl;
	}
}
