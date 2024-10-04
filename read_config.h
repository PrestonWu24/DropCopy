
#ifndef _ET_READ_CONFIG_H_
#define _ET_READ_CONFIG_H_


#include <string>
#include "globex_common.h"
using namespace std;

class EtReadConfig
{
public:

	// ---- function ----
	static bool loadConfigFile(const char* _configFile);
	
	// ---- data ----
	static int m_listenPort;
	static int m_session_num;
	static string m_session_id;
	static int m_checkpoint;
	
	static string m_dropCopyServerIp;
	static int m_dropCopyServerPort;
		
private:
	
};


#endif
