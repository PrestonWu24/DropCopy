
#ifndef _GLOBEX_COMMON_H_
#define _GLOBEX_COMMON_H_


#define SOH							    0x01

#define ORDER_BUFFER_SIZE               4096
#define	GLOBEX_HEART_BEAT_INTERVAL		"60"

#define GLOBEX_ACCT                     "16097"
#define EXPIRE_DATE					    "20101223"

#define	ORDER_DELAY					    2.00
#define	MAX_STRING_ORDER_ID			    32

#define	MIP_BUFFER_LEN					1024

// drop copy test only
#define ORDER_USER_NAME    "FCTYANG"

/*
#define SESSION_ID                    "W7P"
#define FIRM_ID                       "5S1"
#define ORDER_PASSWORD     "tiatt"
#define DROPCOPY_SESSION_ID "OHBJDDN"
#define HOST_ORDER_IP      "10.140.18.32"
#define HOST_ORDER_PORT    9819

#define SESSION_ID_1                  "W7P"
*/

#define ORDER_PASSWORD     "ivenz" 
#define DROPCOPY_SESSION_ID "OJHGBHN" 

/*
#define ORDER_PASSWORD     "tiatt" 
#define DROPCOPY_SESSION_ID "OHBJDDN"
*/

typedef enum
{
	NOT_LOGGED_IN = 0, 
	NEED_TO_LOGIN, 
	LOGIN_CONNECTED, 
	SENT_LOGIN, 
	LOGGED_IN, 
	LOGGED_IN_AND_SUBSCRIBED, 
	LOGGING_OUT
} login_state_type;

typedef enum
{
	DAY = 0,
	GOOD_TILL_CANCEL = 1,
	FILL_OR_KILL = 3,
	GOOD_TILL_DATE = 6
} tif_type;


typedef enum
{
	FLOOR_ROUND = 0, 
	REG_ROUND, 
	CEIL_ROUND
} dec_round_type;


typedef enum 
{
	ORDER_SENT_NEW,
	ORDER_IN_MARKET,
	ORDER_SENT_ALTER,
	ORDER_SENT_CANCEL,
	ORDER_REJECT
} order_stage_type;


// ---- side_type ----
#define BID_SIDE      0    // FIX: 1
#define ASK_SIDE      1    // FIX: 2
#define CROSS_SIDE    7    // FIX: 8
// ---- side_type end ----


// ---- order_execute_type ---------
#define MARKET_ORDER          '1'
#define LIMIT_ORDER           '2'
#define STOP_ORDER            '3'
#define STOP_LIMIT_ORDER      '4'
#define MARKET_LIMIT_ORDER    'K'
// ---- order_execute_type end ----


struct fix_field_struct
{
	char* field_id;
	char* field_val;
	struct fix_field_struct* next;
};


struct fix_message_struct
{
	char* msg_type;
	fix_field_struct* fields;
};

struct session_last_seq_struct
{
	char* name;
	int last_seq;
	struct session_last_seq_struct* next;
};


#endif

