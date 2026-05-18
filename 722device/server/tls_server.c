#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "tls_afx.h"
#include "tls_buff_queue.h"
#include "tls_transmit.h"
#include "tls_receive.h"
#include "tls_statis.h"
#include "tls_vsoa_subscribe.h"
#include "tls_vsoa_publish.h"
#include "tls_vsoa.h"
#include "tls_log.h"
#include "tls_server.h"
#include "tls_work.h"


static U32 s_tx_queue_count[TLS_QUEUE_MAX_COUNT] = 
{
	QUEUE_PRI_0_COUNT,
	QUEUE_PRI_1_COUNT,
	QUEUE_PRI_2_COUNT,
	QUEUE_PRI_3_COUNT,
	QUEUE_PRI_4_COUNT,
	QUEUE_PRI_5_COUNT,
	QUEUE_PRI_6_COUNT,
	QUEUE_PRI_7_COUNT,
};


static TLS_RESULT		tls_server_alloc_send_buff_queue(tls_server_t* s);
static void				tls_server_free_send_buff_queue(tls_server_t* s);

static TLS_RESULT		tls_server_alloc_transmit_buff_queue(tls_server_t* s);
static void				tls_server_free_transmit_buff_queue(tls_server_t* s);

static TLS_RESULT		tls_server_alloc_recv_buff_queue(tls_server_t* s);
static void				tls_server_free_recv_buff_queue(tls_server_t* s);

static TLS_RESULT		tls_server_open_udp(tls_server_t* s);
static void				tls_server_close_udp(tls_server_t* s);

static TLS_RESULT		tls_server_init_transmit(tls_server_t* s);
static TLS_RESULT		tls_server_uninit_transmit(tls_server_t* s);

static TLS_RESULT		tls_server_init_receive(tls_server_t* s);
static TLS_RESULT		tls_server_uninit_receive(tls_server_t* s);

static TLS_RESULT		tls_server_start_work_thread(tls_server_t* s);
static void				tls_server_stop_work_thread(tls_server_t* s);

static void*			tls_server_thread_work(void* arg);
static inline void		tls_server_work_poll(tls_server_t* s);

static U32				tls_server_transmit_queue_count(U8 priority);

static TLS_RESULT		tls_server_init_tls_statis(tls_server_t* s);
static void				tls_server_uninit_tls_statis(tls_server_t* s);

static TLS_RESULT		tls_server_init_vsoa_subscribe(tls_server_t* s);
static void				tls_server_uninit_vsoa_subscribe(tls_server_t* s);

static TLS_RESULT		tls_server_init_vsoa_publish(tls_server_t* s);
static void				tls_server_uninit_vsoa_publish(tls_server_t* s);


tls_server_t*	tls_server_allocate(void)
{
	tls_server_t*		s = NULL;
	s = (tls_server_t*)malloc(sizeof(tls_server_t));
	return s;
}	

void tls_server_free(tls_server_t* server)
{
	if (server)
		free(server);
}

TLS_RESULT tls_server_initialize(tls_server_t* server, const tls_param_t* param)
{
	TLS_RESULT			result = TLS_RESULT_E_FAIL;
	int					oldvalue;
	int					newvalue;

	do
	{
		if (NULL == server || NULL == param)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}	

		oldvalue = 0;
		newvalue = 1;
		if (__sync_val_compare_and_swap(&server->init_, oldvalue, newvalue))
		{
			LOG_INFO("Server has already initialize.");
			result = TLS_RESULT_S_CONTINUE;
			break;
		}
		if (0 == strlen(param->local_ip_) || 0 == param->local_port_ 
			|| 0 == strlen(param->remote_ip_) || 0 == param->remote_port_)
		{
			LOG_ERROR("ERROR-Invalid parameters.");
			result = TLS_RESULT_E_INVALID_DATA;
			break;
		}

		memcpy(&server->svr_param_, param, sizeof(server->svr_param_));
		memset(&server->evt_work_loop_, 0, sizeof(server->evt_work_loop_));
		memset(&server->evt_work_stop_, 0, sizeof(server->evt_work_stop_));
		tls_event_initialize(&server->evt_work_loop_);
		tls_event_initialize(&server->evt_work_stop_);

		server->send_buff_count_ = TLS_TRANSMIT_BUFF_COUNT;
		server->recv_buff_count_ = TLS_RECEIVE_BUFF_COUNT;
		server->buff_size_ = TLS_BUFF_SIZE;

		result = tls_server_alloc_send_buff_queue(server);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-allocate send buff queue fail");
			break;
		}
		result = tls_server_alloc_transmit_buff_queue(server);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-allocate transmit buff queue fail");
			break;
		}
		result = tls_server_alloc_recv_buff_queue(server);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-allocate recv buff queue fail");
			break;
		}

		result = tls_server_init_transmit(server);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-initialize transmit fail.");
			break;
		}
		result = tls_server_init_receive(server);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-initialize receive fail.");
			break;
		}
		result = tls_server_init_vsoa_subscribe(server);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-VSOA subscribe init fail.");
			break;
		}
		result = tls_server_init_vsoa_publish(server);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-VSOA publish init fail.");
			break;
		}

		LOG_INFO("Initialize TLS server ok.");
	}while(0);
	if (TlsResultFail(result))
	{
		tls_server_uninitialize(server);
	}
	return result;
}

TLS_RESULT tls_server_start(tls_server_t* server)
{
	TLS_RESULT			result = TLS_RESULT_E_FAIL;
	int					oldvalue;
	int					newvalue;

	do
	{
		if (NULL == server)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}
		oldvalue = 0;
		newvalue = 1;
		if (__sync_val_compare_and_swap(&server->start_, oldvalue, newvalue))
		{
			LOG_INFO("Server has already start.");
			result = TLS_RESULT_S_CONTINUE;
			break;
		}
		result = tls_server_open_udp(server);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-open udp socket fail.");
			break;
		}
		result = vsoa_publish_start(server->tls_vsoa_publish_);
		if (TlsResultFail(result))	
		{
			LOG_ERROR("Start vsoa publish fail.");
			break;
		}
		result = vsoa_subscribe_start(server->tls_vsoa_subscribe_);
		if (TlsResultFail(result))	
		{
			LOG_ERROR("Start vsoa subscribe fail.");
			break;
		}
		result = tls_transmit_start(server, server->tls_transmit_);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-Start transmit module fail.");
			break;
		}
		result = tls_receive_start(server, server->tls_receive_);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-Start receive module fail.");
			break;
		}
		result = tls_server_start_work_thread(server);
		if (TlsResultFail(result))
		{
			LOG_ERROR("Start server work thread fail.");
			break;
		}
	}while(0);
	if (TlsResultFail(result))
	{
		tls_server_stop(server);	
	}
	return result;
}

TLS_RESULT tls_server_stop(tls_server_t* server)
{
	int					oldvalue;
	int					newvalue;
	
	if (NULL == server)
		return TLS_RESULT_E_INVALID_PARAM;

	oldvalue = 1;
	newvalue = 0;
	if (!__sync_val_compare_and_swap(&server->start_, oldvalue, newvalue))
	{
		LOG_INFO("Server has already stop.");
		return TLS_RESULT_S_CONTINUE;
	}

	tls_server_stop_work_thread(server);
	tls_transmit_stop(server->tls_transmit_);
	tls_udp_socket_close(server->udp_socket_);
	tls_receive_stop(server->tls_receive_);
	tls_server_close_udp(server);
	vsoa_subscribe_stop(server->tls_vsoa_subscribe_);
	vsoa_publish_stop(server->tls_vsoa_publish_);
	return TLS_RESULT_S_OK;
}

TLS_RESULT tls_server_uninitialize(tls_server_t* server)
{
	TLS_RESULT			result = TLS_RESULT_E_FAIL;
	int					oldvalue;
	int					newvalue;

	if (NULL == server)
		return TLS_RESULT_E_INVALID_PARAM;

	oldvalue = 1;
	newvalue = 0;
	if (!__sync_val_compare_and_swap(&server->init_, oldvalue, newvalue))
	{
		LOG_INFO("Server has already Uninitialize.");
		result = TLS_RESULT_S_CONTINUE;
		return result;
	}

	tls_server_stop(server);

	tls_server_uninit_receive(server);
	tls_server_uninit_transmit(server);

	tls_server_free_recv_buff_queue(server);
	tls_server_free_send_buff_queue(server);
	tls_server_free_transmit_buff_queue(server);

	tls_event_uninitialize(&server->evt_work_loop_);
	tls_event_uninitialize(&server->evt_work_stop_);

	tls_server_uninit_vsoa_publish(server);
	tls_server_uninit_vsoa_subscribe(server);

	return result;
}



TLS_RESULT tls_server_alloc_send_buff_queue(tls_server_t* s)
{
	tls_buff_queue_t*	queue  = NULL;
	assert(s);
	queue = &s->queue_send_buff_;
	memset(queue, 0, sizeof(*queue));
	tls_buff_queue_initialize(queue);
	return tls_buff_queue_allocate_t(queue, TLS_BUFF_SIZE, TLS_TRANSMIT_BUFF_COUNT, TLS_BUFF_TYPE_TX);
}
void tls_server_free_send_buff_queue(tls_server_t* s)
{
	tls_buff_queue_t*		queue;
	assert(s);
	queue = &s->queue_send_buff_;
	tls_buff_queue_uninitialize(queue);
	memset(queue, 0, sizeof(s->queue_send_buff_));
}
TLS_RESULT tls_server_alloc_transmit_buff_queue(tls_server_t* s)
{
	TLS_RESULT			result = TLS_RESULT_E_FAIL;
	U32					index;
	tls_buff_queue_t*	queue  = NULL;
	U32					queue_count = 0;
	U32					buff_size;
	U32					buff_count;
	tls_param_t*		tls_param = &s->svr_param_;

	assert(s);

	for (index = TLS_QUEUE_PRI_0; index < TLS_QUEUE_MAX_COUNT; index++)
	{
		buff_size = tls_param->queue_param_[index].buff_size_;
		buff_count = tls_param->queue_param_[index].buff_count_;
		buff_size = buff_size > 0 ? buff_size : TLS_BUFF_SIZE;
		buff_count = buff_count > 0 ? buff_count : tls_server_transmit_queue_count(index);

		LOG_DEBUG("index:%d, buff size:%d, buff count:%d.", index, buff_size, buff_count);

		queue = &(s->queue_transmit_buff_[index]);
		memset(queue, 0, sizeof(*queue));
		tls_buff_queue_initialize(queue);
		queue->priority_ = index;
		result = tls_buff_queue_allocate_t(queue, buff_size, buff_count, TLS_BUFF_TYPE_TX);
		if (TlsResultFail(result))
		{
			LOG_ERROR("tls buff queue allocate fail, priority:%d, buff size:%d, buff count:%d.", 
					index, buff_size, buff_count);
			break;
		}
		LOG_DEBUG("allocate tls queue, priority:%d, buff size:%d, buff count:%d", queue->priority_, buff_size, buff_count);
	}
	return result;
}
void tls_server_free_transmit_buff_queue(tls_server_t* s)
{
	U32						index;
	tls_buff_queue_t*		queue;
	assert(s);
#if 0
	queue = &s->queue_send_buff_;
	tls_buff_queue_uninitialize(queue);
	memset(queue, 0, sizeof(s->queue_send_buff_));
#endif
	for (index = TLS_QUEUE_PRI_0; index < TLS_QUEUE_MAX_COUNT; index++)
	{
		queue = &(s->queue_transmit_buff_[index]);
		tls_buff_queue_uninitialize(queue);
		memset(queue, 0, sizeof(s->queue_transmit_buff_[index]));
	}
}

TLS_RESULT tls_server_alloc_recv_buff_queue(tls_server_t* s)
{
	tls_buff_queue_t*	queue  = NULL;
	assert(s);
	queue = &s->queue_recv_buff_;
	memset(queue, 0, sizeof(*queue));
	tls_buff_queue_initialize(queue);
	return tls_buff_queue_allocate_t(queue, TLS_BUFF_SIZE, TLS_RECEIVE_BUFF_COUNT, TLS_BUFF_TYPE_RX);
}
void tls_server_free_recv_buff_queue(tls_server_t* s)
{
	tls_buff_queue_t*		queue;
	assert(s);
	queue = &s->queue_recv_buff_;
	tls_buff_queue_uninitialize(queue);
	memset(queue, 0, sizeof(s->queue_recv_buff_));
}
TLS_RESULT tls_server_open_udp(tls_server_t* s)
{
	tls_udp_socket_t*		udp_socket = NULL;
	assert(s);
	s->udp_socket_ = NULL;

	udp_socket = tls_udp_socket_allocate();
	if (NULL == udp_socket)
		return TLS_RESULT_E_ALLOCATE;
	s->udp_socket_ = udp_socket;

	strcpy(udp_socket->sip_, s->svr_param_.local_ip_);
	udp_socket->sport_ = s->svr_param_.local_port_;
	strcpy(udp_socket->dip_, s->svr_param_.remote_ip_);
	udp_socket->dport_ = s->svr_param_.remote_port_;

	LOG_DEBUG("----------------- tls udp config -----------------");
	LOG_DEBUG("udp local ip: %s", udp_socket->sip_);
	LOG_DEBUG("udp local port: %d", udp_socket->sport_);
	LOG_DEBUG("udp remote ip: %s", udp_socket->dip_);
	LOG_DEBUG("udp remote port: %d", udp_socket->dport_);
	LOG_DEBUG("----------------------------------------------------");

	return tls_udp_socket_open(udp_socket);
}
void tls_server_close_udp(tls_server_t* s)
{
	tls_udp_socket_t*		udp_socket = NULL;
	assert(s);
	udp_socket = s->udp_socket_;
	tls_udp_socket_close(udp_socket);
	tls_udp_socket_free(udp_socket);
	s->udp_socket_ = NULL;
}
TLS_RESULT tls_server_init_transmit(tls_server_t* s)
{
	tls_transmit_t*			transmit = NULL;
	assert(s);

	transmit = tls_transmit_allocate();
	if  (NULL == transmit)
		return TLS_RESULT_E_ALLOCATE;

	memset(transmit, 0, sizeof(*transmit));
	s->tls_transmit_ = (void*)transmit;
	return TLS_RESULT_S_OK;
}
TLS_RESULT tls_server_uninit_transmit(tls_server_t* s)
{
	tls_transmit_t*			transmit = NULL;
	assert(s);

	transmit = (tls_transmit_t*)s->tls_transmit_;
	tls_transmit_stop(transmit);
	tls_transmit_release_buff(transmit);
	tls_transmit_free(transmit);
	s->tls_transmit_ = NULL;

	return TLS_RESULT_S_OK;
}

TLS_RESULT tls_server_init_receive(tls_server_t* s)
{
	tls_receive_t*			receive = NULL;
	assert(s);

	receive = tls_receive_allocate();
	if  (NULL == receive)
		return TLS_RESULT_E_ALLOCATE;

	memset(receive, 0, sizeof(*receive));
	s->tls_receive_ = (void*)receive;
	return TLS_RESULT_S_OK;
}
TLS_RESULT tls_server_uninit_receive(tls_server_t* s)
{
	tls_receive_t*			receive = NULL;
	assert(s);

	receive = (tls_receive_t*)s->tls_receive_;
	tls_receive_stop(receive);
	tls_receive_release_buff(receive);
	tls_receive_free(receive);
	s->tls_receive_ = NULL;

	return TLS_RESULT_S_OK;
}

TLS_RESULT	tls_server_start_work_thread(tls_server_t* s)
{
	assert(s);
	memset(&s->thread_work_, 0,  sizeof(s->thread_work_));
	s->thread_work_.run_ = 1;
	s->thread_work_.thread_routine_ = tls_server_thread_work;
	s->thread_work_.arg_ = (void*)s;
	s->work_run_ = 1;
	tls_event_set(&s->evt_work_loop_);
	tls_thread_start(&s->thread_work_);
	return TLS_RESULT_S_OK;
}
void tls_server_stop_work_thread(tls_server_t* s)
{
	s->work_run_ = 0;
	tls_event_set(&s->evt_work_loop_);
	tls_event_timewait(&s->evt_work_stop_, 1000);
	tls_thread_stop(&s->thread_work_);
}


void* tls_server_thread_work(void* arg)
{
	tls_server_t*			server = NULL;
	tls_event_t*			evt_loop;
	tls_event_t*			evt_stop;

	server = (tls_server_t*)arg;
	evt_loop = &server->evt_work_loop_;
	evt_stop = &server->evt_work_stop_;

	tls_event_reset(evt_stop);
	while (server->work_run_)
	{
		tls_event_wait(evt_loop);
		if (!server->work_run_)
			break;

		LOG_DEBUG("############### work ###############");	
		tls_server_work_poll(server);
		// sleep(1);
	}
	tls_event_set(evt_stop);
	return NULL;
}
inline void tls_server_work_poll(tls_server_t* s)
{
	tls_receive_t*			receive;
	tls_buff_t*				buff = NULL;

	receive = (tls_receive_t*)s->tls_receive_;
	buff = tls_receive_pop(receive);
	if (buff && buff->buf_ && buff->size_)
	{
		tls_server_parsedata(s, buff);
		tls_server_release_buff(s, buff);
	}
	else
	{
		tls_server_deactive(s);
	}
}

U32 tls_server_transmit_queue_count(U8 priority)
{
	if (priority > TLS_QUEUE_PRI_7)
		return 0;
	return s_tx_queue_count[priority];
}

TLS_RESULT tls_server_init_tls_statis(tls_server_t* s)
{
	TLS_RESULT			result = TLS_RESULT_E_FAIL;
	tls_statis_t*		tls_statis = NULL;
	do
	{
		if (NULL == s)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}
		tls_statis = tls_statis_allocate();	
		if (NULL == tls_statis)
		{
			result = TLS_RESULT_E_ALLOCATE;
			break;
		}
		memset(tls_statis, 0, sizeof(*tls_statis));
		s->tls_statis_ = tls_statis;
		result = TLS_RESULT_S_OK;
	}while(0);
	return result;
}
void tls_server_uninit_tls_statis(tls_server_t* s)
{
	tls_statis_t*		tls_statis;
	if (NULL == s || NULL == s->tls_statis_)
		return;
	tls_statis = s->tls_statis_;
	tls_statis_uninitialize(tls_statis);	
	tls_statis_free(tls_statis);
	s->tls_statis_ = NULL;
}

TLS_RESULT tls_server_init_vsoa_subscribe(tls_server_t* s)
{
	tls_vsoa_subscribe_t*	subscribe = NULL;
	assert(s);

	subscribe = vsoa_subscribe_allocate();
	if (NULL == subscribe)
		return TLS_RESULT_E_ALLOCATE;

	memset(subscribe, 0, sizeof(*subscribe));
	subscribe->tls_server_ = s;
	s->tls_vsoa_subscribe_ = (void*)subscribe;

	return TLS_RESULT_S_OK;
}
void tls_server_uninit_vsoa_subscribe(tls_server_t* s)
{
	tls_vsoa_subscribe_t*	subscribe = NULL;
	
	if (NULL == s || NULL == s->tls_vsoa_subscribe_)
		return;

	subscribe = s->tls_vsoa_subscribe_;
	vsoa_subscribe_stop(subscribe);
	vsoa_subscribe_free(subscribe);
	s->tls_vsoa_subscribe_ = NULL;
}
TLS_RESULT tls_server_init_vsoa_publish(tls_server_t* s)
{
	tls_vsoa_publish_t*	publish = NULL;
	assert(s);

	publish = vsoa_publish_allocate();
	if (NULL == publish)
		return TLS_RESULT_E_ALLOCATE;

	memset(publish, 0, sizeof(*publish));
	publish->tls_server_ = s;
	s->tls_vsoa_publish_ = (void*)publish;

	return TLS_RESULT_S_OK;
}
void tls_server_uninit_vsoa_publish(tls_server_t* s)
{
	tls_vsoa_publish_t*	publish = NULL;
	
	if (NULL == s || NULL == s->tls_vsoa_publish_)
		return;

	publish = s->tls_vsoa_publish_;
	vsoa_publish_stop(publish);
	vsoa_publish_free(publish);
	s->tls_vsoa_publish_ = NULL;
}
