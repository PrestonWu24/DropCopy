
#include "trace_log.h"
#include "util.h"
#include "msg_protocol.h"
#include "client_message.h"
#include "trade_socket_list.h"

#include <string>
#include <vector>
using namespace std;


EtTradeSocketList::EtTradeSocketList()
{
	pthread_mutex_init(&m_mutex, NULL);
}


EtTradeSocketList::~EtTradeSocketList()
{
	pthread_mutex_lock(&m_mutex);
	
	vector<int>::iterator vi = m_socketList.begin();
	DBG_DEBUG("socket list size = %d", (int)m_socketList.size());
	if((int)m_socketList.size() > 0)
	    {
		while (vi != m_socketList.end())
		    {
			if(*vi != -1)
			    {
				close(*vi);
				*vi = -1;
			    }
			vi++;
		    }
	    }
	m_socketList.clear();
	
	pthread_mutex_unlock(&m_mutex);
	
	pthread_mutex_destroy(&m_mutex);
}


bool EtTradeSocketList::addSocketToList(int _socket, bool _needInit)
{
  DBG_DEBUG("addSocketToList: %d", _socket);
	// add the new socket to the socket list
	pthread_mutex_lock(&m_mutex);
	m_socketList.push_back(_socket);
	pthread_mutex_unlock(&m_mutex);
	
	return true;
}

bool EtTradeSocketList::deleteSocketFromList(int _socket)
{
    pthread_mutex_lock(&m_mutex);

    // need modify here
    m_socketList.pop_back();
    pthread_mutex_unlock(&m_mutex);

    return true;
}

bool EtTradeSocketList::sendDataToServers(EtClientMessage& cm)
{
    DBG_DEBUG("sendDataToServers.");
	pthread_mutex_lock(&m_mutex);
	vector<int>::iterator vi = m_socketList.begin();

	while (vi != m_socketList.end())
	{
		if ( !cm.sendMessage(*vi) )
		{
			DBG_ERR("Fail to send package to client.");
			close(*vi);
		    vi = m_socketList.erase(vi);
			DBG(TRACE_LOG_WARN, "There is a connection is closed.");
			return false;
		}
		else
		{
			vi++;
		}
	}
	
	pthread_mutex_unlock(&m_mutex);
	return true;
}

