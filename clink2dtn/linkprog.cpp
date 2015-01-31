/*
 * linkprog.cpp
 *
 *  Created on: Nov 9, 2014
 *      Author: jmeisel
 */

#include "clink2dtn.h"
#include "ProcessManager.h"
#include "clink2dtnUtils.h"
#include "failure_recovery.h"
#include <thread>
#include <fstream>
#include <unistd.h>
#include <string>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <sys/wait.h>
#include <vector>
#include <memory>
#include "defines.h"

pid_t monitor(vector<shared_ptr<clink2dtnFileHandler>> handlers, FailureRecover& recover, ProcessManager& pm, stringtostring& configuration, bool switcht);
bool readConfigFile(string configFile, stringtostring &configuration);
void switchKiosk(ProcessManager & pm, FailureRecover & f, stringtostring & configuration, vector<shared_ptr<clink2dtnFileHandler>>& handlers);
std::vector<std::string> parseLine(std::string line, std::string delimiter);

pid_t client;
pid_t user;
pid_t server;
pid_t dtncpd;

string TMPDIR = "tmpdir";
string INCOMING = "incoming";
string OUTGOING = "outgoing";
string BASE = "base";
string ME = "me";
string THEM = "them";
string MONITOR = "monitor";
string CLIENT = "clientuser";
string CLIENTNAME = "clientname";
string KIOSKIPS = "kioskips";

int main(int argc, char * argv[])
{
	if (argc != 3)
	{
		cout << "Usage: ./dtnconnect <user|city> <configuration> " << endl;
		return 0;
	}

	// Load in the configuration
	stringtostring configuration;

	string configFile = argv[2];
	if (!readConfigFile(configFile, configuration))
	{
		cout << "The configuration file isn't correct";
		return 0;
	}

	string type= argv[1];
	string base = configuration[BASE];

	// Start a process manager to keep track of the different programs being run
	ProcessManager pm(base);

	vector<shared_ptr<clink2dtnFileHandler>> handlers;

	FailureRecover recover{"127.0.0.1", true};

	std::thread recoverythread;

	if (type.compare("user") == 0)
	{
		recoverythread = std::thread{&FailureRecover::acceptorThread, &recover};
	}


	if (type.compare("user") == 0)
	{
		switchKiosk(pm, recover, configuration, handlers);
		monitor(handlers, recover, pm, configuration, true);
	}

	// Don't need to check kiosks when starting as city
	if (type.compare("city") == 0)
	{
		string dtncpdcommand = "+base+DTN2/apps/dtncpd/dtncpd " + configuration[MONITOR];
		dtncpd = pm.startNewCommand(dtncpdcommand, false);

		vector<string> vthem = parseLine(configuration[THEM], " ");
		handlers.push_back(shared_ptr<clink2dtnFileHandler>{new clink2dtnCity{configuration[ME], vthem, configuration[MONITOR], pm, configuration[OUTGOING]}});
		monitor(handlers, recover, pm, configuration, false);
	}
}

bool readConfigFile(string configFile, stringtostring &configuration)
{
	if (!checkFile(configFile, false))
	{
		return false;
	}

	string line;
	ifstream config;
	config.open(configFile.c_str());

	while (getline(config, line))
	{
		vector<string> splitLine;
		boost::split(splitLine, line, boost::is_any_of("="));

		if (splitLine.size() != 2)
		{
			continue;
		}

		string category = splitLine[0];
		string setting = splitLine[1];

		configuration[category] = setting;
	}

	return true;
}

pid_t monitor(vector<shared_ptr<clink2dtnFileHandler>> handlers, FailureRecover& recover, ProcessManager& pm, stringtostring& configuration, bool switcht)
{
	while(true)
	{
		sleep(10);
		for (shared_ptr<clink2dtnFileHandler> begin : handlers)
		{
			monitorDir(*begin);
		}

		if (switcht)
		{
			bool kioskFailure = recover.checkFailure();
			if (kioskFailure || recover.prompted)
			{
				switchKiosk(pm, recover, configuration, handlers);
			}
			else if (!recover.m_kiosk)
			{
				recover.requestMappingFiles();
			}
		}
	}
	return 0;
}

std::vector<string> readKioskFile()
{
	std::vector<string> kioskips;
	char line [256];

	std: ifstream stream{"kiosklist.cfg"};

	while (!stream.fail())
	{
		stream.getline(line, 256);
		kioskips.push_back(std::string{line});
	}
	return kioskips;
}

void switchKiosk(ProcessManager & pm, FailureRecover & f, stringtostring & configuration, vector<shared_ptr<clink2dtnFileHandler>>& handlers)
{
	// Kill all running processes (if any exist), and set their PID's to zero
	pm.killProcess(user, false);
	pm.killProcess(client, false);
	pm.killProcess(dtncpd, false);
	pm.killProcess(server, false);
	user = 0;
	client = 0;
	dtncpd = 0;
	server = 0;

	// "Client" must run as user and the request needs special information
	ProcessRequest clientreq("+base+C-LINK/client config");
	string uservar = "USER=" + configuration[CLIENTNAME];
	clientreq.m_env.push_back(uservar);
	clientreq.m_synchronous = false;

	// Get the client's user ID (may be necessary to use at some point)
	uid_t clientuid = boost::lexical_cast<uid_t>(configuration[CLIENT]);
	clientreq.m_uid = clientuid;

	//vector<string> kioskips = parseLine(configuration[KIOSKIPS], " ");
	vector<string> kioskips = readKioskFile();

	handlers.clear();

	bool me;
	string newKiosk;

	for (string kioskip : kioskips)
	{
		if (kioskip.size() == 0)
		{
			continue;
		}
		// If it's us, we should be the kiosk
		if (kioskip.at(0) == '*')
		{
			if (newKiosk.empty())
			{
				me = true;
				newKiosk = std::string{"127.0.0.1"};
				continue;
			}
			else
			{
				continue;
			}
		}

		// Find the first computer that's online
		//If it's not the kiosk, prompt it
		KioskStatus stat = f.checkFailure(kioskip);
		if (stat == NOKIOSK_ONLINE && newKiosk.empty())
		{
			f.sendPrompt(kioskip);
			newKiosk = std::string{kioskip};
		}
		// If it is the kiosk, use it
		else if (stat == KIOSK_ONLINE && newKiosk.empty())
		{
			newKiosk = std::string{kioskip};
			break;
		}
		// If something further up the list was already selected, prompt the kiosk to check itself
		else if (stat == KIOSK_ONLINE && !newKiosk.empty())
		{
			f.sendPrompt(kioskip);
		}
	}

	if (newKiosk.empty())
	{
		cout << "No kiosk currently available " << endl;
		return;
	}

	f.currentKiosk = newKiosk;
	f.m_kiosk = bool{me};
	f.generateConfig();

	user = pm.startNewCommand("+base+C-LINK/user config", false);
	client = pm.startNewCommand(clientreq);

	if (me)
	{
		vector<string> vthem = parseLine(configuration[THEM], " ");

		handlers.push_back(shared_ptr<clink2dtnFileHandler>{new clink2dtnOutgoing{configuration[ME], vthem, configuration[OUTGOING], pm}});
		handlers.push_back(shared_ptr<clink2dtnFileHandler>{new clink2dtnIncoming{configuration[ME], vthem, configuration[MONITOR], pm}});

		server = pm.startNewCommand("+base+C-LINK/server config", false);
		string dtncpdcommand = "+base+DTN2/apps/dtncpd/dtncpd " + configuration[MONITOR];
		dtncpd = pm.startNewCommand(dtncpdcommand, false);
	}
	f.prompted = false;
	f.m_kiosk = me;

	std::cout << "Kiosk has IP: " << newKiosk << std::endl;
}


std::vector<std::string> parseLine(std::string line, std::string delimiter)
{
	std::vector<std::string> ret;
	boost::split(ret, line, boost::is_any_of(delimiter));
	return ret;
}
