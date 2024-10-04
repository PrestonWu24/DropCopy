
#include <iostream>
#include <queue>
#include <string>
#include <string.h>

#include "drop_copy.h"
#include "trace_log.h"
#include "msg_protocol.h"
#include "trade_socket_list.h"

extern EtTradeSocketList* g_socketListPtr;


drop_copy::drop_copy()
{

}


drop_copy::~drop_copy()
{

}


/**
 * build the package sending the market's data.
 */
void drop_copy::build_dropcopy_info(EtClientMessage& _cm, dropcopy_struct& _node, bool _is_yesterday)
{
	_cm.clearInMap();
	_cm.putField(fid_msg_type, msg_posi_position);
	_cm.putField(fid_posi_account, _node.account);
	_cm.putField(fid_posi_symbol, _node.symbol);
	_cm.putField(fid_posi_price, _node.price);
    _cm.putField(fid_posi_seq_num, _node.appl_seq_num);
	_cm.putField(fid_posi_xtt_qty, _node.size);

	if(_is_yesterday){
		_cm.putField(fid_posi_yesterday, 1);
	}
	else{
		_cm.putField(fid_posi_yesterday, 0);
	}
}


/**
 * When accept a new connection, should send all fill inforation 
 * to this connection.
 */

bool drop_copy::send_dropcopy_info(int _socket)
{
	if (-1 == _socket)
	{
		DBG_ERR("socket is error.");
		return false;
	}
	
	EtClientMessage cm;
	dropcopy_struct *curr_node = NULL;

	curr_node = drop_list;
	
	while (curr_node != NULL)
	{
		build_dropcopy_info(cm, *curr_node, false);
		
		if ( !cm.sendMessage(_socket) )
		{
			DBG_ERR("Fail to send package to client.");
			return false;
		}
		curr_node = curr_node->next;
	}

	return true;
}


/**
 * send new fill information to all PositionSrv.
 */
bool drop_copy::send_dropcopy_info(dropcopy_struct& _node, bool _is_yesterday)
{
	EtClientMessage cm;
	dropcopy_struct *curr_node = NULL;

	curr_node = &_node;
	
	if (curr_node != NULL)
	{
		build_dropcopy_info(cm, _node, _is_yesterday);
	}

	if ( !g_socketListPtr->sendDataToServers(cm) )
	{
		DBG_ERR("fail to send position to all server.");
		return false;
	}
	else
	{
		DBG_DEBUG("send to all server");
	}
	return true;
}


bool drop_copy::add_dropcopy(const char* _symbol, const char* _session, const char* _account, int _price, 
                             int _size, int _appl_seq_num, bool _is_yesterday)
{
	dropcopy_struct *new_node = NULL;
	new_node = new dropcopy_struct;
	memset(new_node, 0, sizeof(dropcopy_struct));

	strcpy(new_node->symbol, _symbol);
    strcpy(new_node->session, _session);
	strcpy(new_node->account, _account);
	new_node->price = _price;
	new_node->size = _size;
    new_node->appl_seq_num = _appl_seq_num;
	
	//DBG_DEBUG("add_dropcopy, symbol: %s, session: %s, account: %s, price: %d, size: %d, appl seq num: %d", 
	//        _symbol, _session, _account, _price, _size, _appl_seq_num);
		
	send_dropcopy_info(*new_node, _is_yesterday);
	delete new_node;

	return true;
}

