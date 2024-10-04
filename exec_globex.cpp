#include <errno.h>
#include <assert.h>
#include "trace_log.h"
#include "util.h"
#include "exec_globex.h"
#include "globex_common.h"
#include "read_config.h"
#include "trade_socket_list.h"
#include "client_message.h"
#include "msg_protocol.h"

// globe
extern bool et_g_exitProcess;
extern EtTradeSocketList* g_socketListPtr;
// globe


int exec_globex::local_order_num = 0;

int recv_size; 
char recv_buffer[ORDER_BUFFER_SIZE]; 
int leave_size; 
char leave_buffer[ORDER_BUFFER_SIZE]; 
int parse_size; 
char parse_buffer[ORDER_BUFFER_SIZE * 2];

exec_globex::exec_globex()
{
	memset(login_name, 0, 32);
	memset(login_pass, 0, 32);
	
	memset(login_order_host, 0, 16);
	login_order_port = -1;
	order_des = -1;

	memset(globex_account, 0, 20);
	login_state = NOT_LOGGED_IN;

	recv_order_message_num = -1;
    before_apply_appl_reserve_seq = -1;
	globex_seq_order_id = 1;
	last_request_seq_num = -1;
	dropcopy_req_id = 1;
	dropcopy_appl_resend = false;
	need_appl_resend = false;
	begin_recv_appl_resend = false;
	num_send_req = 0;
	//curr_appl_seq_num = -1;
	is_gap_fill = 0;
    appl_resend_reserve_last_seq = -1;
	appl_resend_request_up_num = -1;
    finish_tt_init_position = false;

	fix_reserve_seq_num = -1;
	appl_reserve_seq_num = -1;

	checkpoint_num = 0;

	last_seq_list = NULL;
	end_last_seq_list = NULL;

	initSequence();
}


exec_globex::~exec_globex()
{
    DBG_DEBUG("destructor exec_globex. order_des = %d", order_des);
	if (order_des != -1)
	{
	    DBG_DEBUG("order_des != -1");
	    close(order_des);
	}

	finalizeSequence();
}

void exec_globex::initSequence()
{
    DBG_DEBUG("initSequence");
    num_session_ids = EtReadConfig::m_session_num;

    // read file for last saved sequence number
    seqfile.open("/usr/home/ascendant/auto-trading-v1/v2-dropcopy/database/sequence.txt", fstream::in);
    assert(seqfile.good());
    char buf[64];

    // if begin of week, need set sequence to 1
    seqfile.getline(buf, sizeof(buf));
    int diff = EtUtil::diff_time(buf);

    seqfile.getline(buf, sizeof(buf));
    int last_weekday = atoi(buf);

    seqfile.getline(buf, sizeof(buf));
    order_message_num = atoi(buf);
    seqfile.getline(buf, sizeof(buf));
    recv_order_message_num = atoi(buf);

    char date_buffer[2];
    memset(date_buffer, 0, sizeof(date_buffer));
    EtUtil::build_weekday(date_buffer, sizeof(date_buffer));
    int curr_weekday = atoi(date_buffer);

    DBG_DEBUG("last_weekday=%d, curr_weekday=%d, diff=%d", last_weekday, curr_weekday, diff);
    if(last_weekday > 1 && (curr_weekday < 2 || (curr_weekday > 2 && diff > curr_weekday ) ))
	{
	    // begin of the week, set sequence to 1
	    DBG_DEBUG("begin of week");
	    order_message_num = 1;
	    recv_order_message_num = 1;
	}

    for(int i = 0; i<num_session_ids; i++)
	{
	    seqfile.getline(buf, sizeof(buf));

	    session_last_seq_struct* new_last_seq = NULL;
	    new_last_seq = new session_last_seq_struct;
	    memset(new_last_seq, 0, sizeof(session_last_seq_struct));

	    /*
	    appl_req_bool_struct* new_appl_req = NULL;
	    new_appl_req = new appl_req_bool_struct;
	    memset(new_appl_req, 0, sizeof(appl_req_bool_struct));
	    */

	    switch(i)
		{
		case 0:
		    new_last_seq->name = new char[sizeof(EtReadConfig::m_session_id)+1];
		    strcpy(new_last_seq->name, EtReadConfig::m_session_id.c_str());
		    
		    DBG_DEBUG("set account: %s", new_last_seq->name);
		    break;

            /*
		case 1:
        new_last_seq->name = new char[sizeof(EtReadConfig::m_session_2)+1];
		    //new_appl_req->name = new char[sizeof(EtReadConfig::m_session_1)+1];
		    strcpy(new_last_seq->name, EtReadConfig::m_session_2.c_str());
		    //strcpy(new_appl_req->name, EtReadConfig::m_session_1.c_str());
		    DBG_DEBUG("set account: %s", new_last_seq->name);
		    break;
            */
		}

	    if( last_weekday > 1 && (curr_weekday < 2 || (curr_weekday > 2 && diff > curr_weekday ) ))
		{
		    new_last_seq->last_seq = 1;
		}
	    else
		{
		    new_last_seq->last_seq = atoi(buf);
		}

	    if (last_seq_list == NULL)
		{
		    last_seq_list = new_last_seq;
		    end_last_seq_list = new_last_seq;
		}
	    else
		{
		    end_last_seq_list->next = new_last_seq;
		    end_last_seq_list = new_last_seq;
		}

	    /*
	    new_appl_req->is_appl_req = false;
	    if (appl_req_list == NULL)
		{
		    appl_req_list = new_appl_req;
		    end_appl_req_list = new_appl_req;
		}
	    else
		{
		    end_appl_req_list->next = new_appl_req;
		    end_appl_req_list = new_appl_req;
		}
	    */
	}

    print_sessions();
    seqfile.close();
    return;
}

void exec_globex::finalizeSequence()
{
    DBG_DEBUG("finalizeSequence.");
    // write file for last saved sequence number
    seqfile.open("/usr/home/ascendant/auto-trading-v1/v2-dropcopy/database/sequence.txt", fstream::out);
    assert(seqfile.good());

    char date_buffer[9];
    memset(date_buffer, 0, sizeof(date_buffer));
    EtUtil::build_date(date_buffer, sizeof(date_buffer));

    char weekday_buffer[2];
    memset(weekday_buffer, 0, sizeof(weekday_buffer));
    EtUtil::build_weekday(weekday_buffer, sizeof(weekday_buffer));
    
    seqfile << date_buffer << "\n";
    seqfile << weekday_buffer << "\n";
    seqfile << order_message_num << "\n";
    seqfile << recv_order_message_num << "\n";

    print_sessions();
    
    session_last_seq_struct* curr_last_seq = NULL;
    session_last_seq_struct* del_last_seq = NULL;

    for (curr_last_seq = last_seq_list; curr_last_seq != NULL; curr_last_seq = curr_last_seq->next)
	{
	    seqfile << curr_last_seq->last_seq << "\n";
	    del_last_seq = curr_last_seq;
            delete del_last_seq;
	}

    seqfile.close();

    /*
    appl_req_bool_struct* curr_req_bool = NULL;
    appl_req_bool_struct* del_req_bool = NULL;

    for (curr_req_bool = appl_req_list; curr_last_seq != NULL; curr_last_seq = curr_last_seq->next)
        {
            DBG_DEBUG("name: %s", curr_req_bool->name);
            DBG_DEBUG("last seq: %d", curr_req_bool->is_appl_req);
            del_req_bool = curr_req_bool;
            delete del_req_bool;
        }
    */

    return;
}

int exec_globex::globex_start()
{
    DBG_DEBUG("globex_start.");
	strcpy(login_name, ORDER_USER_NAME);
	strcpy(login_pass, ORDER_PASSWORD);

	strcpy(login_order_host, EtReadConfig::m_dropCopyServerIp.c_str());
	login_order_port = EtReadConfig::m_dropCopyServerPort;

	strcpy(globex_account, GLOBEX_ACCT);
	
	if (-1 == connet_to_order())
	{
		DBG_ERR("fail to connect or login.");
		return -1;
	}

	pthread_create(&(order_thread), 0, run_order, this);
	pthread_create(&(heart_thread), 0, set_heart_beat, this);
	
	return 0;
}


int exec_globex::connet_to_order()
{
	int iRes = 0;
	
	// connect
	iRes = EtUtil::connect_client(login_order_host, login_order_port);
	if (iRes == -1)
	{
		DBG_ERR("Fail to connect to host: %s port: %d", 
			login_order_host, login_order_port);
		return -1;
    }
	order_des = iRes;
	
	if ((EtUtil::set_non_block(order_des)) == -1)
	{
	   	DBG_ERR("order_des: problems settng nonblock des: %d errno: %d (%s).", 
			order_des, errno, strerror(errno));
		close(order_des);
		order_des = -1;
		return -1;
	}
	
	login_state = LOGIN_CONNECTED;	
	return 0;
}


void* exec_globex::run_order(void* arg)
{
	int iRes = 0;
	struct timeval select_wait;
	fd_set in_set;	
	exec_globex *my_globex = NULL;
	
	recv_size = 0;
	leave_size = 0;
	parse_size = 0;
	
	memset(recv_buffer, 0, ORDER_BUFFER_SIZE);
	memset(leave_buffer, 0, ORDER_BUFFER_SIZE);
	memset(parse_buffer, 0, ORDER_BUFFER_SIZE * 2);
	
	char* parsePtr = NULL;

	my_globex = (exec_globex*)arg;
	
	select_wait.tv_sec = 1;
	select_wait.tv_usec = 0;

	while (true)
	{
		if (et_g_exitProcess || my_globex->order_des == -1)
		{
			break;
		}
		
		FD_ZERO(&in_set);
		FD_SET(my_globex->order_des, &in_set);
		
		iRes = select(my_globex->order_des + 1, &in_set, NULL, 0, &select_wait);
		if (iRes == -1)
	    {
			if (errno != EINTR)		/* this is ok we get it from the alarm */
		    {
				DBG_ERR("select error errno: %d (%s).", errno, strerror(errno));
				break;
		    }
	    }
		else if (iRes > 0)
	    {
			if (FD_ISSET(my_globex->order_des, &in_set))
			{
				memset(recv_buffer, 0, ORDER_BUFFER_SIZE);
				recv_size = read(my_globex->order_des, recv_buffer, ORDER_BUFFER_SIZE);
				if (recv_size <= 0)
			    {
					DBG_ERR("do not read from des = %d errno = %d (%s).", my_globex->order_des, errno, 
						strerror(errno));
					close(my_globex->order_des);
					my_globex->order_des = -1;
					break;
			    }

				DBG_DEBUG("READ DROPCOPY: %s", recv_buffer);
				memset(parse_buffer, 0, ORDER_BUFFER_SIZE * 2);
				parsePtr = parse_buffer;
				if (leave_size == 0)
				{
					memcpy(parsePtr, recv_buffer, recv_size);
					parse_size = recv_size;
				}
				else
				{
					memcpy(parsePtr, leave_buffer, leave_size);
					parsePtr += leave_size;
					memcpy(parsePtr, recv_buffer, recv_size);
					parse_size = leave_size + recv_size;
					memset(leave_buffer, 0, ORDER_BUFFER_SIZE);
					leave_size = 0;
				}

				//my_globex->split_recv_buffer(parse_buffer, parse_size, leave_buffer, leave_size);
                my_globex->split_recv_buffer();
		    }
	    }
	    
		my_globex->send_heartbeat_flag = 0;
		if (my_globex->send_heartbeat_flag)
		{
			my_globex->send_heartbeat("OR HEART");
			my_globex->send_heartbeat_flag = 0;
	    }
	}

	if(my_globex->order_des != -1)
	    {
		close(my_globex->order_des);
		my_globex->order_des = -1;
	    }
	return 0;
}


//void exec_globex::split_recv_buffer(char* _parse_buf, int _parse_len, char* _leave_buf, int& _leave_len)
void exec_globex::split_recv_buffer()
{
	char endFlag[5];
	endFlag[0] = SOH;
	endFlag[1] = '1';
	endFlag[2] = '0';
	endFlag[3] = '=';
	endFlag[4] = '\0';

    char sohFlag[2]; 
    sohFlag[0] = SOH; 
    sohFlag[1] = '\0';

    int iFlag = 0;
	int length = 0;
    
    char *cBegin = parse_buffer;
    char *buff_end = cBegin + parse_size;
    char *cEnd = NULL;
    char *cTempBegin = parse_buffer;

    while (cBegin <= buff_end)
    {
        cEnd = strstr(cBegin, endFlag);
		if (cEnd == NULL)
	    {
	        // leave something
	        iFlag = 1;
	        break;
	    }

	    // [SOH]10=XXX[SOH]
        cEnd += 4;
        int tempLength = cEnd - cBegin;
        cTempBegin += tempLength;
        cEnd = strstr(cTempBegin, sohFlag);
        if(cEnd == NULL){
          iFlag = 1;
          break;
        }

	    cEnd += 1;
		length = cEnd-cBegin;
		DBG_DEBUG("parse buffer: %.*s", length, cBegin);
	    parse_fix(cBegin, &cEnd);
		cBegin += length;
        cTempBegin += 4;
	}
	
	if (1 == iFlag)
	{
	    leave_size = buff_end - cBegin;
	    memcpy(leave_buffer, cBegin, leave_size);
	}
	else
	{
		leave_size = 0;
	}
}


// heart beat thread
void* exec_globex::set_heart_beat(void* arg)
{
	exec_globex	*my_globex;
	
	my_globex = (exec_globex*)arg;
	my_globex->send_heartbeat_flag = 0;

	for ( ; ; )
    {
    	if (et_g_exitProcess)
		{
			break;
		}
		sleep(atoi(GLOBEX_HEART_BEAT_INTERVAL));
		my_globex->send_heartbeat_flag = 1;
    }
	return (0);
}

bool exec_globex::checkpoint()
{
   DBG_DEBUG("checkpoint.");
    string file_name = "/usr/home/ascendant/auto-trading-v1/v2-dropcopy/database/cp_" + EtUtil::intToStr(recv_order_message_num) + ".txt";
    checkpoint_file.open(file_name.c_str(), fstream::out);
    assert(checkpoint_file.good());
	DBG_DEBUG("open file.");

    // next expected sequence number, last sequence for each account
    checkpoint_file << recv_order_message_num << "\n";

    session_last_seq_struct* curr_last_seq = NULL;
    for (curr_last_seq = last_seq_list; curr_last_seq != NULL; curr_last_seq = curr_last_seq->next)
	{
	    checkpoint_file << curr_last_seq->last_seq << "\n";
	}

    checkpoint_file.close();

    return true;
}

bool exec_globex::send_checkpoint_request()
{
    EtClientMessage ecm;
    ecm.clearInMap();
    ecm.putField(fid_msg_type, msg_posi_checkpoint);
    ecm.putField(fid_posi_seq_num, recv_order_message_num);

    if ( !g_socketListPtr->sendDataToServers(ecm) )
	{
	    DBG_ERR("Fail to send package to client.");
	    return false;
	}

    return true;
}

bool exec_globex::compareSequence(int _sequence)
{
    DBG_DEBUG("recv_order_message_num = %d", recv_order_message_num);
    if(_sequence == recv_order_message_num)
	{
	    DBG_DEBUG("sequence equal to revc_order_message_num");
	    //do nothing
	}
    else if(_sequence > recv_order_message_num)
	{
	    recv_order_message_num = _sequence;
	}
    else if(_sequence < recv_order_message_num && _sequence != -1)
	{
	    DBG_DEBUG("sequence LESS THAN revc_order_message_num");
	    recv_order_message_num = _sequence;
	    
	    // restore each accounts' last sequence number
	    string file_name = "cp_" + EtUtil::intToStr(_sequence) + ".txt";
	    checkpoint_file.open(file_name.c_str(), fstream::in);
	    
	    if(!checkpoint_file)
		{
		    DBG_DEBUG("file %s not exists.", file_name.c_str());
		}
	    else
	    {
		assert(checkpoint_file.good());
		
		char buf[64];
		checkpoint_file.getline(buf, sizeof(buf));
		
		session_last_seq_struct* curr_last_seq = NULL;
		curr_last_seq = last_seq_list;
		if(!checkpoint_file.eof() && curr_last_seq != NULL)
		    {
			checkpoint_file.getline(buf, sizeof(buf));
			curr_last_seq->last_seq = atoi(buf);
			curr_last_seq = curr_last_seq->next;
		    }
		checkpoint_file.close();
	    }
    }
    
    if (-1 == send_login())
	{
	    DBG_DEBUG("fail to login.");
	    et_g_exitProcess = true;
	    return false;
	}

    /*
    if(-1 == send_last_appl_request())
	{
	    DBG_DEBUG("fail to send last appl request.");
	    et_g_exitProcess = true;
	    return false;
	}
    */

    return true;
}

bool exec_globex::start()
{
  if (-1 == send_login()) 
    { 
      DBG_DEBUG("fail to login."); 
      et_g_exitProcess = true; 
      return false; 
    } 

  if(-1 == send_last_appl_request()) 
    { 
      DBG_DEBUG("fail to send last appl request."); 
      et_g_exitProcess = true; 
      return false; 
    } 
 
  return true;
}
