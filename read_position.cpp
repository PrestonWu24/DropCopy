
#include <stdio.h>
#include <cstdio>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <pthread.h>

#include "trace_log.h"
#include "util.h"
#include "msg_protocol.h"
#include "client_message.h"
#include "read_config.h"
#include "read_position.h"
#include "globex_common.h"
#include "trade_socket_list.h"
#include "exec_globex.h"

extern EtTradeSocketList* g_socketListPtr;
extern exec_globex* et_globex;
extern bool et_g_exitProcess;

EtReadPosition::EtReadPosition()
{
	m_exitThread = false;

	memset(m_leaveBuff, 0, recv_buffer_size);
	m_leaveLen = 0;
	m_parseLen = 0;

	// set flag
	memset(m_endFlag, 0, 5);
	m_endFlag[0] = cMsgEqual;
	m_endFlag[1] = cMsgEndChar;
	m_endFlagLen = 2;

	m_recvSocket = -1;
}


EtReadPosition::~EtReadPosition()
{
	if (m_recvSocket != -1)
	{
		close(m_recvSocket);
		m_recvSocket = -1;
	}
}

void EtReadPosition::setRecvSocket(int _socket)
{
    m_recvSocket = _socket;
}

/**
 * start the thread to read from client.
 */
bool EtReadPosition::startReadData()
{
	int	iRes = 0;
	
	// run thread
	iRes = pthread_create(&m_recvThread, NULL, readDataThread, this);
	if (iRes != 0)
	{
		DBG_ERR("fail to create a thread. %s.", strerror(iRes));
		return false;
	}
	return true;
}


bool EtReadPosition::splitBuffer()
{
    int len = 0;
    int leaveFlag = 0;
       
    char* buffEnd = m_parseBuff + m_parseLen;
    char* beginPtr = m_parseBuff;
    char* endPtr = NULL;

    while (beginPtr < buffEnd)
    {
        endPtr = strstr(beginPtr, m_endFlag);
        if (endPtr == NULL)
	    {
	        leaveFlag = 1;
	        break;
	    }

	    endPtr += m_endFlagLen;
	    len = endPtr - beginPtr;
	    
	    if ( !parseBuffer(beginPtr, len) )
	    {
	    	DBG_ERR("fail to parse price package.");
	    	return false;
	    }

		beginPtr += len;
	}
	
	if (1 == leaveFlag)
	{
	    int leaveSize = buffEnd - beginPtr;
	    memcpy(m_leaveBuff, beginPtr, leaveSize);
	    m_leaveLen = leaveSize;
	}
	
	return true;
}


/**
 * read data from Data Provider.
 */
void* EtReadPosition::readDataThread(void *arg)
{
	DBG_DEBUG("enter read position data.");
		
	char* ptr = NULL;
	
	fd_set fds;
	int	res = 0;
	
	struct timeval timeout;	
	timeout.tv_sec  = 1;
	timeout.tv_usec = 0;
	
	EtReadPosition* myObj = (EtReadPosition*)arg;
	
	while (true)
	{
		if (myObj->m_exitThread)
		{
		    break;
		}
		
		FD_ZERO(&fds);
		FD_SET(myObj->m_recvSocket, &fds);

		res = select(myObj->m_recvSocket + 1, &fds, NULL, NULL, &timeout);
		if (res < 0)
		{
			DBG_ERR("fail to select when receive position data: %s.", strerror(errno));
			break;
		}
		else if (res == 0)
		{
			continue;
		}
		
		if (FD_ISSET(myObj->m_recvSocket, &fds))
		{
			memset(myObj->m_recvBuff, 0, recv_buffer_size);
			myObj->m_recvSize = recv(myObj->m_recvSocket, myObj->m_recvBuff, recv_buffer_size, 0);
			if (myObj->m_recvSize <= 0)
			{
				DBG_ERR("fail to read position data. %s", strerror(errno));
				break;
			}
			DBG_DEBUG("read from position server: %s", myObj->m_recvBuff);
			memset(myObj->m_parseBuff, 0, recv_buffer_size * 2);

			if (myObj->m_leaveLen == 0)
			{
				memcpy(myObj->m_parseBuff, myObj->m_recvBuff, myObj->m_recvSize);
				myObj->m_parseLen = myObj->m_recvSize;
			}
			else
			{
				ptr = myObj->m_parseBuff;
				memcpy(ptr, myObj->m_leaveBuff, myObj->m_leaveLen);
				ptr += myObj->m_leaveLen;
				memcpy(ptr, myObj->m_recvBuff, myObj->m_recvSize);
				myObj->m_parseLen = myObj->m_leaveLen + myObj->m_recvSize;
				
				memset(myObj->m_leaveBuff, 0, recv_buffer_size);
				myObj->m_leaveLen = 0;
			}
            
			if ( !myObj->splitBuffer() )
			{
				DBG_ERR("fail to split buffer.");
				break;
			}

		}
	}
	
	myObj->m_exitThread = true;
	DBG(TRACE_LOG_DEBUG, "Leave the readPositionThread.");
	return NULL;
}


bool EtReadPosition::parseBuffer(const char* _buffer, const int _length)
{
    DBG_DEBUG("parse position message: %.*s", _length, _buffer);
	if (_buffer == NULL || _length <= 0)
	{
		DBG_ERR("the parameter is error.");
		return false;
	}
	
	EtClientMessage cliMsg;
	if ( !cliMsg.parseMessage(_buffer, _length) )
	{
		DBG_ERR("fail to parse message.");
		return false;
	}
	
	int msgType = 0;
	if ( !cliMsg.getFieldValue(fid_msg_type, msgType) )
	{
		DBG_ERR("fail to get fid_msg_type value.");
		return false;
	}

	DBG_DEBUG("msgType = %d", msgType);

	if(msgType == msg_posi_start_trade)
	{
		doPosiStart(cliMsg);
	}
	else if (msgType == msg_posi_exit)
	{
	    doPosiExit();
	}
	else
	{
		DBG_ERR("msg type is error, %d", msgType);
		//return false;
	}

	return true;
}

bool EtReadPosition::doPosiStart(EtClientMessage& _ecm)
{
  int seq_num = 0;
  if ( !_ecm.getFieldValue(fid_posi_seq_num, seq_num) ) 
    { 
      DBG_ERR("fail to get fid_posi_seq_num value."); 
      return false; 
    } 
 
  DBG_DEBUG("doPosiStart = %d", seq_num);

  if(seq_num == 0){
    seq_num = 1;
  }
  
  char session[16];
  memset(session, 0, 16);
  strcpy(session, EtReadConfig::m_session_id.c_str());
  et_globex->modify_last_seq(session, seq_num);

  et_globex->start();
  return true;
}

void EtReadPosition::doPosiExit()
{
    DBG_DEBUG("doPosiExit.");
    m_exitThread = true;
    if (-1 == et_globex->send_logout())
        {
            DBG_DEBUG("fail to logout.");
            return;
        }
    return;
}

void EtReadPosition::stopReadData()
{
    g_socketListPtr->deleteSocketFromList(m_recvSocket);
    m_exitThread = true;
    close(m_recvSocket);
    m_recvSocket = -1;
    et_g_exitProcess = true;
}
