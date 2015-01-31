/*
 * ProcessManager.h
 *
 *  Created on: Nov 9, 2014
 *      Author: jmeisel
 */

#ifndef CLINK2DTN_PROCESSMANAGER_H_
#define CLINK2DTN_PROCESSMANAGER_H_

#include <string>
#include <tr1/unordered_set>
#include <unistd.h>
#include <vector>
using namespace std;
using namespace std::tr1;

class ProcessRequest
{
public:

	string m_command;
	bool m_synchronous;
	string m_directory;
	uid_t m_uid;
	vector<string> m_env;

	ProcessRequest(string command) : m_command(command), m_synchronous(false), m_directory(""), m_uid(getuid()), m_env()
	{}
};

class ProcessManager
{
public:
	ProcessManager();
	ProcessManager(string basedirectory);
	~ProcessManager();

	void pollOpenProcesses(); //Reap all processes that have died
	void waitFor(pid_t pid); // Wait for the specific process
	void waitForOne(); // Wait for the next process to finish
	void waitForAll(); // Wait fotProcesses);

	void setMaxProcesses(int maxProcesses);

	pid_t startNewCommand(string command, bool synchronous, string dir);
	pid_t startNewCommand(string command, bool synchronous);
	pid_t startNewCommand(ProcessRequest & request);

	void killProcess(pid_t pid, bool force); // kill

private:
	unordered_set<pid_t> m_runningProcesses;
	string m_baseDirectory;
	int m_maxProcesses;
};


#endif /* CLINK2DTN_PROCESSMANAGER_H_ */
