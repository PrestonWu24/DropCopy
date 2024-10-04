
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>

#include "trace_log.h"
#include "util.h"

#include <sstream>
using namespace std;


int EtUtil::m_globexTimeShift = 0;

int EtUtil::connect_client(const char* _ipAddr, int _port)
{
	int cliSocket = 0;
	struct sockaddr_in server_address;
	
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port	= htons(_port);
    
	if (inet_aton(_ipAddr, &server_address.sin_addr) == 0)
	{
		DBG_ERR("fail to get a IP address, %s.", _ipAddr);
		return -1;
	}
	
	if ((cliSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	    {
		DBG_ERR("fail to create a socket. error number: %d", errno);
		return -1;
	    }
    DBG_DEBUG("created socket.");
    
	if (connect(cliSocket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) 
	    {
		DBG_ERR("fail to connect to server, ip: %s, port: %d, error: %d", 
			_ipAddr, _port, errno);
		close(cliSocket);
		return -1;
	    }
 
	DBG_DEBUG("connected to %s, %d", _ipAddr, _port);
	return (cliSocket);
}


int EtUtil::set_non_block(int des)
{
	if (-1 == fcntl(des, F_SETFL, O_NONBLOCK))
    {
		DBG_ERR("fail to set no block, error number: %d (%s)", errno, strerror(des));
		return (-1);
    }
	return (0);
}


bool EtUtil::build_time_buffer(char* _buffer, int _len)
{
	struct timeval tv;
	struct tm local_time;
	time_t second;

	char time_format[] = {
		'%', 'Y',
		'%', 'm',
		'%', 'd', '-',
		'%', 'H', ':',
		'%', 'M', ':',
		'%', 'S', 0 };

	gettimeofday(&tv, NULL);
	
	second = tv.tv_sec + m_globexTimeShift;
	
	//memcpy(&local_time, localtime(&second), sizeof(local_time));
	memcpy(&local_time, gmtime(&second), sizeof(local_time));
	strftime(_buffer, _len, time_format, &local_time);	
	
	char tmp[10];
	memset(tmp, 0, 10);
	sprintf(tmp, ".%03d", (int)(tv.tv_usec / 1000));
	strcat(_buffer, tmp);

	//DBG_DEBUG("UTC time: %s", _buffer);
	return true;
}


bool EtUtil::sendData(const int _iSockFd, const char *_buffer, const int _iLength)
{
	if (_iSockFd < 0 || _buffer == NULL || _iLength < 0)
	{
	    DBG(TRACE_LOG_ERR, "The parameter is error.");
	    return false;
	}
	
	const char *cPtr = _buffer;
	int iTotalLen = 0;
	int iSendLen = 0;
	
	//DBG_DEBUG("begin send data: _iLength= %d", _iLength);
	while (iTotalLen < _iLength)
	{
		iSendLen = send(_iSockFd, cPtr, _iLength - iTotalLen, 0);
		if (iSendLen < 0)
		{
			DBG(TRACE_LOG_ERR, "Failed to send data to server. %s", strerror(errno));
			return false;
		}
		//DBG_DEBUG("send: %s", cPtr);
		iTotalLen += iSendLen;
		cPtr += iSendLen;
	}
	return true;
}


/** 
 * check Lock function 
 *    true: ok
 *   false: error
 */
bool EtUtil::checkLock(int* fileid, char* _fileName)
{
	if ((*fileid = open(_fileName, O_WRONLY)) == -1)
	{
		printf("Failed to open the lock file: %s. error: %s\n", 
			_fileName, strerror(errno));
		return false;
	}

	if (lockf(*fileid, F_TLOCK, 0) == -1)
	{
		printf("Failed to lock the file: %s, eror: %s\n", _fileName, 
			strerror(errno));
		if (close(*fileid) == -1)
		{
			printf("Failed to close the lock file: %s. error: %s\n", 
				_fileName, strerror(errno));
		}		
		return false;
	}
	else
	{
		return true;
	}
}


bool EtUtil::releaseLock(int fileid)
{    
	if (lockf(fileid, F_ULOCK , 0) == -1)
	{
	    DBG(TRACE_LOG_ERR, "Fail to unlock the lock file.");
		return false;
	}

	if (close(fileid) == -1) 
	{
		DBG(TRACE_LOG_ERR, "Fail to close the lock file.");
		return false;
	}
	return true;
}


int EtUtil::doubleToInt(double _dVal, int _dot_count)
{
	int inum = 0;
	char strNum[32];
	memset(strNum, 0, 32);

	sprintf(strNum, "%lf", _dVal * _dot_count);
	inum  = atol(strNum);
	return inum;
}


/**
 * CL: 100
 * HO, RB: 10000
 */
double EtUtil::intToDouble(int _inum, int _dot_count)
{
	double dnum = 0.0;
	dnum = (double)_inum / _dot_count;
	return dnum;
}


/**
 * int to string.
 */
string EtUtil::intToStr(int _val)
{
    stringstream ss;
    ss << _val;
    return ss.str();
}


/**
 * string to int.
 */
int EtUtil::strToInt(string _str) 
{
	int iVal = 0;
	stringstream ss(_str);
	ss >> iVal;
	return iVal;
}


/**
 * double to string.
 */
string EtUtil::doubleToStr(double _val)
{
    stringstream ss;
    ss << _val;
    return ss.str();
}


/**
 * string to double.
 */
double EtUtil::strToDouble(string _str)
{
	double dVal;
	stringstream ss(_str);
	ss >> dVal;
	return dVal;
}


/*
 * trim the space.
 */
void EtUtil::trimSpace(char *_str)
{
	long iFirst = 0;
	long iLast = 0;
	long iLen = 0;
	long i;
	
	iLen = strlen(_str);
	if (NULL == _str || 0L == iLen) { 
		return;
	}

	while (isspace(_str[iFirst])) { 
		iFirst++;
	}
	if (iFirst == iLen) {
		_str[0] = '\0';
		return;
	}

	iLast = iLen - 1;
	while (isspace(_str[iLast])) { 
		iLast--;
	}
	
	if (0 == iFirst && (iLen - 1) == iLast) { 
		return;
	}

	for (i = iFirst; i <= iLast; i++) {
		_str[i-iFirst] = _str[i];
	}
	_str[iLast-iFirst+1] = '\0';
	return;
}


/**
 * CL: 100
 * HO, RB: 10000
 */
bool EtUtil::doubleToInt(const char* _symbol, double _dVal, int& _iVal)
{
    //DBG_DEBUG("symbol: %s", _symbol);
	if (_symbol == NULL || strlen(_symbol) > 9 || strlen(_symbol) < 4)
	{
		DBG_ERR("symbol is error.");
		return false;
	}
	
	char product[3];
	strncpy(product, _symbol, 2);
	product[2] = '\0';
	
	int dotCount = 0;
	if (strcmp(product, "CL") == 0)
	{
		dotCount = 100;
	}
	else if (strcmp(product, "RB") == 0)
	{
		dotCount = 10000;
	}
	else if (strcmp(product, "HO") == 0)
	{
		dotCount = 10000;
	}
	else
	{
		DBG_ERR("product is error.");
		return false;
	}
	
	char strNum[32];
	memset(strNum, 0, 32);
	
	sprintf(strNum, "%lf", _dVal * dotCount);
	_iVal  = atol(strNum);
	return true;
}


bool EtUtil::convertSymbol(const char* _fixSymbol, char* _etSymbol)
{
	if (_fixSymbol == NULL || _etSymbol == NULL)
	{
		DBG_ERR("parameter is error.");
		return false;
	}

	//convert	
	// spread
	if(strlen(_fixSymbol) > 5)
	{
	    _etSymbol[0] = _fixSymbol[0];
	    _etSymbol[1] = _fixSymbol[1];
	    _etSymbol[2] = _fixSymbol[3];
	    _etSymbol[3] = _fixSymbol[2];
	    _etSymbol[4] = _fixSymbol[4];
	    _etSymbol[5] = _fixSymbol[5];
	    _etSymbol[6] = _fixSymbol[6];
	    _etSymbol[7] = _fixSymbol[8];
	    _etSymbol[8] = _fixSymbol[7];
	}
	// outright
	else
	{
		_etSymbol[0] = _fixSymbol[0];
		_etSymbol[1] = _fixSymbol[1];
		_etSymbol[2] = _fixSymbol[3];
		_etSymbol[3] = _fixSymbol[2];
	}
	
	//DBG_DEBUG("after convert, get: %s", _etSymbol);

	return true;
}

bool EtUtil::build_date(char* _buffer, int _len)
{
    time_t sec;
    sec = time(NULL);

    struct tm* timeinfo;
    timeinfo = localtime(&sec);
    strftime(_buffer, _len, "%Y%m%d", timeinfo);

    //DBG_DEBUG("last trading time = %s", _buffer);
    return true;
}

bool EtUtil::build_date(char* _buffer, int _len, int _days) 
{ 
    time_t sec; 
    sec = time(NULL); 
    sec -= _days * 24 * 3600; 
 
    struct tm* timeinfo; 
    timeinfo = localtime(&sec); 
    strftime(_buffer, _len, "%Y%m%d", timeinfo); 
 
    return true; 
}

bool EtUtil::build_weekday(char* _buffer, int _len)
{
    time_t sec;
    sec = time(NULL);

    struct tm* timeinfo;
    timeinfo = localtime(&sec);
    strftime(_buffer, _len, "%w", timeinfo);

    //DBG_DEBUG("last trading time = %s", _buffer);
    return true;
}

int EtUtil::diff_time(char* _buffer)
{
  //DBG_DEBUG("_buffer=%s", _buffer);

    struct tm* currtime;
    char temp[5];

    time_t sec;
    sec = time(NULL);
    currtime = localtime(&sec);

    memset(temp, 0, sizeof(temp));
    strncpy(temp, _buffer, 4);
    int year = atoi(temp);
    //DBG_DEBUG("year=%d, %d", year, currtime->tm_year + 1900);

    memset(temp, 0, sizeof(temp));
    strncpy(temp, _buffer+4, 2);
    int month = atoi(temp);
    //DBG_DEBUG("mon = %d, %d", month, currtime->tm_mon + 1);

    memset(temp, 0, sizeof(temp));
    strncpy(temp, _buffer+6, 2);
    int day = atoi(temp);
    DBG_DEBUG("day = %d, %d", day, currtime->tm_mday);

    int difference = -1;

    // do not consider multiple months and years
    if(year == currtime->tm_year + 1900)
	{
	    if(month == currtime->tm_mon + 1)
		{
		    difference = currtime->tm_mday - day;
		}
	    else
		{
		    int month_date = get_month_date(month, year);
		    difference = currtime->tm_mday + (month_date - day);
		}
	}
    else
	{
	    difference = currtime->tm_mday + (31 - day);
	}

    DBG_DEBUG("time difference is %d", difference);
    return difference;
}

int EtUtil::get_month_date(int _month, int _year)
{
    if(_month < 1 || _month > 12)
	{
	    DBG_DEBUG("month error!");
	    return -1;
	}
    if(_month % 2 == 1)
	{
	    return 31;
	}
    else if(_month == 2)
	{
	    if(is_leapyear(_year))
		{
		    return 29;
		}
	    else
		{
		    return 28;
		}
	}
    else 
	{
	    return 30;
	}
}

bool EtUtil::is_leapyear(int _year)
{
    bool leapyear = false;

    if (_year % 4 == 0) {
	if (_year % 100 == 0) {
	    if (_year % 400 == 0)
		leapyear = true;
	    else leapyear = false;
	}
	else leapyear = true;
    }
    else leapyear = false;

    return leapyear;
}
