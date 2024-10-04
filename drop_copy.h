#ifndef DROP_COPY_H
#define DROP_COPY_H

#include <queue>
#include <iostream>
#include <string>
#include <string.h>
#include <cstring>
#include "globex_common.h"
#include "client_message.h"

using namespace std;

// -------------
// struct
// -------------

struct dropcopy_struct
{
  char session[10];
	char account[20];
	char symbol[20];
	int size;
	int price;
  int appl_seq_num;
	dropcopy_struct* next;
};


class drop_copy
{
public:
	drop_copy();
	~drop_copy();

	bool add_dropcopy(const char* _symbol, const char* _session, const char* _account, int _price, 
                      int _size, int _apll_seq_num, bool _is_yesterday);
	bool send_dropcopy_info(int _socket);

private:
	
	// function
	void build_dropcopy_info(EtClientMessage& _cm, dropcopy_struct& _node, bool _is_yesterday);
	
	bool send_dropcopy_info(dropcopy_struct& _node, bool _is_yesterday);

	// data
	dropcopy_struct* drop_list;
	dropcopy_struct* last_node;
	pthread_mutex_t m_dlist;
};

#endif
