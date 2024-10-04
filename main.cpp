
#include <stdio.h>
#include <cstdio>
#include <unistd.h>
#include <iostream>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>

#include "trace_log.h"
#include "util.h"
#include "exec_globex.h"
#include "drop_copy.h"
#include "read_config.h"
#include "trade_socket_list.h"
#include "client_message.h"
#include "globex_common.h"
#include "msg_protocol.h"
#include "read_position.h"

using namespace std;

exec_globex* et_globex = NULL;
drop_copy* et_g_dropcopy = NULL;
EtTradeSocketList* g_socketListPtr = NULL;
EtReadPosition* g_readPositionPtr = NULL;
bool et_g_exitProcess = false;
bool dropcopy_exit = false;

typedef void Sigfunc(int);
void sigHandle(int signo);
Sigfunc *setupSignal(int signo, Sigfunc *func);

//void test(int _socket)
void test()
{
  /*
  EtClientMessage cm;

  for(int i=0; i<2; i++)
    {
      cm.clearInMap();
      if(i == 0)
        {
          cm.putField(fid_msg_type, msg_posi_position);
          cm.putField(fid_posi_account, ORDER_USER_NAME);
          cm.putField(fid_posi_symbol, "CLU0");
          cm.putField(fid_posi_price, 8221);
          cm.putField(fid_posi_fix_qty, 2);
        }
      else if(i == 1)
        {
          cm.putField(fid_msg_type, msg_posi_position);
          cm.putField(fid_posi_account, ORDER_USER_NAME);
          cm.putField(fid_posi_symbol, "CLZ0");
          cm.putField(fid_posi_price, 8294);
          cm.putField(fid_posi_fix_qty, 2);
        }
            
      if ( !cm.sendMessage(_socket) )
        {
          DBG_ERR("Fail to send package to client.");
          return;
        }
    }
  return;    
  */

    if (-1 == et_globex->send_login())
        {
	    cout << "fail to login.\n";
	    et_g_exitProcess = true;
	    return;
        }

    sleep(15);

    if (-1 == et_globex->send_logout())
        {
            cout << "fail to logout.\n";
            et_g_exitProcess = true;
            return;
            }
}

void sigHandle(int signo)
{
    switch (signo)
    {
        case SIGTERM:
            DBG(TRACE_LOG_DEBUG, "Catch SIGTERM.");
            et_g_exitProcess = true;
            break;

        case SIGINT:
            DBG(TRACE_LOG_DEBUG, "Catch SIGINT.");
            et_g_exitProcess = true;
            break;

        default:
            DBG(TRACE_LOG_ERR, "The signal is %d.", signo);
            break;
    }
}


Sigfunc *setupSignal(int signo, Sigfunc *func)
{
    struct sigaction act, oact;
    act.sa_handler = func;
    sigemptyset(&act.sa_mask);

    if (-1 == sigaddset(&act.sa_mask, signo))
    {
        DBG_ERR("Failed to add a signal %d: %s", signo, strerror(errno));
        return SIG_ERR;
    }

    act.sa_flags = 0;

    if (sigaction(signo, &act, &oact) < 0)
    {
        DBG_ERR("Failed to act a signal %d: %s", signo, strerror(errno));
        return SIG_ERR;
    }
    return (oact.sa_handler);
}

/**
 * listen the client connection.
 */
bool listenConnection(int _port)
{
    int iListenfd = 0;
    int clientSocket = 0;
    
    struct sockaddr_in clientAddr;
    struct sockaddr_in serverAddr;
    
    int iResult = 0;
    
    iListenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (iListenfd < 0)
    {
    	DBG(TRACE_LOG_ERR, "Fail to open listen socket.");
    	return false;
    }
    
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(_port);
    
    iResult = bind(iListenfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (iResult < 0)
    {
    	DBG_ERR("Failed to bind the server port, %d", _port);
    	close(iListenfd);
    	return false;
    }
    
    iResult = listen(iListenfd, 10);
    if (iResult < 0)
   	{
	    close(iListenfd);
	    DBG(TRACE_LOG_ERR, "Fail to listen the server.");
	    return false;
	}
	
    DBG_DEBUG("begin listen on socket at port %d", _port);
    
    socklen_t cliLen = sizeof(clientAddr);
    
    fd_set fds;
    struct timeval timeout;	
    timeout.tv_sec  = 2;
    timeout.tv_usec = 0;
	
    //bool isRight = false;
    
    while (true)
	{
		if (et_g_exitProcess)
		{
		    break;
		}
		
		FD_ZERO(&fds);
		FD_SET(iListenfd, &fds);

		iResult = select(iListenfd + 1, &fds, NULL, NULL, &timeout);
		if (iResult < 0)
		{
			DBG_ERR("fail to select when receive price data: %s.", strerror(errno));
			break;
		}
		else if (iResult == 0)
		{
			continue;
		}
		
		if (FD_ISSET(iListenfd, &fds))
		{
			clientSocket = accept(iListenfd, (struct sockaddr *)&clientAddr, &cliLen);
			if (clientSocket < 0)
			{
				DBG_ERR("Fail to accept a socket.");	
				break;
   			}

			DBG_DEBUG("accept a socket!");

			g_readPositionPtr->setRecvSocket(clientSocket);

			if ( !g_readPositionPtr->startReadData() )
			    {
                  DBG(TRACE_LOG_ERR, "fail to start reading positionSrv.");
                  return false;
			    }
			else
			    {
                  DBG_DEBUG("start reading positionSrv.");
			    }

			g_socketListPtr->addSocketToList(clientSocket, false);

            /*
            // start of test purpose only
            char confirm = 'n';
            cout << "continue?(y/n)" << endl;
            cin >> confirm;
            if(confirm == 'y' || confirm == 'Y')
              {
                test(clientSocket);
              }
            // start of test purpose only
            */

			return true;
		}
	}
	
	return true;
}

int main(int argc, char *argv[]) 
{
 	int iTmp = 0;
	char* cPtr = NULL;
	char cCurPath[128];
	char cFileName[128];
	int	m_iLockFd = 0;
	
	// path
	memset(cCurPath, 0, 128);
	cPtr = strrchr(argv[0], '/');
    if (cPtr == NULL)
    {
        printf("The process name is error. \n");
        exit(-1);
    }
    iTmp = cPtr - argv[0];
    memcpy(cCurPath, argv[0], iTmp);

    // init log
    g_traceLogInit(cCurPath, "Dropcopy", 0, 0, 1);
    DBG_DEBUG("Start process.");
	
    // setup SIGTERM signal
    if (SIG_ERR == setupSignal(SIGTERM, sigHandle))
    {
        DBG_ERR("Failed to setup signal SIGTERM.");
        exit(-1);
    }
    if (SIG_ERR == setupSignal(SIGINT, sigHandle))
    {
        DBG_ERR("Failed to setup signal SIGINT.");
        exit(-1);
    }

	// load config file
	memset(cFileName, 0, 128);
	strcpy(cFileName, cCurPath);
	strcat(cFileName, "/config");
	if ( !EtReadConfig::loadConfigFile(cFileName) )
	{
		EtUtil::releaseLock(m_iLockFd);
		exit(-1);
	}
	
	et_globex = new exec_globex;
	g_socketListPtr = new EtTradeSocketList;
	g_readPositionPtr = new EtReadPosition;
	et_g_dropcopy = new drop_copy;
    
    if (-1 == et_globex->globex_start()){
      return -1;
    }

	// listen the client connection 
    if (listenConnection(EtReadConfig::m_listenPort) == -1){
      DBG(TRACE_LOG_ERR, "Failed to listen a socket.");
      et_g_exitProcess = true;
      exit(-1);
	}

	//test();

	sleep(1);
	while (1)
	{
		sleep(5);
   		if (dropcopy_exit)
   		{
		    break;
   		}
	}

    if( et_globex != NULL){
      DBG_DEBUG("delete globex ptr.");
      delete et_globex;
      DBG_DEBUG("deleted.");
    }
	if( g_socketListPtr != NULL){
      DBG_DEBUG("delete socket ptr.");
      delete g_socketListPtr;
      DBG_DEBUG("deleted.");
    }
    if( et_g_dropcopy != NULL){
      DBG_DEBUG("delete dropcopy ptr.");
      delete et_g_dropcopy;
      DBG_DEBUG("deleted.");
    }

	DBG(TRACE_LOG_DEBUG, "Leave main.");

  	exit(0);

}

