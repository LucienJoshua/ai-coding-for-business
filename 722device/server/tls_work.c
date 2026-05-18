#include <assert.h>
#include <string.h>
#include "tls_afx.h"
#include "tls_log.h"
#include "tls_transmit.h"
#include "tls_receive.h"
#include "tls_protocol.h"
#include "tls_statis.h"
#include "tls_vsoa.h"
#include "tls_vsoa_publish.h"
#include "tls_statis_rate.h"
#include "tls_server.h"

void tls_server_active(tls_server_t* server)
{
	if (NULL == server)
		return;
	if (0 == server->start_)
		return;

	tls_event_set(&server->evt_work_loop_);
}
void tls_server_deactive(tls_server_t* server)
{
	if (NULL == server)
		return;
	if (0 == server->start_)
		return;

	tls_event_reset(&server->evt_work_loop_);
}

tls_buff_t*	tls_server_acquire_send_buff(tls_server_t* server)
{
	tls_buff_t* buff = NULL;
	buff = tls_buff_queue_acquire(&server->queue_send_buff_);
	if (NULL == buff)
		buff = tls_buff_create(TLS_BUFF_SIZE, TLS_BUFF_TYPE_TX);
	return buff;
}
tls_buff_t*	tls_server_acquire_recv_buff(tls_server_t* server)
{
	tls_buff_t* buff = NULL;
	buff = tls_buff_queue_acquire(&server->queue_recv_buff_);
	if (NULL == buff)
		buff = tls_buff_create(TLS_BUFF_SIZE, TLS_BUFF_TYPE_RX);
	return buff;
}
void tls_server_release_buff(tls_server_t* server, tls_buff_t* buff)
{
	U8					priority = 0;
	tls_buff_queue_t*	queue = NULL;

	if (NULL == server || NULL == buff)
		return;

	if (TLS_BUFF_TYPE_TX == buff->type_)
	{
		priority = buff->priority_ > TLS_QUEUE_PRI_7 ? 0 : buff->priority_;
		queue = &server->queue_transmit_buff_[priority];
		tls_buff_queue_release(queue, buff);
	}
	else if (TLS_BUFF_TYPE_RX == buff->type_)
	{
		tls_buff_queue_release(&server->queue_recv_buff_, buff);
	}
}
tls_buff_t*	tls_server_acquire_transmit_buff(tls_server_t* server, U8 priority)
{
	tls_buff_t*			buff = NULL;
	tls_buff_queue_t*	queue = NULL;

	if (NULL == server || priority > TLS_QUEUE_PRI_7)
		return NULL;

	queue = &server->queue_transmit_buff_[priority];
	buff = tls_buff_queue_acquire(queue);
	if (NULL == buff)
	{
		buff = tls_buff_create(TLS_BUFF_SIZE, TLS_BUFF_TYPE_TX);
		if (buff)
			buff->priority_ = priority;
	}
	return buff;
}

TLS_RESULT	tls_server_register(tls_server_t* server, U32 id, U16 subid)
{
	TLS_RESULT						result;
	tls_buff_t*						buff;
	tls_transmit_t*					transmit;

	PU8								data_ptr;
	tls_message_head_t*				msg_hdr;
	tls_register_request_t*			reg_request;
	U8								priority =  TLS_QUEUE_PRI_7;

	do
	{
		if (NULL == server || !server->start_)
		{
			result = TLS_RESULT_E_INVALID_DATA;
			break;
		}
		transmit = (tls_transmit_t*)server->tls_transmit_;

		buff = tls_server_acquire_transmit_buff(server, priority);
		if (NULL == buff)
		{
			LOG_ERROR("FATAL ERROR!!-Allocate buff fail");
			result = TLS_RESULT_E_ALLOCATE;
			break;
		}

		LOG_DEBUG("buff priority:%d.", buff->priority_);

		data_ptr = buff->buf_;
		memset(data_ptr, 0, buff->buf_size_);
		msg_hdr = (tls_message_head_t*)data_ptr;
		msg_hdr->version = TLS_MSG_MAIN_VERSION;
		msg_hdr->message_type = MSG_TYPE_REGISTER_REQUEST;
		msg_hdr->message_seq = htons(server->sequence_++);
	
		reg_request = (tls_register_request_t*)(data_ptr + sizeof(tls_message_head_t));
		reg_request->src_addr = htonl(id);
		reg_request->src_entry = htons(subid);
		reg_request->src_ip = inet_addr(server->svr_param_.local_ip_);
		buff->size_ = sizeof(tls_message_head_t) + sizeof(tls_register_request_t);

		result = tls_transmit_push(transmit, buff);
	
	}while (0);
	
	return result;
}
TLS_RESULT	tls_server_transmit(tls_server_t* server, const tls_session_t* session, PU8 data, U32 size)
{
	TLS_RESULT					result = TLS_RESULT_S_OK;
	tls_buff_t*					buff;
	tls_transmit_t*				transmit;
	tls_param_t*				svr_param;
	tls_statis_t*				tls_statis;

	tls_message_head_t*			msg_hdr = NULL;
	tls_message_content_t*		msg = NULL;
	PU8							data_ptr = NULL;

	do
	{	
		if (NULL == server || NULL == session || NULL == data || 0 == size)
		{
			LOG_ERROR("ERROR-Invalid parameters.");
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}
		if (!server->start_)
		{
			LOG_ERROR("ERROR-The server is not start.");
			result = TLS_RESULT_E_INVALID_DATA;
			break;
		}
		if (size > 1024)
		{
			LOG_ERROR("The data size is over 1024.");
			result = TLS_RESULT_E_INVALID_DATA;
			break;
		}

		buff = tls_server_acquire_transmit_buff(server, session->tls_priority_);
		if (NULL == buff)
		{
			LOG_ERROR("FATAL ERROR!!-Allocate buff fail, priority:%d.", session->tls_priority_);
			result = TLS_RESULT_E_ALLOCATE;
			break;
		}

		svr_param = &server->svr_param_;
		transmit = (tls_transmit_t*)server->tls_transmit_;
		tls_statis = (tls_statis_t*)server->tls_statis_;

		data_ptr = buff->buf_;
		msg_hdr = (tls_message_head_t*)data_ptr;
		msg_hdr->version = TLS_MSG_MAIN_VERSION;
		msg_hdr->message_type = MSG_TYPE_SEND_MESSAGE;
		msg_hdr->message_seq = htons(server->sequence_++);
	
		msg = (tls_message_content_t*)(data_ptr + sizeof(tls_message_head_t));
		strcpy(msg->id, session->tls_session_id_);
		msg->trans_type = MSG_TRANSFER_TYPE_P2P;
	
		msg->src_addr = htonl(svr_param->tls_local_id_);
		msg->src_entry = htons(svr_param->tls_local_sub_id_);
		msg->dst_count = htons(1);
		msg->dst_addr = htonl(session->tls_remote_id_);
		msg->dst_entry = htons(session->tls_remote_sub_id_);

		msg->msg_type = htonl(0);
		msg->reliable = MSG_NO_RELIABLE;
		msg->priority = MSG_PRIORITY_LOW;
		msg->timeout = htonl(0);
	
		msg->size = htons(size);
		memcpy(msg->payload, data, size);	
		buff->size_ = sizeof(tls_message_head_t) + sizeof(tls_message_content_t) + size;

		result = tls_transmit_push(transmit, buff);
		if (TlsResultOk(result))
		{
			tls_statis_app_tx(tls_statis, size);
		}
	}while (0);
	
	return result;
}
TLS_RESULT tls_server_parsedata(tls_server_t* server, tls_buff_t* buff)
{
	tls_param_t*				svr_param = NULL;
	tls_statis_t*				tls_statis;
	tls_message_head_t*			msg_hdr = NULL;
	tls_register_response_t*	reg_response = NULL;
	tls_message_content_t*		msg = NULL;

	PU8							data;
	PU8							payload = NULL;
	U16							payload_size = 0;
	// U32							size;

	if (NULL == server || NULL == buff)
		return TLS_RESULT_E_INVALID_PARAM;
	if (NULL == buff->buf_ || 0 == buff->size_)
		return TLS_RESULT_E_INVALID_DATA;

	svr_param = &server->svr_param_;
	tls_statis = server->tls_statis_;
	data = buff->buf_;
	// size = buff->size_;
	msg_hdr = (tls_message_head_t*)data;
	if (MSG_TYPE_REGISTER_RESPONSE == msg_hdr->message_type)
	{
		LOG_DEBUG("Receive register response message, seq:%d.", msg_hdr->message_seq);	
		reg_response = (tls_register_response_t*)(data + sizeof(tls_message_head_t));
		if (TLS_REGISTER_OK == reg_response->result)
		{
			if(svr_param->on_tls_register_callback)
			{
				svr_param->on_tls_register_callback(svr_param->priv_data_, TLS_REGISTER_OK);
			}
			LOG_DEBUG("Register response result OK.");
		}
		else
		{
			if(svr_param->on_tls_register_callback)
			{
				svr_param->on_tls_register_callback(svr_param->priv_data_, TLS_REGISTER_FAIL);
			}
			LOG_DEBUG("Register response result Fail.");
		}
	}
	else if (MSG_TYPE_RECV_MESSAGE == msg_hdr->message_type)
	{
		msg = (tls_message_content_t*)(data + sizeof(tls_message_head_t));
		payload_size = ntohs(msg->size);
		payload = msg->payload;
		if (svr_param->on_tls_receive_callback)
		{
			svr_param->on_tls_receive_callback(svr_param->priv_data_, payload, payload_size);
			tls_statis_app_rx(tls_statis, payload_size);
		}
	}

	return TLS_RESULT_S_OK;
}

TLS_RESULT tls_server_vsoa_publish(tls_server_t* server, const vsoa_url_t* url, const vsoa_payload_t* payload, U8 priority)
{
#if 0
	tls_vsoa_publish_t* pub = (tls_vsoa_publish_t*)server->tls_vsoa_publish_;
	vsoa_publish_data(pub, url, payload);
	return TLS_RESULT_S_OK;
#endif

	TLS_RESULT					result = TLS_RESULT_S_OK;
	tls_buff_t*					buff;
	tls_transmit_t*				transmit;
	tls_statis_t*				tls_statis;
	tls_vsoa_hdr_t*				tls_vsoa_hdr;
	PU8							data_ptr = NULL;

	do
	{
		if (NULL == server || NULL == url || NULL == payload)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			LOG_ERROR("ERROR-Invalid parameters.");
			break;
		}
		if (0 == url->url_len || (0 == payload->param_len && 0 == payload->data_len))
		{
			result = TLS_RESULT_E_INVALID_DATA;
			LOG_ERROR("ERROR-Invalid url or paylaod data, url:%s, url len:%ld.", url->url, url->url_len);
			LOG_ERROR("ERROR-payload param:%s, param len:%ld, data len:%ld.", payload->param, payload->param_len, 
					payload->data_len);
			break;
		}
		if (!server->start_)
		{
			result = TLS_RESULT_E_INVALID_DATA;
			LOG_ERROR("ERROR-The server is not start.");
			break;
		}
		
		buff = tls_server_acquire_transmit_buff(server, priority);
		if (NULL == buff)
		{
			LOG_FATAL("FATAL ERROR!!-Allocate buff fail, priority:%d.", priority);
			result = TLS_RESULT_E_ALLOCATE;
			break;
		}
		transmit = (tls_transmit_t*)server->tls_transmit_;
		tls_statis = (tls_statis_t*)server->tls_statis_;

		data_ptr = buff->buf_;

		tls_vsoa_hdr = (tls_vsoa_hdr_t*)data_ptr;
		tls_vsoa_hdr->vsoa_url = NULL;
		tls_vsoa_hdr->vsoa_url_size = url->url_len;
		tls_vsoa_hdr->vsoa_param_size = payload->param_len;
		tls_vsoa_hdr->vsoa_data_size = payload->data_len;

		tls_vsoa_hdr->vsoa_url = (char*)tls_vsoa_hdr->tls_vsoa_data;
		memcpy(tls_vsoa_hdr->vsoa_url, url->url, url->url_len);	

		tls_vsoa_hdr->vsoa_param = tls_vsoa_hdr->vsoa_url + tls_vsoa_hdr->vsoa_url_size;
		if (payload->param_len > 0)
		{
			memcpy(tls_vsoa_hdr->vsoa_param, payload->param, payload->param_len);
		}

		tls_vsoa_hdr->vsoa_data = (U8*)tls_vsoa_hdr->vsoa_param + tls_vsoa_hdr->vsoa_param_size;
		if (payload->data_len > 0)
		{
			memcpy(tls_vsoa_hdr->vsoa_data, payload->data, payload->data_len);
		}

		buff->size_ = sizeof(tls_vsoa_hdr_t) + tls_vsoa_hdr->vsoa_url_size + tls_vsoa_hdr->vsoa_param_size 
			+ tls_vsoa_hdr->vsoa_data_size;
		buff->transmode_ = TLS_VSOA;

		result = tls_transmit_push(transmit, buff);
		if (TlsResultOk(result))
		{
			tls_statis_app_tx(tls_statis, buff->size_);
		}

	}while (0);
	
	return result;
}

TLS_RESULT tls_server_vsoa_publish_overtls(tls_server_t* server, const vsoa_url_t* url, const vsoa_payload_t* payload, U8 priority)
{
	TLS_RESULT					result = TLS_RESULT_S_OK;
	tls_buff_t*					buff;
	tls_transmit_t*				transmit;
	tls_param_t*				svr_param;
	tls_statis_t*				tls_statis;

	tls_message_head_t*			msg_hdr = NULL;
	tls_message_content_t*		msg_content = NULL;
	U32							msg_size = 0;

	PU8							data_ptr = NULL;
	U32							payload_size;
	tls_vsoa_hdr_t*				tls_vsoa_hdr;

	do
	{	
		if (NULL == server || NULL == url || NULL == payload)
		{
			LOG_ERROR("ERROR-Invalid parameters.");
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}
		if (!server->start_)
		{
			LOG_ERROR("ERROR-The server is not start.");
			result = TLS_RESULT_E_INVALID_DATA;
			break;
		}
		payload_size = url->url_len + payload->param_len + payload->data_len + sizeof(tls_vsoa_hdr_t);
		if (payload_size > 1024)
		{
			LOG_ERROR("The data size is over 1024.");
			result = TLS_RESULT_E_INVALID_DATA;
			break;
		}

		buff = tls_server_acquire_transmit_buff(server, priority);
		if (NULL == buff)
		{
			LOG_FATAL("FATAL ERROR!!-Allocate buff fail, priority:%d.", priority);
			result = TLS_RESULT_E_ALLOCATE;
			break;
		}

		svr_param = &server->svr_param_;
		transmit = (tls_transmit_t*)server->tls_transmit_;
		tls_statis = (tls_statis_t*)server->tls_statis_;

		data_ptr = buff->buf_;
		msg_hdr = (tls_message_head_t*)data_ptr;
		msg_hdr->version = TLS_MSG_MAIN_VERSION;
		msg_hdr->message_type = MSG_TYPE_SEND_MESSAGE;
		msg_hdr->message_seq = htons(server->sequence_++);
	
		msg_content = (tls_message_content_t*)(data_ptr + sizeof(tls_message_head_t));
		strcpy(msg_content->id, "session");
		msg_content->trans_type = MSG_TRANSFER_TYPE_P2P;
	
		msg_content->src_addr = htonl(svr_param->tls_local_id_);
		msg_content->src_entry = htons(svr_param->tls_local_sub_id_);
		msg_content->dst_count = htons(1);
		msg_content->dst_addr = htonl(89);
		msg_content->dst_entry = htons(3001);

		msg_content->msg_type = htonl(0);
		msg_content->reliable = MSG_NO_RELIABLE;
		msg_content->priority = MSG_PRIORITY_LOW;
		msg_content->timeout = htonl(0);

#if 0
		msg_content->size = htons(size);
		memcpy(msg_content->payload, data, size);	
		buff->size_ = sizeof(tls_message_head_t) + sizeof(tls_message_content_t) + size;
#endif
		data_ptr = msg_content->payload;

		tls_vsoa_hdr = (tls_vsoa_hdr_t*)data_ptr;
		tls_vsoa_hdr->vsoa_url = NULL;
		tls_vsoa_hdr->vsoa_url_size = url->url_len;
		tls_vsoa_hdr->vsoa_param_size = payload->param_len;
		tls_vsoa_hdr->vsoa_data_size = payload->data_len;

		tls_vsoa_hdr->vsoa_url = (char*)tls_vsoa_hdr->tls_vsoa_data;
		memcpy(tls_vsoa_hdr->vsoa_url, url->url, url->url_len);	

		tls_vsoa_hdr->vsoa_param = tls_vsoa_hdr->vsoa_url + tls_vsoa_hdr->vsoa_url_size;
		if (payload->param_len > 0)
		{
			memcpy(tls_vsoa_hdr->vsoa_param, payload->param, payload->param_len);
		}

		tls_vsoa_hdr->vsoa_data = (U8*)tls_vsoa_hdr->vsoa_param + tls_vsoa_hdr->vsoa_param_size;
		if (payload->data_len > 0)
		{
			memcpy(tls_vsoa_hdr->vsoa_data, payload->data, payload->data_len);
		}
		msg_content->size = htons(payload_size);
		msg_size = sizeof(msg_hdr) + sizeof(msg_content);

		buff->size_ = msg_size + payload_size;
		buff->transmode_ = TLS_VSOA;

		result = tls_transmit_push(transmit, buff);
#if 0
		if (TlsResultOk(result))
		{
			tls_statis_app_tx(tls_statis, buff->size_);
		}
#endif
	}while (0);
	
	return result;
}
TLS_RESULT tls_server_vsoa_publish_transmit(tls_server_t* server, const tls_vsoa_sesstion_t* session)
{
	TLS_RESULT					result = TLS_RESULT_S_OK;
	tls_buff_t*					buff;
	tls_transmit_t*				transmit;
	tls_param_t*				svr_param;
	tls_statis_t*				tls_statis;

	tls_message_head_t*			msg_hdr = NULL;
	tls_message_content_t*		msg_content = NULL;
	U32							msg_size = 0;

	PU8							data_ptr = NULL;
	U32							payload_size;
	tls_vsoa_hdr_t*				tls_vsoa_hdr;

	vsoa_url_t*					url;
	vsoa_payload_t*				payload;
	U8							priority;

	do
	{	
		if (NULL == server || NULL == session || NULL == session->vsoa_url_ || NULL == session->vsoa_payload_)
		{
			LOG_ERROR("ERROR-Invalid parameters.");
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}
		if (!server->start_)
		{
			LOG_ERROR("ERROR-The server is not start.");
			result = TLS_RESULT_E_INVALID_DATA;
			break;
		}

		url = session->vsoa_url_;
		payload = session->vsoa_payload_;
		priority = session->tls_priority_;
		payload_size = url->url_len + payload->param_len + payload->data_len + sizeof(tls_vsoa_hdr_t);
		LOG_DEBUG("url len:%d param:len:%d data len:%d, payload_size:%d", 
				url->url_len, payload->param_len, payload->data_len, payload_size);
#if 0
		if (payload_size > 1024)
		{
			result = TLS_RESULT_E_INVALID_DATA;
			break;
		}
#endif

		buff = tls_server_acquire_transmit_buff(server, priority);
		if (NULL == buff)
		{
			LOG_FATAL("FATAL ERROR!!-Allocate buff fail, priority:%d.", priority);
			result = TLS_RESULT_E_ALLOCATE;
			break;
		}

		svr_param = &server->svr_param_;
		transmit = (tls_transmit_t*)server->tls_transmit_;
		tls_statis = (tls_statis_t*)server->tls_statis_;

		data_ptr = buff->buf_;
		msg_hdr = (tls_message_head_t*)data_ptr;
		msg_hdr->version = TLS_MSG_MAIN_VERSION;
		msg_hdr->message_type = MSG_TYPE_SEND_MESSAGE;
		msg_hdr->message_seq = htons(server->sequence_++);
	
		msg_content = (tls_message_content_t*)(data_ptr + sizeof(tls_message_head_t));
		strcpy(msg_content->id, "session");
		msg_content->trans_type = MSG_TRANSFER_TYPE_P2P;
	
		msg_content->src_addr = htonl(svr_param->tls_local_id_);
		msg_content->src_entry = htons(svr_param->tls_local_sub_id_);

		msg_content->dst_count = htons(1);
		msg_content->dst_addr = htonl(session->tls_dest_addr_);
		msg_content->dst_entry = htons(session->tls_dest_entry_);

		LOG_DEBUG("msg src addr:%d, src entry:%d, dst addr:%d dst entry:%d", 
				svr_param->tls_local_id_, svr_param->tls_local_sub_id_,
				session->tls_dest_addr_, session->tls_dest_entry_);

		msg_content->msg_type = htonl(0);
		msg_content->reliable = MSG_NO_RELIABLE;
		msg_content->priority = MSG_PRIORITY_LOW;
		msg_content->timeout = htonl(0);

		data_ptr = msg_content->payload;

		tls_vsoa_hdr = (tls_vsoa_hdr_t*)data_ptr;
		tls_vsoa_hdr->vsoa_url_size = url->url_len;
		tls_vsoa_hdr->vsoa_param_size = payload->param_len;
		tls_vsoa_hdr->vsoa_data_size = payload->data_len;

		tls_vsoa_hdr->vsoa_url = (char*)tls_vsoa_hdr->tls_vsoa_data;
		memcpy(tls_vsoa_hdr->vsoa_url, url->url, url->url_len);	

		tls_vsoa_hdr->vsoa_param = tls_vsoa_hdr->vsoa_url + tls_vsoa_hdr->vsoa_url_size;
		if (payload->param_len > 0)
		{
			memcpy(tls_vsoa_hdr->vsoa_param, payload->param, payload->param_len);
		}

		tls_vsoa_hdr->vsoa_data = (U8*)tls_vsoa_hdr->vsoa_param + tls_vsoa_hdr->vsoa_param_size;
		if (payload->data_len > 0)
		{
			memcpy(tls_vsoa_hdr->vsoa_data, payload->data, payload->data_len);
		}
		msg_content->size = htons(payload_size);
		// msg_size = sizeof(msg_hdr) + sizeof(msg_content);
		msg_size = sizeof(tls_message_head_t) + sizeof(tls_message_content_t);

		buff->size_ = msg_size + payload_size;
		buff->transmode_ = TLS_VSOA;


		result = tls_transmit_push(transmit, buff);
	}while (0);
	
	return result;
}


TLS_RESULT tls_server_vsoa_publish_transmit_for_722(tls_server_t* server, const tls_vsoa_sesstion_t* session)
{
	TLS_RESULT					result = TLS_RESULT_S_OK;
	tls_buff_t*					buff;
	tls_transmit_t*				transmit;
	tls_param_t*				svr_param;
	tls_statis_t*				tls_statis;

	tls_message_head_t*			msg_hdr = NULL;
	tls_message_content_t*		msg_content = NULL;
	U32							msg_size = 0;

	PU8							data_ptr = NULL;
	U32							payload_size;
	tls_vsoa_hdr_t*				tls_vsoa_hdr;

	vsoa_url_t*					url;
	vsoa_payload_t*				payload;
	U8							priority;

	do
	{	
		if (NULL == server || NULL == session || NULL == session->vsoa_url_ || NULL == session->vsoa_payload_)
		{
			LOG_ERROR("ERROR-Invalid parameters.");
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}
		if (!server->start_)
		{
			LOG_ERROR("ERROR-The server is not start.");
			result = TLS_RESULT_E_INVALID_DATA;
			break;
		}

		url = session->vsoa_url_;
		payload = session->vsoa_payload_;
		priority = session->tls_priority_;
		payload_size = url->url_len + payload->param_len + payload->data_len + sizeof(tls_vsoa_hdr_t);
		LOG_DEBUG("url len:%d param:len:%d data len:%d, payload_size:%d.", 
				url->url_len, payload->param_len, payload->data_len, payload_size);

		buff = tls_server_acquire_transmit_buff(server, priority);
		if (NULL == buff)
		{
			LOG_FATAL("FATAL ERROR!!-Allocate buff fail, priority:%d.", priority);
			result = TLS_RESULT_E_ALLOCATE;
			break;
		}

		svr_param = &server->svr_param_;
		transmit = (tls_transmit_t*)server->tls_transmit_;
		tls_statis = (tls_statis_t*)server->tls_statis_;

		data_ptr = buff->buf_;

		tls_vsoa_hdr = (tls_vsoa_hdr_t*)data_ptr;
		tls_vsoa_hdr->vsoa_url_size = url->url_len;
		tls_vsoa_hdr->vsoa_param_size = payload->param_len;
		tls_vsoa_hdr->vsoa_data_size = payload->data_len;


		tls_vsoa_hdr->vsoa_url = (char*)tls_vsoa_hdr->tls_vsoa_data;
		memcpy(tls_vsoa_hdr->vsoa_url, url->url, url->url_len);	

		tls_vsoa_hdr->vsoa_param = tls_vsoa_hdr->vsoa_url + tls_vsoa_hdr->vsoa_url_size;
		if (payload->param_len > 0)
		{
			memcpy(tls_vsoa_hdr->vsoa_param, payload->param, payload->param_len);
		}

		tls_vsoa_hdr->vsoa_data = (U8*)tls_vsoa_hdr->vsoa_param + tls_vsoa_hdr->vsoa_param_size;
		if (payload->data_len > 0)
		{
			memcpy(tls_vsoa_hdr->vsoa_data, payload->data, payload->data_len);
		}

		buff->size_ = payload_size;
		buff->transmode_ = TLS_VSOA;


		result = tls_transmit_push(transmit, buff);
	}while (0);
	
	return result;
}
