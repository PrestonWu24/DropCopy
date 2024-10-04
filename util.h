
#ifndef _UTIL_H_
#define _UTIL_H_


#include <string>
using namespace std;


class EtUtil
{
public:
	
	/**
	 * build a TCP connection.
	 * return:
	 *   >0: socket code
	 *   -1: error
	 */
	static int connect_client(const char* _ipAddr, int _port);

	/**
	 * set a socket with non block.
	 * return:
	 *   0: OK
	 *  -1: error
	 */
	static int set_non_block(int des);

	/**
	 * convert current time to buffer.
	 */
	static bool build_time_buffer(char* _buffer, int _len);
	static bool build_weekday(char* _buffer, int _len);
	static bool build_date(char* _buffer, int _len);
	static bool build_date(char* _buffer, int _len, int _days);
	static int diff_time(char* _buffer);
	static int get_month_date(int _month, int _year);
	static bool is_leapyear(int _year);

	static bool sendData(const int _iSockFd, const char *_buffer, const int _iLength);

	static bool checkLock(int* fileid, char* _fileName);
	static bool releaseLock(int fileid);
	
	/**
     * CL: 100
     * HO, RB: 10000
     */
	static int doubleToInt(double _dVal, int _dot_count);
	static double intToDouble(int _inum, int _dot_count);
	
	static bool doubleToInt(const char* _symbol, double _dVal, int& _iVal);
	static bool convertSymbol(const char* _fixSymbol, char* _etSymbol);
	
	static string intToStr(int _val);
	static int strToInt(string _str);
	static string doubleToStr(double _val);
	static double strToDouble(string _str);
	
	static void trimSpace(char *_str);

private:
	
	int static m_globexTimeShift;

};

#endif
