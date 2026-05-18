#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "tls_server.h"
#include "tls_log.h"
#include "tls_protocol.h"
#include "tls_request.h"
#include "tls_vsoa_subscribe.h"

static void vsoa_on_subscribe_connect(void* arg, vsoa_client_auto_t* client_auto, bool connect, const char* info);

static void vsoa_on_subscribe_message(void* arg, struct vsoa_client* client, vsoa_url_t* url, vsoa_payload_t* payload, bool quick);


tls_vsoa_subscribe_t* vsoa_subscribe_allocate(void)
{
	tls_vsoa_subscribe_t*	subscribe = NULL;
	subscribe = (tls_vsoa_subscribe_t*)malloc(sizeof(tls_vsoa_subscribe_t));
	assert(subscribe);
	return subscribe;
}
void vsoa_subscribe_free(tls_vsoa_subscribe_t* subscribe)
{
	if (subscribe)
		free(subscribe);
}

TLS_RESULT vsoa_subscribe_start(tls_vsoa_subscribe_t* subscribe)
{
	TLS_RESULT					result;
	int							oldvalue;
	int							newvalue;
	int							ret;
	tls_server_t*				tls_server;
	struct tls_vsoa_subscribe_param* subscribe_param;


	int					vsoa_url_count = 1;
	int					vsoa_keepalive = 1000;
	int					vsoa_conn_timeout = 1000;
	int					vsoa_reccon_delay = 1000;;

	do
	{
		if (NULL == subscribe)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			LOG_ERROR("ERROR-Invalid parameters.");
			break;
		}
		oldvalue = 0;
		newvalue = 1;
		if (__sync_val_compare_and_swap(&subscribe->vsoa_start_, oldvalue, newvalue))
		{
			result = TLS_RESULT_S_CONTINUE;
			LOG_INFO("VSOA subscribe is already started.");
			break;
		}

		tls_server = subscribe->tls_server_;
		if (NULL == tls_server)
		{
			result = TLS_RESULT_E_INVALID_DATA;
			LOG_ERROR("ERROR-VSOA subscribe invalid data.");
			break;
		}
		subscribe_param = &tls_server->svr_param_.subscribe_param_;

		strcpy(subscribe->server_name_, subscribe_param->server_name_);
		strcpy(subscribe->server_password_, subscribe_param->server_password_);
		strcpy(subscribe->subscribe_url_, subscribe_param->subscribe_url_);
		subscribe->urls_[0] = subscribe->subscribe_url_;
		subscribe->url_count_ = 1;
		subscribe->arg_ = subscribe_param->arg;
		subscribe->on_subscribe_connect = subscribe_param->on_subscribe_connect;
		subscribe->on_subscribe_message = subscribe_param->on_subscribe_message;
		subscribe->on_subscribe_receive_data = subscribe_param->on_subscribe_receive_data;

		if (0 == strlen(subscribe->server_name_) || 0 == strlen(subscribe->subscribe_url_))
		{
			result = TLS_RESULT_E_INVALID_DATA;
			LOG_ERROR("ERROR-VSOA subscribe invalid data.");
			break;
		}

		subscribe->vsoa_client_auto_ = vsoa_client_auto_create(vsoa_on_subscribe_message, subscribe);
		if (NULL == subscribe->vsoa_client_auto_)
		{
			result = TLS_RESULT_E_FAIL;
			LOG_ERROR("ERROR-VSOA client auto create fail.");
			break;
		}
		subscribe->vsoa_client_ = vsoa_client_auto_handle(subscribe->vsoa_client_auto_);
		if (NULL == subscribe->vsoa_client_)
		{
			result = TLS_RESULT_E_INVALID_DATA;
			LOG_ERROR("ERROR-VSOA client auto handle fail.");
			break;
		}
		ret = vsoa_client_auto_setup(subscribe->vsoa_client_auto_, vsoa_on_subscribe_connect, subscribe);
		if (!ret)
		{
			result = TLS_RESULT_E_FAIL;
			LOG_ERROR("ERROR-VSOA client auto setup fail, errno:%d, errstr:%s.", errno, strerror(errno));
			break;
		}
#if 0
		ret = vsoa_client_auto_start(subscribe->vsoa_client_auto_, subscribe->server_name_, 
				subscribe->server_password_, (char *const)subscribe->subscribe_url_, 
				vsoa_url_count, vsoa_keepalive, vsoa_conn_timeout, vsoa_reccon_delay);
#endif
		LOG_INFO("------------------- tls subscribe config -------------------");
		LOG_INFO("subscribe server name:%s.", subscribe->server_name_);
		LOG_INFO("subscribe server pass:%s.", subscribe->server_password_);
		LOG_INFO("subscribe url:%s.", subscribe->subscribe_url_);
		LOG_INFO("------------------------------------------------------------");
		ret = vsoa_client_auto_start(subscribe->vsoa_client_auto_, subscribe->server_name_, subscribe->server_password_, 
				(char* const)subscribe->urls_, subscribe->url_count_, vsoa_keepalive, vsoa_conn_timeout, vsoa_reccon_delay);
		if (!ret)
		{
			result = TLS_RESULT_E_FAIL;
			LOG_ERROR("ERROR-VSOA client start fail, errno:%d, errstr:%s.", errno, strerror(errno));
			break;
		}
		LOG_INFO("VSOA subscribe start OK!");
		result = TLS_RESULT_S_OK;
	}while(0);

	if (TlsResultFail(result))
	{
		vsoa_subscribe_stop(subscribe);
	}
	return result;
}
void vsoa_subscribe_stop(tls_vsoa_subscribe_t* subscribe)
{
	int				oldvalue;
	int				newvalue;

	if (NULL == subscribe)
		return;

	oldvalue = 1;
	newvalue = 0;
	if (!__sync_val_compare_and_swap(&subscribe->vsoa_start_, oldvalue, newvalue))
	{
		LOG_INFO("VSOA subscribe already stop.");
		return;
	}

	if (subscribe->vsoa_client_auto_ != NULL)
	{
		vsoa_client_auto_stop(subscribe->vsoa_client_auto_);
		vsoa_client_auto_delete(subscribe->vsoa_client_auto_);
		subscribe->vsoa_client_auto_ = NULL;
	}

}

void vsoa_on_subscribe_connect(void* arg, vsoa_client_auto_t* client_auto, bool connect, const char* info)
{
	LOG_DEBUG("Connect status:%s, info:%s.", (connect == true) ? "connected":"disconnected", info);
}

void vsoa_on_subscribe_message(void* arg, struct vsoa_client* client, vsoa_url_t* url, vsoa_payload_t* payload, bool quick)
{
	tls_vsoa_subscribe_t*		tls_vsoa_subscribe = NULL;
	PU8							data;
	U32							size;
	tls_message_head_t*			msg_hdr;
	tls_message_content_t*		msg_content;
	tls_path_msg_t*				msg_path;

	if (NULL == arg || NULL == payload || NULL == payload->data)
		return;

	assert(url);
	assert(payload);


	tls_vsoa_subscribe = (tls_vsoa_subscribe_t*)arg;
	data = payload->data;
	size = payload->data_len;

#if 0
	msg_hdr = (tls_message_head_t*)data;

	if (MSG_TYPE_RECV_MESSAGE == msg_hdr->message_type)
	{
		msg_content = (tls_message_content_t*)(data + sizeof(tls_message_head_t));
		size = ntohs(msg_content->size);
		data = msg_content->payload;
	
		if (tls_vsoa_subscribe->on_subscribe_receive_data)
		{
			tls_vsoa_subscribe->on_subscribe_receive_data(tls_vsoa_subscribe->arg_, data, size);
		}
	}
	else if (MSG_TYPE_LINK_STATUS_RESPONSE == msg_hdr->message_type)
	{
		msg_path = (tls_path_msg_t*)data;

		tls_request_on_response(tls_vsoa_subscribe->arg_, msg_path);
	}
#endif
	if (tls_vsoa_subscribe->on_subscribe_receive_data)
	{
		tls_vsoa_subscribe->on_subscribe_receive_data(tls_vsoa_subscribe->arg_, data, size);
	}

}

