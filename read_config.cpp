
#include "trace_log.h"
#include "util.h"
#include "read_config.h"

	
int EtReadConfig::m_listenPort = -1;
int EtReadConfig::m_session_num = -1;
string EtReadConfig::m_session_id = "";
int EtReadConfig::m_checkpoint = -1;
	
string EtReadConfig::m_dropCopyServerIp = "";
int EtReadConfig::m_dropCopyServerPort = -1;


bool EtReadConfig::loadConfigFile(const char* _configFile)
{
    FILE *fd;
    char sBuf[128];
    char* ptr = NULL;
    int paramCount = 0;
    
    char param[32];
    char val[32];

	fd = fopen(_configFile, "r");
	if (NULL == fd)
	{
		DBG(TRACE_LOG_ERR, "Failed to open configuration file: %s", _configFile);
		return false;
	}

	memset(sBuf, 0, 128);
	while (fgets(sBuf, 128, fd) != NULL)
	{
		// line is too long and do not continue to read the next line
		if (strlen(sBuf) > 127)
		{
			DBG(TRACE_LOG_ERR, "A line in the config is too long.");
			break;
		}
		
		EtUtil::trimSpace(sBuf);
		
		// comment line or a line with space only
		if (('#' == sBuf[0]) || ('\0' == sBuf[0])) 
		{
			continue;
		}
		
		ptr = strchr(sBuf, '=');
		if (ptr == NULL)
		{
			DBG_ERR("do not find = sign.");
			break;
		}
		
		memset(param, 0, 32);
		memset(val, 0, 32);
		strncpy(param, sBuf, ptr - sBuf);
		ptr++;
		strcpy(val, ptr);
		
		if (strcmp(param, "drop_copy_server_ip") == 0)
		{
			m_dropCopyServerIp = val;
			paramCount++;
		}
		else if (strcmp(param, "drop_copy_server_port") == 0)
		{
			m_dropCopyServerPort = atoi(val);
			paramCount++;
		}
		else if (strcmp(param, "listen_port") == 0)
		{
			m_listenPort = atoi(val);
			paramCount++;
		}
		else if(strcmp(param, "session_num") == 0){
          m_session_num = atoi(val);
          paramCount++;
        }
        else if (strcmp(param, "session") == 0){
          m_session_id = val;
          paramCount++;
        }
        else if (strcmp(param, "checkpoint") == 0)
          {
            m_checkpoint = atoi(val);
            paramCount++;
          }
		else
		{
			DBG_ERR("parameter is error: %s", param);
			break;
		}
	}

	fclose(fd);
	
	if (paramCount >= 6)
	{
		return true;
	}
	
	DBG_ERR("parameter count is error.");
	return false;
}
