
#include <iostream>
#include <errno.h>

#include "trace_log.h"
#include "util.h"
#include "exec_globex.h"
#include "package_store.h"
#include "read_config.h"

using namespace std;


char exec_globex::DELIM_STRING[] = {SOH};
int exec_globex::DELIM_STRING_LEN = 1;


// login to fix server
int exec_globex::send_login()
{
	int size = 0;
	char body_buffer[ORDER_BUFFER_SIZE];
	memset(body_buffer, 0, ORDER_BUFFER_SIZE);
	char* body_ptr = body_buffer;
	
	//DBG_DEBUG("name: %s.", login_name);

	// password len
	size = build_field_int(body_ptr, 95, (int)strlen(login_pass));
	body_ptr += size;

	// password
	size = build_field_string(body_ptr, 96, login_pass);
	body_ptr += size;

	// encrypt type
	size = build_field_int(body_ptr, 98, 0);
	body_ptr += size;

	// heart beat method
	size = build_field_string(body_ptr, 108, GLOBEX_HEART_BEAT_INTERVAL);
	body_ptr += size;

    char write_buffer[ORDER_BUFFER_SIZE];
    char* write_ptr = write_buffer;
    memset(write_buffer, 0, ORDER_BUFFER_SIZE);
    
    // header
	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "A", 0, 0);
	
	// copy body
	memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	
	// trailer
	make_fix_trailer(write_buffer, &write_ptr);

	size = write_ptr - write_buffer;
	
	DBG_DEBUG("login_str: %s", write_buffer);
	
	int iRes = 0;
	iRes = send_to_order_des(write_buffer, size);
	if (iRes == 0)
	{
		login_state = SENT_LOGIN;
	}
	else
	{
		DBG_ERR("fail to send the login buffer.");
	}
	
	return iRes;
}


int exec_globex::send_reset_seq_login(void)
{
	int size = 0;	
	char body_buffer[ORDER_BUFFER_SIZE];
	memset(body_buffer, 0, ORDER_BUFFER_SIZE);
	char* body_ptr = body_buffer;


    // 95 = RawDataLength, password length
	size = build_field_int(body_ptr, 95, (int)strlen(login_pass));
	body_ptr += size;

	// 96 = RawData, password
	size = build_field_string(body_ptr, 96, login_pass);
	body_ptr += size;

	// encrypt type
	size = build_field_int(body_ptr, 98, 0);
	body_ptr += size;

	// heart beach method
	size = build_field_string(body_ptr, 108, GLOBEX_HEART_BEAT_INTERVAL);
	body_ptr += size;

	// reset seq num
	order_message_num = 1;
	size = build_field_string(body_ptr, 141, "Y");
	body_ptr += size;
	
	
	char write_buffer[ORDER_BUFFER_SIZE];
	char* write_ptr = write_buffer;

    // header
	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "A", 0, 0);
	// copy body
	memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	// trailer
	make_fix_trailer(write_buffer, &write_ptr);

	size = write_ptr - write_buffer;
	DBG_DEBUG("login_reset_seq_str: %s", write_buffer);

	return (send_to_order_des(write_buffer, size));
}


int exec_globex::send_logout()
{
	int size = 0;
	char body_buffer[ORDER_BUFFER_SIZE];	
	char* body_ptr = body_buffer;

	memset(body_buffer, 0, ORDER_BUFFER_SIZE);

	// text
	size = build_field_string(body_ptr, 58, "logging out");
	body_ptr += size;

	/*
	// next_seq_num
	size = build_field_int(body_ptr, 789, 1);
	body_ptr += size;    
	*/

	char write_buffer[ORDER_BUFFER_SIZE];
	char* write_ptr = write_buffer;
	memset(write_buffer, 0, ORDER_BUFFER_SIZE);
    
	// header
	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "5", 0, 0);
	// copy body
	memcpy (write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	// trailer
	make_fix_trailer(write_buffer, &write_ptr);

	size = write_ptr - write_buffer;
	
	DBG_DEBUG("logout_str: %s", write_buffer);

	int iRes = send_to_order_des(write_buffer, size);
	if (-1 == iRes)
	{
		DBG_ERR("fail to send logout buffer.");
	}
	return iRes;
}


int exec_globex::send_heartbeat(const char *id_string)
{
	char write_buffer[0xff];
	char body_buffer[ORDER_BUFFER_SIZE];
	char tmp_buffer[0xff];
	
	char* write_ptr = NULL;
	char* body_ptr = NULL;
	char* curr_str = NULL;
	
	int bytes_to_write = 0;
	
	if (id_string == NULL)
	{
		DBG_ERR("id_string is NULL.");
		return -1;
	}
	
	body_ptr = body_buffer;

	sprintf (tmp_buffer, "112=%s", id_string);
	curr_str = tmp_buffer;
	memcpy (body_ptr, curr_str, strlen(curr_str));
	body_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;

	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "0", 0, 0);
	memcpy (write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	make_fix_trailer(write_buffer, &write_ptr);

	bytes_to_write = write_ptr - write_buffer;
	
	DBG_DEBUG("heartbeat_str: %s", write_buffer);

	return (send_to_order_des (write_buffer, bytes_to_write));
}


int exec_globex::send_test_request()
{
	char write_buffer[ORDER_BUFFER_SIZE];
	char body_buffer[ORDER_BUFFER_SIZE];
	char tmp_buffer[0xff];

	char* write_ptr = NULL;
	char* body_ptr = NULL;
	char* curr_str = NULL;

	int bytes_to_write = 0;
	body_ptr = body_buffer;

	/* id */
	sprintf(tmp_buffer, "112=%s", "ARE YOU THERE");
	curr_str = tmp_buffer;
	memcpy(body_ptr, curr_str, strlen(curr_str));
	body_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;

	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "1", 0, 0);
	memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	make_fix_trailer(write_buffer, &write_ptr);

	bytes_to_write = write_ptr - write_buffer;
	
	DBG_DEBUG("test_request_str: %s", write_buffer);

	int iRes = send_to_order_des(write_buffer, bytes_to_write);
	if (-1 == iRes)
	{
		DBG_ERR("fail to send test request buffer.");
	}
	return iRes;
}


int exec_globex::send_resend_request(int start, int end)
{
	char write_buffer[ORDER_BUFFER_SIZE];
	char body_buffer[ORDER_BUFFER_SIZE];
	char tmp_buffer[0xff];
	
	char *write_ptr = NULL;
	char *body_ptr = body_buffer;
	char *curr_str = NULL;
	
	int bytes_to_write = 0;

	/* start num */
	sprintf (tmp_buffer, "7=%i", start);
	curr_str = tmp_buffer;
	memcpy (body_ptr, curr_str, strlen (curr_str));
	body_ptr += strlen (curr_str);

	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;

	/* end num */
	sprintf (tmp_buffer, "16=%i", end);
	curr_str = tmp_buffer;
	memcpy (body_ptr, curr_str, strlen (curr_str));
	body_ptr += strlen (curr_str);

	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;

	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "2", 0, 0);
	memcpy (write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	make_fix_trailer(write_buffer, &write_ptr);

	bytes_to_write = write_ptr - write_buffer;
	
	DBG_DEBUG("resend_request_str: %s", write_buffer);

	return (send_to_order_des (write_buffer, bytes_to_write));
}


int exec_globex::send_sequence_reset_due_to_gap(int new_seq)
{
	char write_buffer[ORDER_BUFFER_SIZE];
	char body_buffer[ORDER_BUFFER_SIZE];
	char tmp_buffer[0xff];
	
	char* write_ptr = NULL;
	char* body_ptr = body_buffer;
	char* curr_str = NULL;
	
	int bytes_to_write = 0;
	int save_msg_num = 0;

	/* NewSeqNo */
	sprintf(tmp_buffer, "36=%d", order_message_num);
	curr_str = tmp_buffer;
	memcpy(body_ptr, curr_str, strlen(curr_str));
	body_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;

	/* GapFillFlag */
	sprintf(tmp_buffer, "123=%s", "Y");
	curr_str = tmp_buffer;
	memcpy(body_ptr, curr_str, strlen(curr_str));
	body_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;
	
	save_msg_num = order_message_num;
	order_message_num = new_seq;
	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "4", 1, 1);
	order_message_num = save_msg_num;

	memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	make_fix_trailer(write_buffer, &write_ptr);

	bytes_to_write = write_ptr - write_buffer;
	
	DBG_DEBUG("buffer: %s.", write_buffer);

	return (send_to_order_des (write_buffer, bytes_to_write));
}

int exec_globex::send_session_id_request()
{
	char write_buffer[ORDER_BUFFER_SIZE];
	char body_buffer[ORDER_BUFFER_SIZE];
	
	char *write_ptr = NULL;
	char* body_ptr = NULL;
	char *curr_str = NULL;
	
	char tmp_buffer[0xff];
	
	int	bytes_to_write;

	body_ptr = body_buffer;
	memset(body_ptr, 0, ORDER_BUFFER_SIZE);

	// ApplReqID
	sprintf(tmp_buffer, "1346=%d", dropcopy_req_id++);
	curr_str = tmp_buffer;
	memcpy(body_ptr, curr_str, strlen(curr_str));
	body_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;

	// ApplReqType
	sprintf(tmp_buffer, "1347=%d", 3);
	curr_str = tmp_buffer;
	memcpy(body_ptr, curr_str, strlen(curr_str));
	body_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;

	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "BW", 0, 0);
	memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	make_fix_trailer(write_buffer, &write_ptr);

	bytes_to_write = write_ptr - write_buffer;
	
	DBG_DEBUG("dropcopy_str: %s.", write_buffer);

	return (send_to_order_des (write_buffer, bytes_to_write));
}

int exec_globex::send_appl_resend_request(string session, int begin_seq, int end_seq)
{
	char write_buffer[ORDER_BUFFER_SIZE];
	char body_buffer[ORDER_BUFFER_SIZE];
	
	char *write_ptr = NULL;
	char* body_ptr = NULL;
	char *curr_str = NULL;
	
	char tmp_buffer[0xff];
	
	int	bytes_to_write;

	body_ptr = body_buffer;
	memset(body_ptr, 0, ORDER_BUFFER_SIZE);
	memset(write_buffer, 0, ORDER_BUFFER_SIZE);

	// ApplReqID
	sprintf(tmp_buffer, "1346=%d", dropcopy_req_id++);
	curr_str = tmp_buffer;
	memcpy(body_ptr, curr_str, strlen(curr_str));
	body_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;

	// ApplReqType
	sprintf(tmp_buffer, "1347=%d", 0);
	curr_str = tmp_buffer;
	memcpy(body_ptr, curr_str, strlen(curr_str));
	body_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;	

	// NoApplIds
	sprintf(tmp_buffer, "1351=%d", 1);
	curr_str = tmp_buffer;
	memcpy(body_ptr, curr_str, strlen(curr_str));
	body_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;

	// RefApplID
	sprintf(tmp_buffer, "1355=%s", session.c_str());
	curr_str = tmp_buffer;
	memcpy(body_ptr, curr_str, strlen(curr_str));
	body_ptr += strlen(curr_str);
	
	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;	
	
	// ApplBeginSeqNo
	sprintf(tmp_buffer, "1182=%d", begin_seq);
	curr_str = tmp_buffer;
	memcpy(body_ptr, curr_str, strlen(curr_str));
	body_ptr += strlen(curr_str);
	
	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;
	
	// ApplEndSeqNo
	sprintf(tmp_buffer, "1183=%d", end_seq);
	curr_str = tmp_buffer;
	memcpy(body_ptr, curr_str, strlen(curr_str));
	body_ptr += strlen(curr_str);
	
	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;
	
	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "BW", 0, 0);
	memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	make_fix_trailer(write_buffer, &write_ptr);

	bytes_to_write = write_ptr - write_buffer;
	
	DBG_DEBUG("dropcopy_str: %s.", write_buffer);

	return (send_to_order_des (write_buffer, bytes_to_write));
}

int exec_globex::send_last_appl_request()
{
	char write_buffer[ORDER_BUFFER_SIZE];
	char body_buffer[ORDER_BUFFER_SIZE];
	
	char *write_ptr = NULL;
	char* body_ptr = NULL;
	char *curr_str = NULL;
	
	char tmp_buffer[0xff];
	
	int	bytes_to_write;

	body_ptr = body_buffer;
	memset(body_ptr, 0, ORDER_BUFFER_SIZE);
	memset(write_buffer, 0, ORDER_BUFFER_SIZE);

	// ApplReqID
	sprintf(tmp_buffer, "1346=%d", dropcopy_req_id++);
	curr_str = tmp_buffer;
	memcpy(body_ptr, curr_str, strlen(curr_str));
	body_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;

	// ApplReqType
	sprintf(tmp_buffer, "1347=%d", 2);
	curr_str = tmp_buffer;
	memcpy(body_ptr, curr_str, strlen(curr_str));
	body_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;	

	// NoApplIDs
	sprintf(tmp_buffer, "1351=%d", EtReadConfig::m_session_num);
	curr_str = tmp_buffer;
	memcpy(body_ptr, curr_str, strlen(curr_str));
	body_ptr += strlen(curr_str);	

	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;

	// RefApplID
	sprintf(tmp_buffer, "1355=%s", EtReadConfig::m_session_id.c_str());
	curr_str = tmp_buffer;
	memcpy(body_ptr, curr_str, strlen(curr_str));
	body_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy (body_ptr, curr_str, DELIM_STRING_LEN);
	body_ptr += DELIM_STRING_LEN;	

	make_fix_header(write_buffer, &write_ptr, body_ptr - body_buffer, "BW", 0, 0);
	memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
	write_ptr += (body_ptr - body_buffer);
	make_fix_trailer(write_buffer, &write_ptr);

	bytes_to_write = write_ptr - write_buffer;
	
	DBG_DEBUG("dropcopy_str: %s.", write_buffer);

	return (send_to_order_des (write_buffer, bytes_to_write));
}

/**
 * send buffer to order server.
 *    0: ok
 *   -1: error
 */
int exec_globex::send_to_order_des(char* write_buffer, int bytes_to_write)
{
	if (write_buffer == NULL || bytes_to_write <= 0)
	{
		DBG_ERR("parameter is error.");
		return -1;
	}
	
	char* write_buffer_ptr = NULL;
	char* end_ptr = NULL;
	int bytes_written = 0;

	write_buffer_ptr = write_buffer;
	end_ptr = write_buffer + bytes_to_write;
	
	while (write_buffer_ptr < end_ptr)
	{
		bytes_written = write(order_des, write_buffer_ptr, (end_ptr - write_buffer_ptr));
		if (bytes_written <= 0)
	    {
			DBG_ERR("fail to write to des = %d, errno = %d, (%s).", order_des, errno, strerror(errno));
			close(order_des);
			order_des = -1;
			return(-1);
	    }
		write_buffer_ptr += bytes_written;
	}
	
	return (0);
}


int exec_globex::make_fix_header(char *write_buffer, char **write_ptr, 
	int body_size, const char *msg_type, int is_resend, int send_orig_sending_time)
{
	char time_buffer[0x4f];
	char seq_buffer[0xf];
	char tmp_buffer[0xff];
	int t_write_byte_count = 0;
	
	char *len_ptr = NULL;
	const char *curr_str = NULL;
	char *start_count_ptr = NULL;
	char *end_count_ptr = NULL;

	*write_ptr = write_buffer;

	/* begin string */
	curr_str = "8=FIX.4.2";
	memcpy(*write_ptr, curr_str, strlen(curr_str));
	*write_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy(*write_ptr, curr_str, DELIM_STRING_LEN);
	*write_ptr += DELIM_STRING_LEN;

	/* body len */
	curr_str = "9=";
	memcpy(*write_ptr, curr_str, strlen(curr_str));
	*write_ptr += strlen(curr_str);
	len_ptr = *write_ptr;
	memset(*write_ptr, 'x', 6);
	*write_ptr += 6;

	curr_str = DELIM_STRING;
	memcpy (*write_ptr, curr_str, DELIM_STRING_LEN);
	*write_ptr += DELIM_STRING_LEN;
	start_count_ptr = *write_ptr;

	/* msg type */
	sprintf(tmp_buffer, "35=%s", msg_type);
	curr_str = tmp_buffer;
	memcpy(*write_ptr, curr_str, strlen(curr_str));
	*write_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy(*write_ptr, curr_str, DELIM_STRING_LEN);
	*write_ptr += DELIM_STRING_LEN;

	/* sequence number */
	sprintf(seq_buffer, "34=%d", order_message_num);
	order_message_num++;
	
	curr_str = seq_buffer;
	memcpy(*write_ptr, curr_str, strlen(curr_str));
	*write_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy (*write_ptr, curr_str, DELIM_STRING_LEN);
	*write_ptr += DELIM_STRING_LEN;

	/* is dup */
	if (is_resend)
	{
		sprintf(tmp_buffer, "43=%s", "Y");
	}
	else
	{
		sprintf(tmp_buffer, "43=%s", "N");
	}

	curr_str = tmp_buffer;
	memcpy (*write_ptr, curr_str, strlen(curr_str));
	*write_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy (*write_ptr, curr_str, DELIM_STRING_LEN);
	*write_ptr += DELIM_STRING_LEN;

	/* SenderCompID */
/*
	// *start* change FIRM ID, route through test only	
	if(msg_type == "D")
	{
		char firmid[5];
		memset(firmid, 0, 5);
		cout << "Enter the firm id (826 or 033): ";
		cin >> firmid;	
		strcat(CompId, firmid);
	}
	else
	{
		strcat(CompId, FIRM_ID);
	}
	// *end* chage FIRM ID, route through test only	
*/

	sprintf(tmp_buffer, "49=%s", DROPCOPY_SESSION_ID);
	curr_str = tmp_buffer;
	memcpy(*write_ptr, curr_str, strlen(curr_str));
	*write_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy(*write_ptr, curr_str, DELIM_STRING_LEN);
	*write_ptr += DELIM_STRING_LEN;

	/* SenderSubID */
	sprintf(tmp_buffer, "50=%s", login_name);
	curr_str = tmp_buffer;
	memcpy(*write_ptr, curr_str, strlen(curr_str));
	*write_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy(*write_ptr, curr_str, DELIM_STRING_LEN);
	*write_ptr += DELIM_STRING_LEN;

    /* sending time */
    memset(time_buffer, 0, sizeof(time_buffer));
    EtUtil::build_time_buffer(time_buffer, sizeof(time_buffer));
	sprintf(tmp_buffer, "52=%s", time_buffer);
	curr_str = tmp_buffer;
	memcpy (*write_ptr, curr_str, strlen(curr_str));
	*write_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy (*write_ptr, curr_str, DELIM_STRING_LEN);
	*write_ptr += DELIM_STRING_LEN;

	/* target id */
	curr_str = "56=CME";
	memcpy(*write_ptr, curr_str, strlen(curr_str));
	*write_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy (*write_ptr, curr_str, DELIM_STRING_LEN);
	*write_ptr += DELIM_STRING_LEN;

	/* target id */
	curr_str = "57=G";
	memcpy(*write_ptr, curr_str, strlen(curr_str));
	*write_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy(*write_ptr, curr_str, DELIM_STRING_LEN);
	*write_ptr += DELIM_STRING_LEN;

	if (send_orig_sending_time)
	{
		sprintf(tmp_buffer, "122=%s", time_buffer);
		curr_str = tmp_buffer;
		memcpy(*write_ptr, curr_str, strlen(curr_str));
		*write_ptr += strlen(curr_str);

		curr_str = DELIM_STRING;
		memcpy (*write_ptr, curr_str, DELIM_STRING_LEN);
		*write_ptr += DELIM_STRING_LEN;
	}

	/* location id */
	curr_str = "142=DropCopy";
	memcpy(*write_ptr, curr_str, strlen(curr_str));
	*write_ptr += strlen(curr_str);

	curr_str = DELIM_STRING;
	memcpy(*write_ptr, curr_str, DELIM_STRING_LEN);
	*write_ptr += DELIM_STRING_LEN;
	end_count_ptr = *write_ptr;

	t_write_byte_count = ((end_count_ptr - start_count_ptr) + body_size);
	body_len(len_ptr, t_write_byte_count);
	
	return (0);
}

void exec_globex::body_len(char*& len_ptr, int& t_write_byte_count)
{
    len_ptr[0] = (char)((t_write_byte_count / 100000) + 0x30);
    t_write_byte_count = t_write_byte_count % 100000;
    len_ptr[1] = (char)((t_write_byte_count / 10000) + 0x30);
    t_write_byte_count = t_write_byte_count % 10000;
    len_ptr[2] = (char)((t_write_byte_count / 1000) + 0x30);
    t_write_byte_count = t_write_byte_count % 1000;
    len_ptr[3] = (char)((t_write_byte_count / 100) + 0x30);
    t_write_byte_count = t_write_byte_count % 100;
    len_ptr[4] = (char)((t_write_byte_count / 10) + 0x30);
    t_write_byte_count = t_write_byte_count % 10;
    len_ptr[5] = (char)((t_write_byte_count / 1) + 0x30);
}

int exec_globex::make_fix_trailer(char *write_buffer, char **write_ptr)
{
	char* curr_ptr = NULL;
    int t_write_byte_count = 0;
	int int_check_sum = 0;
	unsigned char char_check_sum;
	const char *curr_str = NULL;
	
	char check_sum[3] = { 0, 0, 0};

	for (curr_ptr = write_buffer; curr_ptr < *write_ptr; curr_ptr++)
	{
		int_check_sum += *curr_ptr;
	}

	/* location id */
	curr_str = "10=";
	memcpy(*write_ptr, curr_str, strlen(curr_str));
	*write_ptr += strlen(curr_str);

	char_check_sum = (unsigned char)int_check_sum;
	t_write_byte_count = char_check_sum;

	check_sum[0] = (char)((t_write_byte_count / 100) + 0x30);
	t_write_byte_count = t_write_byte_count % 100;
	check_sum[1] = (char)((t_write_byte_count / 10) + 0x30);
	t_write_byte_count = t_write_byte_count % 10;
	check_sum[2] = (char)((t_write_byte_count / 1) + 0x30);

	curr_str = check_sum;
	memcpy(*write_ptr, curr_str, sizeof(check_sum));
	*write_ptr += sizeof(check_sum);

	curr_str = DELIM_STRING;
	memcpy(*write_ptr, curr_str, DELIM_STRING_LEN);
	*write_ptr += DELIM_STRING_LEN;

	return (0);
}

