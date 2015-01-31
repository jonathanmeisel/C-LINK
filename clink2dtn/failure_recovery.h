/*
 * failure_recovery.h
 *
 *  Created on: Nov 24, 2014
 *      Author: jmeisel
 */

#ifndef CLINK2DTN_FAILURE_RECOVERY_H_
#define CLINK2DTN_FAILURE_RECOVERY_H_

#include <string>
#include "socket_link2.h"
#include "socket_utils2.h"

enum KioskStatus
{
	NOKIOSK_ONLINE,
	KIOSK_ONLINE,
	OFFLINE
};


struct FailureRecover
{
	std::string currentKiosk;
	bool m_kiosk;
	bool prompted;

	FailureRecover(std::string kioskIp, bool kiosk);
	bool requestMappingFiles();
	void handleMessage(SockWrapper&& wrapper);
	bool checkFailure();
	KioskStatus checkFailure(std::string ipAddr);
	void startRecoveryServer();
	void generateConfig();
	void acceptorThread();
	void startThreads();
	void sendPrompt(std::string ip);
};

enum MappingFileType
{
	LASTKNOWNMAPPING, LOCATIONMAPPING
};


#endif /* CLINK2DTN_FAILURE_RECOVERY_H_ */
