/*
 * clink2dtn.h
 *
 *  Created on: Nov 9, 2014
 *      Author: jmeisel
 */

#ifndef CLINK2DTN_CLINK2DTN_H_
#define CLINK2DTN_CLINK2DTN_H_

#include<string>
#include "ProcessManager.h"
using namespace std;

class clink2dtnFileHandler
{
protected:
	clink2dtnFileHandler(string const & origin, vector<string> const & destination, string const & monitorDir, ProcessManager &pm, bool monitorIn);
	bool m_bMonitorIn;
	ProcessManager m_pm;
	string m_origin;
	vector<string> m_destination;
	string m_monitorDirectory;
	vector<string> allMonitorDirs;

public:
	virtual ~clink2dtnFileHandler();
	virtual void handleFile(string file, string dir) = 0;

	vector<string> const & getDestination();
	string getOrigin();
	vector<string> const & getMonitorDirs();
};

class clink2dtnOutgoing : public clink2dtnFileHandler
{
public:
	clink2dtnOutgoing(string const & origin, vector<string> const & destination, string const & monitorDir, ProcessManager &pm);
	virtual ~clink2dtnOutgoing();
	virtual void handleFile(string file, string dir);
};

class clink2dtnIncoming : public clink2dtnFileHandler
{
public:
	clink2dtnIncoming(string const & origin, vector<string> const & destination, string const & monitorDir, ProcessManager &pm);
	virtual ~clink2dtnIncoming();
	virtual void handleFile(string file, string dir);
};

class clink2dtnCity : public clink2dtnFileHandler
{
private:
	string m_outDirectory;
	void sendFile(string filename);
public:
	clink2dtnCity(string const & origin, vector<string> const & destination, string const & monitorDir, ProcessManager &pm, string const & outDir);
	virtual ~clink2dtnCity();
	virtual void handleFile(string file, string dir);
};

#endif /* CLINK2DTN_CLINK2DTN_H_ */
