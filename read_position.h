
#ifndef _READ_POSITION_H_
#define _READ_POSITION_H_

#include <string>
using namespace std;


class EtReadPosition
{
public:
	
	// ---- constructor ----
	EtReadPosition();
	~EtReadPosition();
	
	// ---- function ----
	bool startReadData();
	void stopReadData();
	void setRecvSocket(int _socket);
	
private:
	
	// ---- function ----
	static void* readDataThread(void *arg);
	
	bool parseBuffer(const char* _buffer, const int _length);
	bool splitBuffer();
	void doPosiExit();
    bool doPosiStart(EtClientMessage& _ecm);

	// ---- data ----
	static const int recv_buffer_size = 128;
	pthread_t m_recvThread;
	
	bool m_exitThread;
	int m_recvSocket;
	
	char m_leaveBuff[recv_buffer_size];
	int m_leaveLen;
	char m_parseBuff[recv_buffer_size * 2];
	int m_parseLen;
	char m_recvBuff[recv_buffer_size];
	int m_recvSize;
	
	char m_endFlag[5];
	int m_endFlagLen;
};


#endif
