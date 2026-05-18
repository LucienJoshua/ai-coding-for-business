#ifndef __TLS_REQUEST_H__
#define __TLS_REQUEST_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tls_com.h"
#include "tls_type.h"
#include "tls_list.h"
#include "tls_thread.h"
#include "tls_event.h"
#include "tls_lock.h"
#include "app_linkinfo.h"
#include "tls_protocol.h"
#include "tls_linkinfo.h"


typedef struct __tls_command
{
	tls_list_node_t			cmd_node_;
	tls_event_t				cmd_event_;

	U32						cmd_timeout_;
	U8						cmd_data_[2048];
	U32						cmd_data_size_;

	U8						cmd_id_[16];
	U32						cmd_seq_;
	void*					cmd_usr_;

	U32						cmd_result_;
	U64						cmd_tv_start_;
	U64						cmd_tv_tick_;
	void					(*cmd_callback_)(struct __tls_command_t* cmd, void* arg);
}tls_command_t;

typedef struct __tls_request
{
	S32						start_;
	
	tls_list_node_t			queue_wait_cmd_;
	tls_lock_t				queue_wait_cmd_lock_;

	void*					tls_server_;

	S32						thread_run_;
	tls_thread_t			thread_command_;
	tls_event_t				evt_thread_loop_;
	tls_event_t				evt_thread_stop_;
}tls_request_t;


typedef struct __tls_request_param
{
	U32						dst_addr_;
	U16						dst_entry_;
}tls_request_param_t;



extern TLS_RESULT			tls_request_initialize(void* tls_server);
extern void					tls_request_uninitialize(void* tls_server);

extern tls_request_t*		tls_request_allocate(void);
extern void					tls_request_free(tls_request_t* req);

extern TLS_RESULT			tls_request_start(tls_request_t* req);
extern void					tls_request_stop(tls_request_t* req);


extern TLS_RESULT			tls_request_link_status(tls_request_t* req, link_device_info_t* info);

extern TLS_RESULT			tls_request_link_info(tls_request_t* req, 
													const app_linkinfo_request_t* linkinfo_req, 
													app_linkinfo_result_t* linkinfo_result);

extern void					tls_request_on_response(void* app, tls_path_msg_t* msg_path);

#ifdef __cplusplus
}
#endif

#endif // __TLS_REQUEST_H__
