#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "app_transmit.h"
#include "tls_log.h"
#include "tls_afx.h"
#include "tls_vsoa_publish.h"
#include "tls_server.h"
#include "tls_request.h"


#define __LOCK_COMMAND_QUEUE(req) \
	tls_lock(&((req)->queue_wait_cmd_lock_))

#define __UNLOCK_COMMAND_QUEUE(req) \
	tls_unlock(&((req)->queue_wait_cmd_lock_))
	

static void				tls_request_start_command_thread(tls_request_t* req);
static void				tls_request_stop_command_thread(tls_request_t* req);
static void*			tls_request_command_thread(void* arg);
static void				tls_request_command_poll(tls_request_t* req);
static TLS_RESULT		tls_request_build_sessionid(PU8 session_id);

static tls_command_t*	tls_command_allocate(void);
static void				tls_command_initialize(tls_command_t* cmd);
static void				tls_command_uninitialize(tls_command_t* cmd);
static void				tls_command_free(tls_command_t* cmd);

static void				tls_command_callback(tls_command_t* cmd, void* arg);
static void				tls_command_push_queue(tls_request_t* req, tls_command_t* cmd);
static void				tls_command_remove_queue(tls_request_t* req, tls_command_t* cmd);
static tls_command_t*	tls_command_find(tls_request_t* req, const PU8 id);
static inline U64		tls_command_get_time_value(void);
static void				tls_command_call_cb(tls_command_t* cmd, S32 result);


static inline void		tls_copy_linkinfo_toapp(app_linkinfo_result_t* app_result, const tls_path_msg_t* msg_path);



TLS_RESULT tls_request_initialize(void* tls_server)
{
	TLS_RESULT		result = TLS_RESULT_E_FAIL;
	tls_server_t*	server = NULL;
	tls_request_t*	req = NULL;

	if (NULL == tls_server)
		return TLS_RESULT_E_INVALID_PARAM;

	server = (tls_server_t*)tls_server;

	req = tls_request_allocate();
	if (NULL == req)
		return TLS_RESULT_E_ALLOCATE;

	result = tls_request_start(req);
	if (TlsResultOk(result))
	{
		server->tls_request_ = req;
		req->tls_server_ = server;
	}
	else
	{
		LOG_ERROR("Start request fail\n");
	}
	return result;
}
void tls_request_uninitialize(void* tls_server)
{
	tls_server_t*	server = NULL;
	tls_request_t*	req = NULL;

	if (NULL == tls_server)
		return TLS_RESULT_E_INVALID_PARAM;

	server = (tls_server_t*)tls_server;
	req = server->tls_request_;

	tls_request_stop(req);
	tls_request_free(req);
	server->tls_request_ = NULL;
}

tls_request_t* tls_request_allocate(void)
{
	tls_request_t*	req;
	req = (tls_request_t*)malloc(sizeof(tls_request_t));
	return req;
}
void tls_request_free(tls_request_t* req)
{
	if (req)
		free(req);
}

TLS_RESULT tls_request_start(tls_request_t* req)
{
	TLS_RESULT			result = TLS_RESULT_E_FAIL;
	int					oldvalue;
	int					newvalue;

	do
	{
		if (NULL == req)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}	

		oldvalue = 0;
		newvalue = 1;
		if (__sync_val_compare_and_swap(&req->start_, oldvalue, newvalue))
		{
			LOG_INFO("tls request has already started.");
			result = TLS_RESULT_S_CONTINUE;
			break;
		}

		tls_list_initialize(&req->queue_wait_cmd_);
		tls_lock_init(&req->queue_wait_cmd_lock_);

		tls_event_initialize(&req->evt_thread_loop_);	
		tls_event_initialize(&req->evt_thread_stop_);	

		tls_request_start_command_thread(req);

		result = TLS_RESULT_S_OK;
	}while(0);
	if (TlsResultFail(result))
		tls_request_stop(req);
	return result;
}
void tls_request_stop(tls_request_t* req)
{
	S32			oldvalue;
	S32			newvalue;

	oldvalue = 1;
	newvalue = 0;
	if (!__sync_val_compare_and_swap(&req->start_, oldvalue, newvalue))
	{
		LOG_INFO("TLS request has already stop");
		return;
	}
	
	tls_request_stop_command_thread(req);
	tls_event_uninitialize(&req->evt_thread_loop_);
	tls_event_uninitialize(&req->evt_thread_stop_);
	tls_lock_uninit(&req->queue_wait_cmd_lock_);
}

TLS_RESULT tls_request_linkstatus(tls_request_t* req, tls_command_t* cmd)
{
	return TLS_RESULT_S_OK;
}

void tls_request_start_command_thread(tls_request_t* req)
{
	assert(req);
	memset(&req->thread_command_, 0, sizeof(req->thread_command_));
	req->thread_command_.arg_ = (void*)req;
	req->thread_command_.thread_routine_ = tls_request_command_thread;
	req->thread_run_ = 1;

	tls_event_set(&req->evt_thread_loop_);
	tls_thread_start(&req->thread_command_);
}
void tls_request_stop_command_thread(tls_request_t* req)
{
	if (NULL == req)
		return;

	req->thread_run_ = 0;
	tls_event_set(&req->evt_thread_loop_);
	tls_event_timewait(&req->evt_thread_stop_, 3000);
	tls_thread_stop(&req->thread_command_);
}
void* tls_request_command_thread(void* arg)
{
	tls_request_t* req = NULL;

	if (NULL == arg)
		return;

	req = (tls_request_t*)arg;

	tls_event_reset(&req->evt_thread_stop_);
	while (req->thread_run_)
	{
		tls_event_wait(&req->evt_thread_loop_);
		if (0 == req->thread_run_)
			break;

		tls_request_command_poll(req);
		usleep(1000);
	}
	tls_event_set(&req->evt_thread_stop_);
	return NULL;
}
void tls_request_command_poll(tls_request_t* req)
{
	tls_list_node_t*	list_head;
	tls_list_node_t*	list_node;
	tls_command_t*		cmd = NULL;

	__LOCK_COMMAND_QUEUE(req);

	list_head = &req->queue_wait_cmd_;
	list_node = __LIST_START_NODE(list_head);
	while (__LIST_IS_LAST_NODE(list_head, list_node))
	{
		cmd = __LIST_CONTAIN_OF(list_node, tls_command_t, cmd_node_);
		if (NULL == cmd)
		{
			list_node = __LIST_NEXT_NODE(list_node);
			continue;
		}

		if (0 == cmd->cmd_result_)
		{
			// __TLSDBG("cmd timeout:%d\n", cmd->cmd_timeout_);
			if (cmd->cmd_tv_tick_ - cmd->cmd_tv_start_ >= cmd->cmd_timeout_)
			{
				tls_command_call_cb(cmd, TLS_RESULT_E_TIMEOUT);
			}
			cmd->cmd_tv_tick_ = tls_command_get_time_value();
		}
		list_node = __LIST_NEXT_NODE(list_node);
	}	
	__UNLOCK_COMMAND_QUEUE(req);
}
TLS_RESULT tls_request_build_sessionid(PU8 sessionid)
{
	U8		id[16];

	int		random;

	if (NULL == sessionid)
		return TLS_RESULT_E_INVALID_PARAM;


	memset(id, 0, sizeof id);

	srand((unsigned)time(NULL));
	random = rand();
	memcpy(id, &random, sizeof(random));

	memcpy(sessionid, id, sizeof(id));
}
TLS_RESULT tls_request_link_info(tls_request_t* req, const app_linkinfo_request_t* linkreq, app_linkinfo_result_t* result)
{
	int							ret;
	tls_server_t*				tls_server;
	tls_param_t*				server_param;
	tls_vsoa_publish_t*			tls_publish;
	tls_command_t*				command = NULL;

	tls_net_status_req_msg_t	status_req_msg;
	tls_message_head_t*			status_msg_head;
	U8							sessionid[16];
	tls_path_msg_t*				status_path_msg;


	assert(req);
	if (NULL == linkreq || NULL == result)
		return TLS_RESULT_E_INVALID_PARAM;

	tls_server = req->tls_server_;
	server_param = &tls_server->svr_param_;
	tls_publish = tls_server->tls_vsoa_publish_;

	memset(&status_req_msg, 0, sizeof(status_req_msg));
	status_msg_head = &status_req_msg.msg_header;

	status_msg_head->version = TLS_MSG_MAIN_VERSION;
	status_msg_head->message_type = MSG_TYPE_LINK_STATUS_REQUEST;
	status_msg_head->message_seq = htons(tls_server->sequence_++);

	tls_request_build_sessionid(sessionid);
	memcpy(status_req_msg.sessionid, sessionid, sizeof(sessionid));
	status_req_msg.src_addr			= htonl(server_param->tls_local_id_);
	status_req_msg.src_entry		= htons(server_param->tls_local_sub_id_);
	status_req_msg.dst_addr			= htonl(linkreq->dst_app_id);
	status_req_msg.dst_entry		= htons(linkreq->dst_entity_id);
	status_req_msg.path_policy		= htons(linkreq->path_policy);

	// create a command object and push to the wait queue
	command = tls_command_allocate();
	assert(command);
	tls_command_initialize(command);
	memcpy(command->cmd_id_, &status_req_msg.sessionid, sizeof(status_req_msg.sessionid));
	tls_command_push_queue(req, command);
	vsoa_publish_data(tls_publish, (PU8)&status_req_msg, sizeof(status_req_msg));

	ret = tls_event_timewait(&command->cmd_event_, 3000);
	tls_command_remove_queue(req, command);

	if (ret != 0 && 0 == command->cmd_result_)
	{
		status_path_msg = (tls_path_msg_t*)command->cmd_data_;
		tls_copy_linkinfo_toapp(result, status_path_msg);
	}
	else
	{
		memset(result, 0, sizeof(*result));
		result->result = 0;
	}

done:
	if (command != NULL)
	{
		tls_command_uninitialize(command);
		tls_command_free(command);
	}
	return TLS_RESULT_S_OK;
}
tls_command_t* tls_command_allocate(void)
{
	tls_command_t* cmd;
	cmd = (tls_command_t*)malloc(sizeof(tls_command_t));
	return cmd;
}
void tls_command_initialize(tls_command_t* cmd)
{
	if (NULL == cmd)
		return;
	
	memset(cmd, 0, sizeof(tls_command_t));
	tls_list_initialize(&cmd->cmd_node_);
	tls_event_initialize(&cmd->cmd_event_);
	cmd->cmd_timeout_ = 2000 * 1000;
	cmd->cmd_callback_ = tls_command_callback;
}
void tls_command_uninitialize(tls_command_t* cmd)
{
	if (NULL == cmd)
		return;

	tls_list_initialize(&cmd->cmd_node_);
	tls_event_uninitialize(&cmd->cmd_event_);
}
void tls_command_free(tls_command_t* cmd)
{
	if (cmd)
		free(cmd);
}
void tls_command_callback(tls_command_t* cmd, void* arg)
{
	assert(cmd);

	tls_event_set(&cmd->cmd_event_);
}
void tls_command_push_queue(tls_request_t* req, tls_command_t* cmd)
{
	assert(req);
	assert(cmd);

	cmd->cmd_tv_start_ = tls_command_get_time_value();
	cmd->cmd_tv_tick_ = cmd->cmd_tv_start_;

	__LOCK_COMMAND_QUEUE(req);
	tls_list_insert_tail(&req->queue_wait_cmd_, &cmd->cmd_node_);
	__UNLOCK_COMMAND_QUEUE(req);
}
void tls_command_remove_queue(tls_request_t* req, tls_command_t* cmd)
{
	assert(req);
	assert(cmd);

	__LOCK_COMMAND_QUEUE(req);
	tls_list_remove_node(&req->queue_wait_cmd_, &cmd->cmd_node_);
	__UNLOCK_COMMAND_QUEUE(req);
}
tls_command_t*	tls_command_find(tls_request_t* req, const PU8 id)
{
	int					found = 0;
	tls_list_node_t*	list_head;
	tls_list_node_t*	list_node;
	tls_command_t*		cmd = NULL;

	__LOCK_COMMAND_QUEUE(req);

	list_head = &req->queue_wait_cmd_;
	list_node = __LIST_START_NODE(list_head);
	while (__LIST_IS_LAST_NODE(list_head, list_node))
	{
		cmd = __LIST_CONTAIN_OF(list_node, tls_command_t, cmd_node_);
		if (cmd)
		{
			if (0 == memcmp(cmd->cmd_id_, id, sizeof(cmd->cmd_id_)))
			{
				found = 1;
				break;
			}
		}
		list_node = __LIST_NEXT_NODE(list_node);
	}
	
	__UNLOCK_COMMAND_QUEUE(req);

	return found ? cmd : NULL;
}
inline U64 tls_command_get_time_value(void)
{
	struct timeval		tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1e6 + tv.tv_usec;
}
void tls_command_call_cb(tls_command_t* cmd, S32 result)
{
	if (NULL == cmd)
		return;

	if (TLS_RESULT_E_TIMEOUT == result)
	{
		// __TLSDBG("command timeout.\n");
		cmd->cmd_result_ = -1;
	}
	else if (TLS_RESULT_S_OK == result)
	{
		cmd->cmd_result_ = 1;
	}
	if (cmd->cmd_callback_)
	{
		cmd->cmd_callback_(cmd, cmd->cmd_usr_);
	}
}
void tls_request_on_response(void* app, tls_path_msg_t* msg_path)
{
	app_transmit_t*			app_transmit = NULL;
	tls_server_t*			tls_server = NULL;
	tls_request_t*			tls_request = NULL;
	tls_command_t*			command = NULL;

	if (NULL == app || NULL == msg_path)
		return;

	app_transmit = (app_transmit_t*)app;
	tls_server = (tls_server_t*)app_transmit->tls_server_;
	assert(tls_server);
	tls_request = (tls_request_t*)tls_server->tls_request_;
	assert(tls_request);

	command = tls_command_find(tls_request, msg_path->sessionid);
	if (NULL == command)
	{
		LOG_ERROR("CAN NOT find the response command!");
		return;
	}

	command->cmd_data_size_ = sizeof(*msg_path);
	memcpy(command->cmd_data_, msg_path, sizeof(*msg_path));
	tls_event_set(&command->cmd_event_);
}



inline void tls_copy_linkinfo_toapp(app_linkinfo_result_t* app_result, const tls_path_msg_t* msg_path)
{
	if (NULL == app_result || NULL == msg_path)
		return;

	app_result->src_app_id			= ntohl(msg_path->src_addr);
	app_result->src_entity_id		= ntohs(msg_path->src_entry);
	app_result->dst_app_id			= ntohl(msg_path->dst_addr);
	app_result->dst_entity_id		= ntohs(msg_path->dst_entry);

	app_result->reserve				= ntohl(msg_path->reserve);
	app_result->result				= !msg_path->result;
	app_result->pathnum				= msg_path->pathnum;

	memcpy(app_result->pathinfo, msg_path->pathinfo, sizeof(msg_path->pathinfo));
}
