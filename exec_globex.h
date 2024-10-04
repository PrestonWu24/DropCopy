#ifndef __EXEC_GLOBEX_HPP
#define __EXEC_GLOBEX_HPP


/********************/
/* constants		*/
/********************/

#include <queue>
#include <map>
#include <string>
#include <fstream>
#include "globex_common.h"

class exec_globex
{
public:
		
	exec_globex();
	~exec_globex();
	
	// data
	
	// function
	// ---------- exec_globex.cpp ----------
	int globex_start();
    
	// ---------- exec_globex.cpp end ----------
	
	// ---------- exec_globex_cmd.cpp ----------
    bool start();
	int send_login();
	int send_test_request();
	int send_logout();
	int send_reset_seq_login();
	int send_heartbeat(const char*);
	int send_resend_request(int, int);
	int send_sequence_reset_due_to_gap(int);
	int send_session_id_request();
	int send_appl_resend_request(string, int, int);
	int send_last_appl_request();
    int modify_last_seq(char* passed_name, int passed_seq);
	void finalizeSequence();
	bool compareSequence(int _sequence);
	// ---------- exec_globex_cmd.cpp end ----------

private:

	// data
	static char DELIM_STRING[];
	static int DELIM_STRING_LEN;
	fstream seqfile;
	fstream checkpoint_file;

	char login_name[32];
	char login_pass[32];
		
	char login_order_host[16];    // [nhy] fix server host
	int login_order_port;         // [nhy] fix server port
	int order_des;                // [nhy] socket, connect to fix server
		
	char globex_account[20];
	login_state_type login_state;

	pthread_t order_thread;
	pthread_t heart_thread;
		
	int order_message_num;
	int recv_order_message_num;
	int last_request_seq_num;
	int num_session_ids;
	int num_send_req;

	int fix_reserve_seq_num;
	int appl_reserve_seq_num;
    int before_apply_appl_reserve_seq;
    bool finish_tt_init_position;

	session_last_seq_struct *last_seq_list;
	session_last_seq_struct *end_last_seq_list;
    int appl_resend_reserve_last_seq;
	int appl_resend_request_up_num;
	
	static int local_order_num;
	int globex_seq_order_id;    // tag 11, ClOrdID
	int dropcopy_req_id;
	int checkpoint_num;
	
	int send_heartbeat_flag;
	bool dropcopy_appl_resend;  // application resend request
	bool need_appl_resend;
	bool begin_recv_appl_resend;
	int is_gap_fill; // fix resend request
	std::queue<fix_message_struct*> fixReserveQueue;
    std::queue<fix_message_struct*> applReserveQueue;

	// -------- exec_globex.cpp --------
	int connet_to_order();
	static void* run_order(void*);
	static void* set_heart_beat(void*);
	//void split_recv_buffer(char* _parse_buf, int _parse_len, char* _leave_buf, int& _leave_len);
    void split_recv_buffer();
	void body_len(char*& len_ptr, int& t_write_byte_count);
	int addToFixReserve(fix_message_struct*);
    int addToApplReserve(fix_message_struct*);
	int applyFixReserve();
	int applyApplReserve();
	int parse_field(fix_message_struct*&, const int& new_seq_num);
	int check_sequence(fix_message_struct*& fix_msg);
	int find_last_seq(const char* passed_name);
	string globex_get_session(fix_message_struct*& fix_msg);
	int globex_gap_fill_curr_seq(fix_message_struct*& fix_msg);
	int globex_get_last_seq(fix_message_struct*& fix_msg);
	void print_sessions();
	void initSequence();
	bool checkpoint();
	bool send_checkpoint_request();

	// -------- exec_globex.cpp, end --------
	
	// ---------- exec_globex_cmd.cpp ----------
	inline int build_field_int(char* _buffer, const int _tag, int _value)
	{
		int size = 0;
		char* ptr = _buffer;
		size = sprintf(ptr, "%d=%d", _tag, _value);
		ptr += size;
		*ptr = SOH;
		return size + 1;
	}
	inline int build_field_string(char* _buffer, const int _tag, const char* _value)
	{
		int size = 0;
		char* ptr = _buffer;
		size = sprintf(ptr, "%d=%s", _tag, _value);
		ptr += size;
		*ptr = SOH;
		return size + 1;
	}
	
	int make_fix_header(char *write_buffer, char **write_ptr, int body_size, const char *msg_type, 
                        int is_resend, int send_orig_sending_time);

	int make_fix_trailer(char *write_buffer, char **write_ptr);
	int send_to_order_des(char *write_buffer, int bytes_to_write);
	
 
	// ---------- exec_globex_cmd.cpp end ----------
	
	// ---------- exec_globex_parse.cpp ----------
	int parse_fix(char *buffer, char **buffer_ptr);
	int get_fix_field(fix_message_struct *fix_msg, const char *look_for, 
		              fix_field_struct *ret_field);
	int get_next_fix_field(fix_message_struct *fix_msg, const char *look_for, 
		fix_field_struct *ret_field, int next_num);

	int parse_fix_get_field(char *buffer, char **buffer_ptr, int *field_size, 
		                    fix_field_struct **fix_field);

	int parse_fix_get_message(char *buffer, char **buffer_ptr, 
                              fix_message_struct **fix_msg);
	
	void print_fix_struct(fix_message_struct*);
	int	free_fix_struct(fix_message_struct*);
	int handle_globex_login(fix_message_struct*);
	int handle_globex_logout(fix_message_struct*);
	int handle_globex_heartbeat_request(fix_message_struct*);
	int handle_globex_resend_request(fix_message_struct*);
	int handle_globex_session_reject(fix_message_struct*);
	int handle_globex_sequence_request(fix_message_struct*);
	int handle_globex_order_message(fix_message_struct*);
	int handle_globex_cancel_reject_message(fix_message_struct*);
	int handle_globex_business_reject_message(fix_message_struct*);
	int handle_globex_quote_acknowledge(fix_message_struct *fix_msg);
	int handle_globex_request_acknowledge(fix_message_struct *fix_msg);
	int handle_globex_resend_ack(fix_message_struct *fix_msg);
	// ---------- exec_globex_parse.cpp, end ----------

};

#endif
