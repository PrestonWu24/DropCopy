#include <cstring>
#include <queue>

#include "trace_log.h"
#include "util.h"
#include "exec_globex.h"
#include "package_store.h"
#include "drop_copy.h"
#include "read_config.h"
#include "read_position.h"

extern drop_copy* et_g_dropcopy;
extern EtReadPosition* g_readPositionPtr;
extern bool et_g_exitProcess;
extern bool dropcopy_exit;

int exec_globex::parse_fix(char *buffer, char **buffer_ptr)
{
	fix_message_struct* fix_msg = NULL;
	int ret_val = 0;
	
	for (;(ret_val = parse_fix_get_message(buffer, buffer_ptr, &fix_msg)) == 0;)
	    {
          check_sequence(fix_msg);
	    }
	return 0;
}

int exec_globex::check_sequence(fix_message_struct*& fix_msg)
{
  //DBG_DEBUG("check_sequence.");
    checkpoint_num++;

    fix_field_struct seq_field;
    fix_field_struct report_type_field;
    fix_field_struct appl_last_seq_field;

    int new_seq_num = 0;

    // tag: 34 - MsgSeqNum
    if (get_fix_field(fix_msg, "34", &seq_field) == -1)
	{
	    DBG_ERR("fail to get 34 field.");
	    return -1;
	}
    new_seq_num = atoi(seq_field.field_val);

    //DBG_DEBUG("CME SeqNum: %d", new_seq_num);
    DBG_DEBUG("CME SeqNum, also new seq: %d, dropcopy_appl_resend=%d, is_gap_fill=%d, recv_order_message_num=%d", 
              new_seq_num, dropcopy_appl_resend, is_gap_fill, recv_order_message_num);
    
    // if is in application resend request process
    if(dropcopy_appl_resend)
	{
		//DBG_DEBUG("dropcopy_appl_resend");
	    if (strcmp(fix_msg->msg_type, "BX") == 0)
		{
		    handle_globex_resend_ack(fix_msg);
		    // dropcopy_appl_resend may changed during above ack message
		    if(dropcopy_appl_resend)
			{
			    begin_recv_appl_resend = true;                
			}
		    else
			{
			    DBG_DEBUG("CME unable to process application requested messages.");
			    recv_order_message_num = appl_reserve_seq_num;
                before_apply_appl_reserve_seq = new_seq_num + 1;
			    applyApplReserve();
			    dropcopy_appl_resend = false;
			    need_appl_resend = false;
			    begin_recv_appl_resend = false;
                recv_order_message_num = before_apply_appl_reserve_seq;
                return 0;
			}
		}
	    else if (strcmp(fix_msg->msg_type, "BY") == 0)
		{
          DBG_DEBUG("BY message, appl_reserve_seq_num=%d", appl_reserve_seq_num);
          recv_order_message_num = new_seq_num + 1;

          if (get_fix_field(fix_msg, "1357", &appl_last_seq_field) == -1) 
            { 
              DBG_ERR("Fail to get field of 1357."); 
              //return -1; 
            }
          else{
              DBG_DEBUG("1357=%d", atoi(appl_last_seq_field.field_val));
              // only administrative messages
              int by_last_seq = atoi(appl_last_seq_field.field_val);

			  // finish applicaiton resend request
              if(by_last_seq == 0 || by_last_seq >= appl_resend_reserve_last_seq)
				{
				    // no non-administarive messages
				    dropcopy_appl_resend = false;
				    begin_recv_appl_resend = false;
				    //recv_order_message_num = appl_reserve_seq_num;
                    before_apply_appl_reserve_seq = new_seq_num + 1;
				    applyApplReserve();
				    appl_reserve_seq_num = -1;
				    is_gap_fill = false;
				    need_appl_resend = false;
                    recv_order_message_num = before_apply_appl_reserve_seq;                    
                    finish_tt_init_position = true;
				    return 0;
				}
          }
 
          // application resend complete
          if (get_fix_field(fix_msg, "1426", &report_type_field) == -1)  
            {  
              DBG_ERR("Fail to get field of 1426.");
              //return -1;  
            }
          else{
			    int report_type = atoi(report_type_field.field_val);
			    DBG_DEBUG("ApplReportType: %d", report_type);
			    if(report_type == 3)
				{
                  dropcopy_appl_resend = false;

                  // finish receiving current request messsages
                  string curr_session = EtReadConfig::m_session_id;
                  int curr_last_seq = atoi(appl_last_seq_field.field_val);
                  if(appl_resend_reserve_last_seq > curr_last_seq){
                    int diff = appl_resend_reserve_last_seq - atoi(appl_last_seq_field.field_val);
                    
                    // only one application resend request at one time 
                    if(!dropcopy_appl_resend){
                      appl_reserve_seq_num = new_seq_num + 1; 
                      //int last_seq = find_last_seq(curr_session.c_str());
					  int last_seq = appl_resend_request_up_num;

                      if(diff <= 2500){
                        send_appl_resend_request(curr_session, last_seq, appl_resend_reserve_last_seq);
						appl_resend_request_up_num = appl_resend_reserve_last_seq;
                      }
                      else{
                        send_appl_resend_request(curr_session, last_seq, 0);
						appl_resend_request_up_num = last_seq + 2500;
                      }
                      dropcopy_appl_resend = true;

					  DBG_DEBUG("appl_resend_reserve_last_seq=%d, appl_resend_request_up_num=%d", appl_resend_reserve_last_seq, appl_resend_request_up_num);
                    }

                    return 0;
                  }
                  else{
                    DBG_DEBUG("finish_tt_init_position=%d", finish_tt_init_position); 
                    if(!finish_tt_init_position){ 
                      send_appl_resend_request(curr_session, find_last_seq(curr_session.c_str()), 0); 
                      dropcopy_appl_resend = true;
                      return 0;
                    }
                    else{
                      // finish receiving all messages.
                      begin_recv_appl_resend = false;
                      recv_order_message_num = appl_reserve_seq_num;
                      before_apply_appl_reserve_seq = new_seq_num + 1;
                      applyApplReserve();
                      appl_reserve_seq_num = -1;
                      is_gap_fill = false;
                      need_appl_resend = false;
                      recv_order_message_num = before_apply_appl_reserve_seq;
                    }
                  }
				}
			}
		    return 0;
		}
	}
    
    if (recv_order_message_num == -1)
	{
	    recv_order_message_num = new_seq_num;
	    recv_order_message_num++;
	}
    else
	{
	    // if is in FIX resend request process
	    if(is_gap_fill)
		{
			//DBG_DEBUG("is_gap_fill");
		    // if FIX resend still miss message, need application resend request
		    if(need_appl_resend)
			{
			    if(dropcopy_appl_resend)
				{ 
				    if(!begin_recv_appl_resend)
					{
					    addToApplReserve(fix_msg);
					    return 0;
					}
				    else
					{
					    // parse this message
					}
				}
			    else 
				{
				    string curr_session = globex_get_session(fix_msg);
				    if (curr_session.empty())
					{
					    DBG_DEBUG("did not get session.");
					}
				    else
					{
					    int curr_session_last_seq = globex_get_last_seq(fix_msg);
					    if(curr_session_last_seq > find_last_seq(curr_session.c_str()))
						{
						    // only one application resend request at one time
						    appl_reserve_seq_num = new_seq_num;
                            DBG_DEBUG("appl_reserve_seq_num=%d", appl_reserve_seq_num);
                            if(!dropcopy_appl_resend){
                              send_appl_resend_request(curr_session, find_last_seq(curr_session.c_str()), curr_session_last_seq);
                            }

						    addToApplReserve(fix_msg);
						    dropcopy_appl_resend = true;
						    return 0;
						}
					}
				}
			}
		    else if(new_seq_num == recv_order_message_num)
			{
			    recv_order_message_num++;
			    if(recv_order_message_num == fix_reserve_seq_num)
				{
				    DBG_DEBUG("finish get the missed messages, begin apply reserve queue messages.");
				    applyFixReserve();
				    is_gap_fill = false;
				    fix_reserve_seq_num = -1;
				}
			}
		    else if(new_seq_num > recv_order_message_num)
			{
			    addToFixReserve(fix_msg);
			    return 0;
			}
		}
	    else
		{
			//DBG_DEBUG("normal situation");
		    // normal situation
		    if (new_seq_num == recv_order_message_num)
			{
				DBG_DEBUG("new_seq_num == recv_order_message_num");
			    // next expected message sequence number
			    recv_order_message_num++;

                fix_field_struct appl_start_seq;
                fix_field_struct appl_last_seq;
                if (strcmp(fix_msg->msg_type, "BX") == 0){ 
					DBG_DEBUG("type is BX.");
                  if (get_fix_field(fix_msg, "1182", &appl_start_seq) == -1)  
                    {  
                      //DBG_ERR("Fail to get field of 1182.");  
                    }
                  else if (get_fix_field(fix_msg, "1183", &appl_last_seq) == -1)   
                    {   
                      //DBG_ERR("Fail to get field of 1183.");   
                    }
                  else{
					  DBG_DEBUG("CME response, resend from %d to %d", atoi(appl_start_seq.field_val), atoi(appl_last_seq.field_val));
                  }
                }
                else if (strcmp(fix_msg->msg_type, "8") == 0){
                  string curr_session = globex_get_session(fix_msg); 
                  if (curr_session.empty()) { 
                    DBG_DEBUG("did not get session."); 
                  }
                  else {
					  //DBG_DEBUG("current session: %s", curr_session.c_str());

                    // 1350
                    int curr_session_last_seq = globex_get_last_seq(fix_msg); 

                    int last_seq = find_last_seq(curr_session.c_str());
                    
                    if(curr_session_last_seq > last_seq){
                      int diff = curr_session_last_seq - last_seq;

                      // only one application resend request at one time 
                      if(!dropcopy_appl_resend){
                        appl_reserve_seq_num = new_seq_num; 
                        
						int curr_stored_last_seq = find_last_seq(curr_session.c_str());
                        if(diff <= 2500){
                          send_appl_resend_request(curr_session, curr_stored_last_seq, curr_session_last_seq);
						  appl_resend_request_up_num = curr_session_last_seq;
                        }
                        else{
                          send_appl_resend_request(curr_session, curr_stored_last_seq, 0);
						  appl_resend_request_up_num = curr_stored_last_seq + 2500;
                        }

						appl_resend_reserve_last_seq = curr_session_last_seq;

						DBG_DEBUG("appl_resend_reserve_last_seq=%d, appl_resend_request_up_num=%d", appl_resend_reserve_last_seq, appl_resend_request_up_num);

                        dropcopy_appl_resend = true;
                      }

                      addToApplReserve(fix_msg); 
                      return 0;
                    }
                  }
                }
            }
		    // miss messages
		    else if(new_seq_num > recv_order_message_num)
			{
			    // FIX resend request, range of messages to resend is 2500
			    DBG_DEBUG("messages missed, FIX resend request, new_seq_num =%d, expected recv_num=%d", new_seq_num, recv_order_message_num);
			    if ((new_seq_num - recv_order_message_num + 1) > 2500)
				{
				    recv_order_message_num = new_seq_num - 2500 + 1;
				}

			    send_resend_request(recv_order_message_num, 0);
			    addToFixReserve(fix_msg);
			    fix_reserve_seq_num = new_seq_num;
			    is_gap_fill = true;
			    return 0;
			}
		}
	}
    
    //DBG_DEBUG("msg_type: %s", fix_msg->msg_type);
    parse_field(fix_msg, new_seq_num);
    return 0;
}

int exec_globex::parse_field(fix_message_struct*& fix_msg, const int& new_seq_num)
{
	if (strcmp(fix_msg->msg_type, "5") == 0)
    {
		handle_globex_logout(fix_msg);
    }
	else if (strcmp(fix_msg->msg_type, "A") == 0)
    {
    	recv_order_message_num = new_seq_num + 1;
		handle_globex_login(fix_msg);
    }
	else if (strcmp(fix_msg->msg_type, "1") == 0)
    {
		handle_globex_heartbeat_request(fix_msg);
    }
	else if (strcmp(fix_msg->msg_type, "2") == 0)
    {
		handle_globex_resend_request(fix_msg);
    }
	else if (strcmp(fix_msg->msg_type, "3") == 0)
    {
		handle_globex_session_reject(fix_msg);
    }
	else if (strcmp(fix_msg->msg_type, "4") == 0)
    {
		handle_globex_sequence_request(fix_msg);
    }
	else if (strcmp(fix_msg->msg_type, "8") == 0)
    {
		handle_globex_order_message(fix_msg);
    }
	else if (atoi(fix_msg->msg_type) == 9)
    {
    	/* seems like they send "09" sometimes */
		handle_globex_cancel_reject_message(fix_msg);
    }
	else if (strcmp(fix_msg->msg_type, "j") == 0)
    {
    	/* business reject */
		handle_globex_business_reject_message(fix_msg);
    }
    else if (strcmp(fix_msg->msg_type, "0") == 0)
    {
    	DBG_DEBUG("receive a heartbeat message.");
    }
    else if (strcmp(fix_msg->msg_type, "b") == 0)
    {
    	handle_globex_quote_acknowledge(fix_msg);
    }
    else if (strcmp(fix_msg->msg_type, "BX") == 0)
    {
    	handle_globex_request_acknowledge(fix_msg);
    }
	else
    {
		DBG_ERR("unhandled msg_type = %s", fix_msg->msg_type);
    }
    
	free_fix_struct(fix_msg);
	return 0;
}

int exec_globex::parse_fix_get_message (char *buffer, char **buffer_ptr, fix_message_struct **fix_msg) 
    {
	int				field_size;
	fix_field_struct	*fix_field;
	fix_field_struct	*last_field;
	int				ret_get_field;
	char		*curr_ptr;
	int				to_erase;
	int				to_move;



	for (*fix_msg = 0, curr_ptr = buffer;; curr_ptr += field_size)
	    {
		if ((ret_get_field = parse_fix_get_field (curr_ptr, buffer_ptr, &field_size, &fix_field)) == -1)
		    {
              //DBG_ERR("fail to get field.");
			if (*fix_msg != 0)
			    {
                  free_fix_struct (*fix_msg);
			    }
			return (-1);
		    }

		if (fix_field == 0)
		    {
			continue;	
		    }

		if (strcmp ((char*) fix_field->field_id, "8") == 0)
		    {
			if (*fix_msg == 0)
			    {
				//*fix_msg = (fix_message_struct*) malloc (sizeof (fix_message_struct));
				*fix_msg = new fix_message_struct;
				memset (*fix_msg, 0, sizeof (fix_message_struct));
				last_field = (*fix_msg)->fields = fix_field;
			    }
		    }
		else if (strcmp ((char*) fix_field->field_id, "35") == 0)
		    {
			if (*fix_msg != 0)
			    {
				(*fix_msg)->msg_type = (char*) strdup ((char*) fix_field->field_val);
			    }
			last_field->next = fix_field;
			last_field = fix_field;
		    }
		else if (strcmp ((char*) fix_field->field_id, "10") == 0)
		    {
			curr_ptr += field_size;
			to_erase =  curr_ptr - buffer;
			to_move =  *buffer_ptr - curr_ptr;

			memmove (buffer, buffer + to_erase, to_move);
			(*buffer_ptr) -= to_erase;

			last_field->next = fix_field;
			last_field = fix_field;
			return (0);
		    }
		else if (*fix_msg != 0)
		    {
			//for (curr_field = (*fix_msg)->fields; curr_field->next != 0; curr_field = curr_field->next);
			//curr_field->next = fix_field;
			last_field->next = fix_field;
			last_field = fix_field;
		    }
	    }

	if (*fix_msg != 0)
	    {
		free_fix_struct (*fix_msg);
	    }
	return (-1);
}

void exec_globex::print_fix_struct(fix_message_struct *fix_msg) 
{
	fix_field_struct* curr_field = NULL;

	DBG_DEBUG("msg_type = %s", fix_msg->msg_type);

	for (curr_field = fix_msg->fields; curr_field != NULL; curr_field = curr_field->next)
    {
		DBG_DEBUG("field: id = %s, val = %s", curr_field->field_id, curr_field->field_val);
    }
}


int exec_globex::free_fix_struct(fix_message_struct *fix_msg) 
{
	fix_field_struct* curr_field = NULL;
	fix_field_struct* to_del = NULL;

	curr_field = fix_msg->fields;
	while (curr_field != NULL)
    {
		to_del = curr_field;
		curr_field = curr_field->next;

		if (to_del->field_id != 0)
	    {
			free(to_del->field_id);
	    }
		if (to_del->field_val != 0)
	    {
			free(to_del->field_val);
	    }
		free (to_del);
    }

	if (fix_msg->msg_type != 0)
    {
		free(fix_msg->msg_type);
    }
	free(fix_msg);
	return 0;
}


/**
 * return: int
 *    0: ok
 *   -1: error
 */


int exec_globex::parse_fix_get_field (char *buffer, char **buffer_ptr, int *field_size, fix_field_struct **fix_field) 
{
	char		*field_end;
	char		*field_start;
	int				to_erase;
	int				to_move;

	/* might just be waiting for the SOH leave buffer alone */
	if ((field_end = (char*) memchr (buffer, SOH, *buffer_ptr - buffer)) == 0)
	{
      //DBG_ERR("do not find the SOH");
		*fix_field = 0;
		return (-1);
    }

	/* no equall means bad field move along in buffer */
	if ((field_start = (char*) memchr (buffer, '=', field_end - buffer)) == 0)
	{
		field_end++;
		to_erase =  field_end - buffer;
		to_move =  *buffer_ptr - field_end;

		memmove (buffer, buffer + to_erase, to_move);
		(*buffer_ptr) -= to_erase;
		*fix_field = 0;
		*field_size = 0;
		return (0);
	}


	*fix_field = new fix_field_struct;

	memset (*fix_field, 0, sizeof (fix_field_struct));
	(*fix_field)->field_id = new char [((field_start - buffer) + 1)];
	(*fix_field)->field_val = new char [((field_end - (field_start)))];
	
	memset ((*fix_field)->field_id, 0, ((field_start - buffer) + 1));
	memset ((*fix_field)->field_val, 0, ((field_end - (field_start))));

	memcpy ((*fix_field)->field_id, buffer, ((field_start - buffer)));
	memcpy ((*fix_field)->field_val, field_start + 1, ((field_end - (field_start)) - 1));

	*field_size = (field_end + 1) - buffer;

	return (0);
}

/**
 * return:
 *    0: ok
 *   -1: error
 */
int exec_globex::get_fix_field(fix_message_struct *fix_msg, const char *look_for, 
	fix_field_struct *ret_field) 
{
	fix_field_struct* curr_field = NULL;

	for (curr_field = fix_msg->fields; curr_field != NULL; curr_field = curr_field->next)
    {
		if (curr_field->field_id != NULL)
	    {
			if (strcmp(curr_field->field_id, look_for) == 0)
		    {
		    	memset(ret_field, 0, sizeof(fix_field_struct));
				memcpy(ret_field, curr_field, sizeof(fix_field_struct));
				return (0);
		    }
	    }
    }
    DBG_ERR("not find this field id, %s", look_for);
	return (-1);
}

int exec_globex::get_next_fix_field(fix_message_struct *fix_msg, const char *look_for, 
	fix_field_struct *ret_field, int next_num) 
{
	fix_field_struct* curr_field = NULL;
	int curr_count = 0;

	for (curr_field = fix_msg->fields; curr_field != NULL; curr_field = curr_field->next)
    {
		if (curr_field->field_id != NULL)
	    {
			if (strcmp(curr_field->field_id, look_for) == 0)
		    {
				if(curr_count == next_num)
				{
					memset(ret_field, 0, sizeof(fix_field_struct));
					memcpy(ret_field, curr_field, sizeof(fix_field_struct));
					return (0);
				}
				curr_count++;
		    }
	    }
    }
    DBG_ERR("not find this field id, %s", look_for);
	return (-1);
}

string exec_globex::globex_get_session(fix_message_struct*& fix_msg)
{
    fix_field_struct session_field;

    // tag 1180
    if (get_fix_field(fix_msg, "1180", &session_field) == -1)
        {
			DBG_ERR("Fail to get Session field");
			return "";
        }
    //DBG_DEBUG("Session: %s", session_field.field_val);

    return (session_field.field_val);
}

int exec_globex::globex_gap_fill_curr_seq(fix_message_struct*& fix_msg)
{
	fix_field_struct appl_seq_num_field;

    // tag: 1181 - ApplSeqNum
	if (get_fix_field(fix_msg, "1181", &appl_seq_num_field) == -1)
	{
		DBG_ERR("Fail to get ApplSeqNum field");
		return false;
	}
	DBG_DEBUG("ApplSeqNum: %s", appl_seq_num_field.field_val);
	
	return (atoi(appl_seq_num_field.field_val));
}

int exec_globex::globex_get_last_seq(fix_message_struct*& fix_msg)
{
	fix_field_struct appl_last_seq_num_field;

	// tag: 1350 - ApplLastSeqNum
	if (get_fix_field(fix_msg, "1350", &appl_last_seq_num_field) == -1)
	{
		DBG_ERR("Fail to get ApplLastSeqNum field");
		return false;
	}
	DBG_DEBUG("ApplLastSeqNum: %s", appl_last_seq_num_field.field_val);
	
	return (atoi(appl_last_seq_num_field.field_val));
}

/*
int exec_globex::globex_check_session(fix_message_struct*& fix_msg)
{
	fix_field_struct session_field;
	fix_field_struct appl_seq_num_field;
	fix_field_struct appl_last_seq_num_field;

	if (strcmp(fix_msg->msg_type, "8") == 0)
	{
	    // tag: 1180 - source session
	    if (get_fix_field(fix_msg, "1180", &session_field) == -1)
		{
		    DBG_ERR("Fail to get source session field");
		    return false;
		}
	    DBG_DEBUG("source session: %s", session_field.field_val);

	    // tag: 1181 - ApplSeqNum
	    if (get_fix_field(fix_msg, "1181", &appl_seq_num_field) == -1)
		{
		    DBG_ERR("Fail to get ApplSeqNum field");
		    return false;
		}
	    DBG_DEBUG("ApplSeqNum: %s", appl_seq_num_field.field_val);
	    
	    // tag: 1350 - ApplLastSeqNum
	    if (get_fix_field(fix_msg, "1350", &appl_last_seq_num_field) == -1)
		{
		    DBG_ERR("Fail to get ApplLastSeqNum field");
		    return false;
		}
	    DBG_DEBUG("ApplLastSeqNum: %s", appl_last_seq_num_field.field_val);

	    // update
	    int last_seq = find_last_seq(session_field.field_val);
	    DBG_DEBUG("last_seq=%d", last_seq);

	    if(last_seq == -1)
		{
		    DBG_DEBUG("no session associate.");
		}
	    else
		{
		    modify_last_seq(session_field.field_val, atoi(appl_seq_num_field.field_val));
		}
	    print_sessions();
	}
	
	return 0;
}
*/

/*
 * eg:
 * 	  8=FIX.4.29=10235=A34=74369=652=20060823-19:32:11.64949=CME50=G56=H37512N57=pioneer (mco capital)98=0108=3010=033
 */
int exec_globex::handle_globex_login(fix_message_struct *fix_msg)
{
	fix_field_struct found_field;

	DBG_DEBUG("handle globex login.");

	if (get_fix_field(fix_msg, "108", &found_field) == 0)
	    {
		if (strcmp(found_field.field_val, GLOBEX_HEART_BEAT_INTERVAL) == 0)
		    {
              //DBG_DEBUG("login name: %s", login_name);
              login_state = LOGGED_IN_AND_SUBSCRIBED;
		    }
		return 0;
	    }
   
    //DBG_ERR("fail to get 108 field.");
	return -1;
}


/*
 * eg:
 *    order_buffer=8=FIX.4.29=18535=534=11369=352=20060823-15:09:42.73749=CME50=G56=H37512N57=pioneer (mco capital)58=Sequence number received lower than expected. Expected [4] Received 2. Logout forced.789=410=063
 */
int exec_globex::handle_globex_logout(fix_message_struct *fix_msg)
{
	fix_field_struct text_field;

	//  tag: 58 - reason or confirmation text 
        if (get_fix_field(fix_msg, "58", &text_field) == 0)
	    {
          //  DBG_ERR("Fail to get field of reason or confirmation.");
	    }
        //cout << "logout forced: " << text_field.field_val << endl;
	DBG_DEBUG("logout forced: %s", text_field.field_val);
	checkpoint();
	//send_checkpoint_request();
	g_readPositionPtr->stopReadData();
	dropcopy_exit = true;
	return (-1);
}


/**
 * eg:
 *    order_buffer=8=FIX.4.29=10235=234=868369=1052=20060823-20:10:01.22549=CME50=G56=H37512N57=pioneer (mco capital)7=1116=010=244
 */
int exec_globex::handle_globex_heartbeat_request(fix_message_struct *fix_msg)
{
	fix_field_struct found_field;
	
	DBG_DEBUG("handle globex test request.");

    // tag: 112 - TestReqID
	if (get_fix_field(fix_msg, "112", &found_field) == 0)
    {
		send_heartbeat(found_field.field_val);
		return (0);
    }
	else
    {
		send_heartbeat(0);
    }

	DBG_ERR("fail to get TestReqID.");
	return (-1);
}


int exec_globex::handle_globex_resend_request(fix_message_struct *fix_msg)
{
	fix_field_struct start_seq_field;
	fix_field_struct end_seq_field;
	
	int seq_num = 0;
	package_struct* pakPtr = NULL;

    // tag: 7 - BeginSeqNo
	if (get_fix_field(fix_msg, "7", &start_seq_field) == -1)
    {
    	DBG_ERR("Fail to get field of BeginSeqNo.");
		return (-1);
    }
    //DBG_DEBUG("BeginSeqNo: %s", start_seq_field.field_val);
    seq_num = atoi(start_seq_field.field_val);

	if(last_request_seq_num == seq_num)
	{
    	return 0;
	}
	else
	{
		last_request_seq_num = seq_num;
	}
	// tag: 16 - EndSeqNo
	if (get_fix_field(fix_msg, "16", &end_seq_field) == -1)
    {
    	DBG_ERR("Fail to get field of EndSeqNo.");
		return (-1);
    }
    
	while(1)
	{
		pakPtr = PackageStore::get_package(seq_num++);		
		if (NULL == pakPtr)
		{
			DBG_ERR("Fail to get a package.");
			DBG_DEBUG("seq_num: %d, order_message_num: %d", seq_num - 1, order_message_num);
			
			if (seq_num-1 == order_message_num)
			{
				break;
			}
			else if (seq_num-1 < order_message_num)
			{
				return send_sequence_reset_due_to_gap(atoi(start_seq_field.field_val));
			}
			else
			{
				DBG_ERR("seq_num (%d) is larger than order_message_num (%d)", 
					seq_num-1, order_message_num);
				break;
			}
		}

		// add more info before send on resend messages
		char write_buffer[1024];
		char* write_ptr = NULL;
		char* curr_str = NULL;
		char* dup_str = NULL;
		int bytes_to_write = 0;
		char body_buffer[1024];
		char time_buffer[32];
		char* body_ptr = body_buffer;
		int size = 0;

		write_ptr = write_buffer;
		
		memset(body_buffer, 0, 1024);
		memcpy(body_ptr, pakPtr->buffer, pakPtr->buffer_len);
		body_ptr += pakPtr->buffer_len;

		// add tag 369 for last processed sequence number
		size = build_field_int(body_ptr, 369, recv_order_message_num-1);
		body_ptr += size;

		char *len_ptr = NULL;
		char *start_count_ptr = NULL;
		char *end_count_ptr = NULL;
		int t_write_byte_count = 0;

		// add tag 122, can be the same value as tag 52
		char timeFlag[5];
		timeFlag[0] = SOH;
		timeFlag[1] = '5';
		timeFlag[2] = '2';
		timeFlag[3] = '=';	
		timeFlag[4] = '\0';		
		curr_str = 	strstr(body_buffer, timeFlag);
		curr_str += (DELIM_STRING_LEN + strlen("52="));

		memset(time_buffer, 0, 32);
		memcpy(time_buffer, curr_str, 21);
		time_buffer[21] = '\0';
		size = build_field_string(body_ptr, 122, time_buffer);
		body_ptr += size;

		// modify tag 9 body length
		char startFlag[4];
		startFlag[0] = SOH;
		startFlag[1] = '9';
		startFlag[2] = '=';
		startFlag[3] = '\0';
        len_ptr = start_count_ptr = (strstr(body_buffer, startFlag)  + DELIM_STRING_LEN + strlen("9="));

		memset(len_ptr, 'x', 6);
		start_count_ptr += (6 + DELIM_STRING_LEN);

		end_count_ptr = body_ptr;
		t_write_byte_count = (end_count_ptr - start_count_ptr);
		body_len(len_ptr, t_write_byte_count);

		// modify tag 43, make it to "Y"
		char dupFlag[4];
		dupFlag[0] = SOH;
		dupFlag[1] = '4';
		dupFlag[2] = '3';
		dupFlag[3] = '=';	
		dupFlag[4] = '\0';
		dup_str = 	strstr(body_buffer, dupFlag);
		dup_str += (DELIM_STRING_LEN + strlen("43="));
		*dup_str = 'Y';

		// copy body
		memset(write_buffer, 0, 1024);
		memcpy(write_ptr, body_buffer, body_ptr - body_buffer);
		write_ptr += (body_ptr - body_buffer);

		// trailer
		make_fix_trailer(write_buffer, &write_ptr);
		
		bytes_to_write = write_ptr - write_buffer;
		DBG_DEBUG("bytes to write: %d", bytes_to_write);
		DBG_DEBUG("resend: %s", write_buffer);

		send_to_order_des(write_buffer, bytes_to_write);
	}

	return 0;
}

int exec_globex::handle_globex_resend_ack(fix_message_struct *fix_msg)
{
	fix_field_struct resp_type;

	// tag: 1348 - ApplResponseType
	if (get_fix_field(fix_msg, "1348", &resp_type) == -1)
	    {
		DBG_ERR("Fail to get field of ApplResponseType.");
		return -1;
	    }
	DBG_DEBUG("resend response type: %s", resp_type.field_val);

	int resp = atoi(resp_type.field_val);

	if(resp == 2)
	    {
          DBG_DEBUG("Messages not available.");
          //applyReserve();
          recv_order_message_num++;
          //dropcopy_appl_resend = false;
	    }
	else if(resp == 1)
	    {
          DBG_DEBUG("Unable to process all requested messages.");
          // check following fields to see which ApplId does not support Application Message recovery
          //applyReserve();
          recv_order_message_num++;
          //dropcopy_appl_resend = false;
	    }
	else if(resp == 0)
	    {
		//successful
	    }

	return 0;
}

int exec_globex::handle_globex_session_reject(fix_message_struct *fix_msg)
{
	fix_field_struct text_field;

    // tag: 58 - Text
	if (get_fix_field(fix_msg, "58", &text_field) == -1)
    {
		DBG_ERR("Fail to get field of text.");
    }
	else
    {
		DBG_DEBUG("Success to get field of text: %s", text_field.field_val);
    }
	return 0;
	//return send_logout();
}


int exec_globex::handle_globex_quote_acknowledge(fix_message_struct *fix_msg)
{
	fix_field_struct quote_req_id;
	fix_field_struct quote_ack_status;
	fix_field_struct ex_quote_req_id;
	fix_field_struct quote_id;
	
    // tag: 131 - QuoteReqID
	if (get_fix_field(fix_msg, "131", &quote_req_id) == -1)
    {
		DBG_ERR("Fail to get field of QuoteReqID.");
		return -1;
    }
	DBG_DEBUG("QuoteReqID: %s", quote_req_id.field_val);
		
    
    // tag: 297 - QuoteAckStatus
	if (get_fix_field(fix_msg, "297", &quote_ack_status) == -1)
    {
		DBG_ERR("Fail to get field of QuoteAckStatus.");
		return -1;
    }
	DBG_DEBUG("QuoteAckStatus: %s", quote_req_id.field_val);
    
    // tag: 9770 - ExchangeQuoteReqID
	if (get_fix_field(fix_msg, "9770", &ex_quote_req_id) == -1)
    {
		DBG_ERR("Fail to get field of ExchangeQuoteReqID.");
		return -1;
    }
	DBG_DEBUG("ExchangeQuoteReqID: %s", ex_quote_req_id.field_val);
	
	// tag: 117 - QuoteID
	if (get_fix_field(fix_msg, "117", &quote_id) == -1)
    {
		DBG_ERR("Fail to get field of QuoteID.");
		return -1;
    }
	DBG_DEBUG("QuoteID: %s", quote_id.field_val);
	
	return 0;
}

int exec_globex::handle_globex_request_acknowledge(fix_message_struct *fix_msg)
{
	fix_field_struct num_appl_ids;	
	fix_field_struct appl_req_type;
	fix_field_struct appl_resp_type;
	fix_field_struct appl_id;
	fix_field_struct appl_last_seq;

    // tag: 1348 - ApplRespType
	if (get_fix_field(fix_msg, "1348", &appl_resp_type) == -1)
    {
		DBG_ERR("Fail to get field of ApplRespType.");
		return -1;
    }
	//DBG_DEBUG("ApplRespType: %s", appl_resp_type.field_val);

    // tag: 1351 - NoApplIds
	if (get_fix_field(fix_msg, "1351", &num_appl_ids) == -1)
    {
		DBG_ERR("Fail to get field of NoApplIds.");
		return -1;
    }
	DBG_DEBUG("NoApplIds: %s", num_appl_ids.field_val);
	num_session_ids = atoi(num_appl_ids.field_val);

    // tag: 1347 - ApplReqType
	if (get_fix_field(fix_msg, "1347", &appl_req_type) == -1)
    {
		DBG_ERR("Fail to get field of ApplRespType.");
		return -1;
    }
    //	DBG_DEBUG("ApplRespType: %s", appl_req_type.field_val);
	int req_type = atoi(appl_req_type.field_val);

	switch (req_type)
	{
		case 0:
			DBG_DEBUG("retransmission of application messages");
			break;	
		case 2:
			DBG_DEBUG("request for the last ApplLastSeqNum published");
			for(int i=0; i<num_session_ids; i++)
			{
				// tag: 1355 - RefAppId
				if (get_next_fix_field(fix_msg, "1355", &appl_id, i) == -1)
				{
					DBG_ERR("Fail to get field of RefAppId.");
					return -1;
				}
				DBG_DEBUG("RefAppId: %s", appl_id.field_val);

				// tag: 1357 - RefApplLastSeqNum
				if (get_next_fix_field(fix_msg, "1357", &appl_last_seq, i) == -1)
				{
					DBG_ERR("Fail to get field of RefApplLastSeqNum.");
					return -1;
				}
				DBG_DEBUG("RefApplLastSeqNum: %s", appl_last_seq.field_val);

				// session last sequence number
				session_last_seq_struct* new_last_seq = NULL;
				new_last_seq = new session_last_seq_struct;
				memset(new_last_seq, 0, sizeof(session_last_seq_struct));

				new_last_seq->name = new char[sizeof(appl_id.field_val)+1];
				strcpy(new_last_seq->name, appl_id.field_val);
				new_last_seq->last_seq = atoi(appl_last_seq.field_val);

				if (last_seq_list == NULL)
				{
					last_seq_list = new_last_seq;
					end_last_seq_list = new_last_seq;
				}
				else
				{
				    if(-1 == find_last_seq(new_last_seq->name))
					{
					    end_last_seq_list->next = new_last_seq;
					    end_last_seq_list = new_last_seq;
					}
				    else
					{
                      //modify_last_seq(new_last_seq->name, new_last_seq->last_seq);
                      // check last seq
                      int last_seq = find_last_seq(new_last_seq->name);
                      if(new_last_seq->last_seq > last_seq){
                        int diff = new_last_seq->last_seq - last_seq;
 
                        // only one application resend request at one time  
                        if(!dropcopy_appl_resend){ 
                          appl_reserve_seq_num = recv_order_message_num - 1;
                         
                          if(diff <= 2500){ 
                            send_appl_resend_request(new_last_seq->name, find_last_seq(new_last_seq->name), new_last_seq->last_seq);
                          } 
                          else{ 
                            send_appl_resend_request(new_last_seq->name, find_last_seq(new_last_seq->name), 0); 
                          } 
                          appl_resend_reserve_last_seq = new_last_seq->last_seq;
                          dropcopy_appl_resend = true; 
                        } 
 
                        addToApplReserve(fix_msg);  
                        return 0; 
                      }
					}
				}

				print_sessions();
			}
			
			break;
		case 3:
			DBG_DEBUG("request valid set of ApplFeedIDs");
			break;
		default:
			DBG_DEBUG("unknown ApplReqType.");
			break;
	}
	
	return 0;
}

int exec_globex::handle_globex_business_reject_message(fix_message_struct *fix_msg)
{
	fix_field_struct text_field;

    // tag: 58 - Text
	if (get_fix_field(fix_msg, "58", &text_field) == -1)
    {
		DBG_ERR("Fail to get field of text.");
    }
	else
    {
		DBG_DEBUG("Business reject: %s", text_field.field_val);
    }
	return 0;
}


int exec_globex::handle_globex_sequence_request(fix_message_struct *fix_msg)
{
	fix_field_struct new_seq_field;
	fix_field_struct gap_fill_field;

	if (get_fix_field(fix_msg, "36", &new_seq_field) == -1)
	    {
		DBG_ERR("fail to get 36 field.");
		return (-1);
	    }
	DBG_DEBUG("new_seq_field: %s", new_seq_field.field_val);
	
	if (get_fix_field(fix_msg, "123", &gap_fill_field) == -1)
	    {
		DBG_ERR("fail to get 123 field.");
		need_appl_resend = false;
	    }
	else
	    {
		if (strcmp(gap_fill_field.field_val, "Y") == 0)
		    {
			// must wait next message to determine the sequence number then send appl request
			need_appl_resend = true;
		    }
		else
		    {
			need_appl_resend = false;
		    }
	    }
	DBG_DEBUG("dropcopy_appl_resend: %d", dropcopy_appl_resend);
	
	return 0;
}


int exec_globex::handle_globex_order_message(fix_message_struct *fix_msg)
{
	fix_field_struct order_status_field;
	fix_field_struct symbol_field;
	fix_field_struct inst_display_field;
	fix_field_struct text_field;
	fix_field_struct price_field;
	fix_field_struct size_field;
	fix_field_struct order_side_field;
	fix_field_struct quant_left_field;
	fix_field_struct order_quant_field;
	fix_field_struct session_field;
	fix_field_struct appl_seq_num_field;
	fix_field_struct appl_last_seq_num_field;
    fix_field_struct account_field;
	fix_field_struct trade_date_field;
    
	DBG_DEBUG("handle globex order message.");

	bool is_yesterday = false;

    if (get_fix_field(fix_msg, "1", &account_field) == -1)
      {
        DBG_ERR("Fail to get account field");
        return false;
      }
    DBG_DEBUG("source account: %s", account_field.field_val);

	// tag: 39 - OrdStatus
	if (get_fix_field(fix_msg, "39", &order_status_field) == -1)
        {
	    DBG_ERR("can't find order status.");
	    return (-1);
	}
	//DBG_DEBUG("OrdStatus: %s", order_status_field.field_val);

	// tag: 48 - symbol
	if (get_fix_field(fix_msg, "48", &symbol_field) == -1)
        {
	    DBG_ERR("can't find symbol.");
	    return false;
	}
	//DBG_DEBUG("symbol: %s", symbol_field.field_val);

	// tag: 54 - side
        if (get_fix_field(fix_msg, "54", &order_side_field) == -1)
        {
	    DBG_ERR("can't find order_side.");
	    return false;
	}
        //DBG_DEBUG("order_side: %s", order_side_field.field_val);
	
	// tag: 107 - SecurityDesc
	if (get_fix_field(fix_msg, "107", &inst_display_field) == -1)
        {
	    DBG_ERR("can't find SecurityDesc field.");
	    return (-1);
	}
	//DBG_DEBUG("DisplayName: %s", inst_display_field.field_val);

    /*
	// tag: 432 - Expire date
	if (get_fix_field(fix_msg, "432", &expire_field) == -1)
        {
	    DBG_ERR("can't find Expire date.");
	    //return false;
	}
	//DBG_DEBUG("Expire date: %s", expire_field.field_val);
    */

	// tag: 1180 - source session
	if (get_fix_field(fix_msg, "1180", &session_field) == -1)
	{
	    DBG_ERR("Fail to get source session field");
	    return false;
	}
	//DBG_DEBUG("source session: %s", session_field.field_val);

	// tag: 1181 - ApplSeqNum
	if (get_fix_field(fix_msg, "1181", &appl_seq_num_field) == -1)
	{
	    DBG_ERR("Fail to get ApplSeqNum field");
	    return false;
	}
	//DBG_DEBUG("ApplSeqNum: %s", appl_seq_num_field.field_val);
	int curr_appl_seq_num = atoi(appl_seq_num_field.field_val);

	// tag: 1350 - ApplLastSeqNum
    int last_seq_num = 0;
	if (get_fix_field(fix_msg, "1350", &appl_last_seq_num_field) == -1)
	{
	    DBG_ERR("Fail to get ApplLastSeqNum field");
        int last_seq = find_last_seq(session_field.field_val); 
        DBG_DEBUG("session : %s, last seq=%d", session_field.field_val, last_seq); 
        if(last_seq == -1) 
          { 
            DBG_DEBUG("no session associated!"); 
            return -1; 
          } 
        else 
          { 
            modify_last_seq(session_field.field_val, atoi(appl_seq_num_field.field_val)); 
          }
	    
        if (get_fix_field(fix_msg, "1352", &appl_last_seq_num_field) == -1) 
          { 
            DBG_ERR("Fail to get ApplResendFlag field");
          }
        else{
          last_seq_num = 0;
        }

        return 0;
	}
	else
	{
      //DBG_DEBUG("ApplLastSeqNum: %s", appl_last_seq_num_field.field_val);
      last_seq_num = atoi(appl_last_seq_num_field.field_val);
	    // update
	    int last_seq = find_last_seq(session_field.field_val);
	    //DBG_DEBUG("session : %s, last seq=%d", session_field.field_val, last_seq);
	    if(last_seq == -1)
		{
		    DBG_DEBUG("no session associated!");
		    return -1;
		}
	    else
		{
		    modify_last_seq(session_field.field_val, atoi(appl_seq_num_field.field_val));
		}
	}

    // trade date 
    if (get_fix_field(fix_msg, "75", &trade_date_field) == -1)  
      { 
        DBG_ERR("Fail to get trade_date_field");  
        return true;  
      }  
    //DBG_DEBUG("Trade_date: %s", trade_date_field.field_val); 
 
    char date_buffer[9];
    memset(date_buffer, 0, sizeof(date_buffer));  
    EtUtil::build_date(date_buffer, sizeof(date_buffer)); 
    if(strncmp(date_buffer, trade_date_field.field_val, 8) != 0){ 
		memset(date_buffer, 0, sizeof(date_buffer));
		EtUtil::build_date(date_buffer, sizeof(date_buffer), 1);
		if(strncmp(date_buffer, trade_date_field.field_val, 8) == 0){
			is_yesterday = true;
			DBG_DEBUG("Yesterday trade.");
		}
		else{
			DBG_DEBUG("Not today, Not yesterday, return.");
			return true;
		}
    }

	print_sessions();

	// ----------------------
	// order rejected
	// OrdStatus = 8, rejected
	// ----------------------
	if (strcmp(order_status_field.field_val, "8") == 0)
	    {	
		// tag: 58 - Text
		if (get_fix_field(fix_msg, "58", &text_field) == -1)
		    {
			DBG_ERR("Fail to get rejected text");
		    }
		else
		    {
			DBG_DEBUG("Success to get rejected text: %s",	text_field.field_val);
		    }

		return (0);
	    }

	// ---------------------
	// order accepted
	// OrdStatus = 0, New
	// ---------------------
	else if (strcmp(order_status_field.field_val, "0") == 0)
	{
	    DBG_DEBUG("New order");	
	    return (0);
	}

	// -------------------------------------
	// order filled
	// OrderStatus = 1, Partially filled
	// OrderStatus = 2, Filled
	// -------------------------------------
	else if ( (strcmp(order_status_field.field_val, "1") == 0) || 
		      (strcmp(order_status_field.field_val, "2") == 0) )
	{
	    // tag: 31 - LastPx
	    if (get_fix_field(fix_msg, "31", &price_field) == -1)
	    {
		DBG_ERR("Fail to get LastPx field");
		return (-1);
	    }
	    double dFillPrice = atof(price_field.field_val);
	    int iPrice = atoi(price_field.field_val);
	    //DBG_DEBUG("dFillPrice=%ld, iPrice=%d", dFillPrice, iPrice);
	    
	    // tag: 32 - LastShares
	    if (get_fix_field(fix_msg, "32", &size_field) == -1)
	    {
			DBG_ERR("Fail to get LastShares field");
			return (-1);
	    }
	    int fillSize = atoi(size_field.field_val);

	    // add information to dropcopy  
	    DBG_DEBUG("order filled, symbol: %s, security name: %s, price_field: %s, filled_size: %s, source session: %s", 
		      symbol_field.field_val, inst_display_field.field_val, price_field.field_val, 
		      size_field.field_val, session_field.field_val);
		
	    // convert double to int
	    if ( !EtUtil::doubleToInt(inst_display_field.field_val, dFillPrice, iPrice) )
	    {
          DBG_ERR("fail to convert price.");
          return -1;
	    }
	    //DBG_DEBUG("after doubleToInt, iPrice=%d", iPrice);
	    iPrice = atoi(price_field.field_val);
	    
	    // ask
	    if (strcmp(order_side_field.field_val,"2") == 0)
	    {
		fillSize = 0 - fillSize;
	    }

	    // if outright, then prepare to send to positionSrv
	    if( strlen(inst_display_field.field_val) < 5 )
	    {
          et_g_dropcopy->add_dropcopy(inst_display_field.field_val, session_field.field_val, account_field.field_val, 
                                      iPrice, fillSize, curr_appl_seq_num, is_yesterday);
	    }
	    
	    return (0);
	}

	// ---------------------------
	// order cancelled
	// OrderStatus = 4, Canceled
	// OrderStatus = C, Expired
	// ---------------------------
	else if	( (strcmp(order_status_field.field_val, "4") == 0) ||
		      (strcmp(order_status_field.field_val, "C") == 0) )
	{
		// tag: 151 - LeavesQty
		if (get_fix_field(fix_msg, "151", &quant_left_field) == -1)
	    {
	    	DBG_ERR("Fail to get LeavesQty field.");
	    }
		else
		{
			DBG_DEBUG("LeavesQty: %s", quant_left_field.field_val);
		}

		// tag: 38 - OrderQty
		if (get_fix_field(fix_msg, "38", &order_quant_field) == -1)
	    {
	    	DBG_ERR("Fail to get OrderQty field.");
			return (-1);
	    }
	    //DBG_DEBUG("OrderQty: %s", order_quant_field.field_val);

		return (0);
	}
	

	// --------------------------
	// order altered
    // OrderStatus = 5, Replaced
    // --------------------------
	else if (strcmp(order_status_field.field_val, "5") == 0)
    {   
	    // tag: 44 - Price
		if (get_fix_field(fix_msg, "44", &price_field) == -1)
	    {
			DBG_ERR("Fail to get Price field");
			return (-1);
	    }
	    //DBG_DEBUG("Price: %s", price_field.field_val);

		// tag: 151 - LeavesQty
		if (get_fix_field(fix_msg, "151", &quant_left_field) == -1)
	    {
	    	DBG_ERR("fail to get LeavesQty field.");
	    }
		else
	    {
	    	DBG_DEBUG("LeavesQty: %s", quant_left_field.field_val);
	    }

		// tag: 38 - OrderQty
		if (get_fix_field(fix_msg, "38", &order_quant_field) == -1)
	    {
	    	DBG_ERR("fail to get OrderQty field.");
			return (-1);
	    }
		//DBG_DEBUG("OrderQty: %s", order_quant_field.field_val);
	
		return (0);
	}

	// -----------------------------------------
	// order unfilled (trade cancelled)
	// OrderStatus = H, Not find in fix 4.2
	// -----------------------------------------
	else if (strcmp(order_status_field.field_val, "H") == 0)
    {
    	// tag: 31 - LastPx
		if (get_fix_field(fix_msg, "31", &price_field) == -1)
	    {
			DBG_ERR("Fail to find LastPx field");
			return (-1);
	    }

		// tag: 31 - LastShares
		if (get_fix_field(fix_msg, "32", &size_field) == -1)
		{
			DBG_ERR("Fail to find LastShares field");
			return (-1);
		}

		//DBG_DEBUG("trade was cancelled: name: %s, price: %s, size: %s", inst_display_field.field_val, price_field.field_val, size_field.field_val);
		return (0);
    }
    
    DBG_ERR("order status is not handle: %s", order_status_field.field_val);
	return (-1);
}


int exec_globex::handle_globex_cancel_reject_message(fix_message_struct *fix_msg)
{
	fix_field_struct order_id_field;
	fix_field_struct ex_order_id_field;
	fix_field_struct order_status_field;
	fix_field_struct text_field;

	char comp_id[0xf];
	
	//DBG_DEBUG("Cancel order was rejected.");
	// tag: 39, OrdStatus
	if (get_fix_field(fix_msg, "39", &order_status_field) == -1)
    {
		DBG_ERR("Fail to get order status.");
		return (-1);
    }
    //DBG_DEBUG("OrdStatus: %s", order_status_field.field_val);


	// tag: 37, OrderID
	if (get_fix_field(fix_msg, "37", &ex_order_id_field) == -1)
    {
		DBG_ERR("Fail to get exchange order id, ord_status=%s ord_id=%s.",
			order_status_field.field_val, order_id_field.field_val);
		return (-1);
    }
    //DBG_DEBUG("OrderID: %s", ex_order_id_field.field_val);

	// tag: 58, Text
	if (get_fix_field(fix_msg, "58", &text_field) == -1)
    {
    	DBG_ERR("cancel reject, comp_id: %s, denied: (%s)", comp_id, "reason not provided");
    }
	else
    {
    	DBG_DEBUG("cancel reject, comp_id: %s, denied: (%s)", comp_id, text_field.field_val);
    }

	return (0);
}

int exec_globex::addToApplReserve(fix_message_struct* fix_msg)
{
	DBG_DEBUG("Add to reserve queue!");
	applReserveQueue.push(fix_msg);
	return 0;
}

int exec_globex::addToFixReserve(fix_message_struct* fix_msg) 
{ 
  DBG_DEBUG("Add to reserve queue!"); 
  fixReserveQueue.push(fix_msg); 
  return 0; 
}

int exec_globex::applyFixReserve()
{
	DBG_DEBUG("Apply reserve queue!");
	fix_message_struct* msg;
	fix_field_struct seq_field;
	int new_seq_num;

	while(!fixReserveQueue.empty())
	    {
          msg = fixReserveQueue.front();
          fixReserveQueue.pop();
          
          // tag: 34 - MsgSeqNum
          if (get_fix_field(msg, "34", &seq_field) == -1)
		    {
              DBG_ERR("fail to get 34 field.");
              continue;
		    }
          new_seq_num = atoi(seq_field.field_val);
          DBG_DEBUG("Server SeqNum: %d", new_seq_num);
          
          // no consider for message miss in the reserve queue
          recv_order_message_num = new_seq_num + 1;
          parse_field(msg, new_seq_num);
	    }

	return 0;
}

int exec_globex::applyApplReserve()
{
    DBG_DEBUG("Apply Application reserve queue!");
    fix_message_struct* msg;

    while(!applReserveQueue.empty())
	{
	    msg = applReserveQueue.front();
	    applReserveQueue.pop();
	    check_sequence(msg);
	}

    DBG_DEBUG("Finish apply application reserve queue.");
    return 0;
}

int exec_globex::find_last_seq(const char* passed_name)
{
  //DBG_DEBUG("enter into find_last_seq. passed_anme= %s, size = %d", passed_name, sizeof(passed_name));
	session_last_seq_struct* curr_last_seq = NULL;
	for(curr_last_seq = last_seq_list; curr_last_seq != NULL; curr_last_seq = curr_last_seq->next)
	{
	    //DBG_DEBUG("curr_last_seq->name = %s, size = %d", curr_last_seq->name, sizeof(curr_last_seq->name));
		if( strcmp(curr_last_seq->name, passed_name) == 0)
		{
          //DBG_DEBUG("find session in list.");
			return curr_last_seq->last_seq;
		}
	}

	DBG_ERR("not find this session name in list: %d", passed_name);
	return (-1);
}

/*
int exec_globex::modify_req_bool(char* passed_name, bool passed_bool)
{
	appl_req_bool_struct* curr_req_bool = NULL;
	for(curr_req_bool = appl_req_list; curr_req_bool != NULL; curr_req_bool = curr_req_bool->next)
	{
		if( strcmp(curr_req_bool->name, passed_name) == 0)
		{
			curr_req_bool->is_appl_req = passed_bool;
			return (0);
		}
	}
	DBG_ERR("do not find the session: %d", passed_name);
	return (-1);
}
*/

int exec_globex::modify_last_seq(char* passed_name, int passed_seq)
{
	session_last_seq_struct* curr_last_seq = NULL;
	for(curr_last_seq = last_seq_list; curr_last_seq != NULL; curr_last_seq = curr_last_seq->next)
	{
		if(strcmp(curr_last_seq->name, passed_name) ==0)
		{
			curr_last_seq->last_seq = passed_seq;
			return (0);
		}
	}
	DBG_ERR("do not find the session: %d", passed_name);
	return (-1);
}
	
void exec_globex::print_sessions()
{
	session_last_seq_struct* curr_last_seq = NULL;

	for (curr_last_seq = last_seq_list; curr_last_seq != NULL; curr_last_seq = curr_last_seq->next)
	    {
			//DBG_DEBUG("%s, %d", curr_last_seq->name, curr_last_seq->last_seq);
	    }
    //DBG_DEBUG("The total number of sessions: %d", i);
}

