
#ifndef _ET_TRADE_SOCKET_LIST_H_
#define _ET_TRADE_SOCKET_LIST_H_

#include <vector>
#include <pthread.h>
#include "client_message.h"

using namespace std;


class EtTradeSocketList
{
public:
	
	// ---- function ----
	EtTradeSocketList();
	~EtTradeSocketList();

	bool addSocketToList(int _socket, bool _needInit);
	bool deleteSocketFromList(int _socket);
	bool sendDataToServers(EtClientMessage& cm);
	
private:
	
	// ---- function ----

	// ---- data ----
	pthread_mutex_t m_mutex;
	vector<int> m_socketList;

};

#endif
