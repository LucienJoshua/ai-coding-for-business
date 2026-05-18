#include <string.h>
#include <assert.h>
#include "tls_afx.h"
#include "tls_udp_socket.h"
#include "tls_statis.h"
#include "tls_server.h"
#include "tls_log.h"
#include "tls_receive.h"

#define __RX_QUEUE_LOCK()		tls_lock(&receive->queue_receive_lock_);
#define __RX_QUEUE_UNLOCK()		tls_unlock(&receive->queue_receive_lock_);

static void* tls_receive_thread_work(void*arg);

tls_receive_t* tls_receive_allocate(void)
{
	tls_receive_t*	receive = NULL;
	receive = (tls_receive_t*)malloc(sizeof(tls_receive_t));
	return receive;
}

void tls_receive_free(tls_receive_t* r)
{
	if (r)
	{
		tls_receive_stop(r);
		free(r);
	}
}

TLS_RESULT tls_receive_start(void* s, tls_receive_t* r)
{
	int				oldvalue = 0;
	int				newvalue = 1;

	if (NULL == s || NULL == r)
		return TLS_RESULT_E_INVALID_PARAM;

	oldvalue = 0;
	newvalue = 1;
	if (__sync_val_compare_and_swap(&r->start_, oldvalue, newvalue))
	{
		LOG_INFO("Receive mode  has already started.\n");
		return TLS_RESULT_S_CONTINUE;
	}

	r->tls_server_ = (tls_server_t*)s;
	r->udp_socket_ = ((tls_server_t*)s)->udp_socket_;

	tls_list_initialize(&r->queue_receive_);
	tls_lock_init(&r->queue_receive_lock_);

	tls_event_initialize(&r->evt_loop_);
	tls_event_initialize(&r->evt_stop_);

	memset(&r->thread_receive_, 0, sizeof(r->thread_receive_));
	r->thread_receive_.run_ = 1;
	r->thread_receive_.thread_routine_ = tls_receive_thread_work;
	r->thread_receive_.arg_ = r;
	r->run_ = 1;
	tls_event_set(&r->evt_loop_);
	tls_thread_start(&r->thread_receive_);

	return TLS_RESULT_S_OK;
}

TLS_RESULT tls_receive_stop(tls_receive_t* r)
{
	int				oldvalue = 1;
	int				newvalue = 0;

	if (NULL == r)
		return TLS_RESULT_E_INVALID_PARAM;

	oldvalue = 1;
	newvalue = 0;
	if (!__sync_val_compare_and_swap(&r->start_, oldvalue, newvalue))
	{
		LOG_INFO("Receive mode  has already stop.");
		return TLS_RESULT_S_CONTINUE;
	}

	r->run_ = 0;
	tls_event_set(&r->evt_loop_);
	tls_event_timewait(&r->evt_stop_, 2000);
	tls_thread_stop(&r->thread_receive_);

	tls_event_uninitialize(&r->evt_loop_);
	tls_event_uninitialize(&r->evt_stop_);
	return TLS_RESULT_S_OK;
}
void tls_receive_release_buff(tls_receive_t* r)
{
	tls_server_t*			server;
	tls_list_node_t*		head;
	tls_list_node_t*		node;
	tls_buff_t*				buff;

	if (NULL == r)
		return;

	server = (tls_server_t*)r->tls_server_;
	head = &r->queue_receive_;
	while (!tls_list_empty(head))
	{
		node = tls_list_remove_head(head);
		buff = __LIST_CONTAIN_OF(node, tls_buff_t, list_node_);
		if (buff)
		{
			tls_buff_queue_release(&server->queue_recv_buff_, buff);
		}
	}
}
void tls_receive_push(tls_receive_t* receive, tls_buff_t* buff)
{
	tls_server_t*		server;

	assert(receive);
	assert(buff);
	
	__RX_QUEUE_LOCK();
	tls_list_insert_tail(&receive->queue_receive_, &buff->list_node_);
	__RX_QUEUE_UNLOCK();

	server = (tls_server_t*)receive->tls_server_;
	tls_server_active(server);
}
tls_buff_t* tls_receive_pop(tls_receive_t* receive)
{
	tls_list_node_t*	list_head;
	tls_list_node_t*	list_node;
	tls_buff_t*			buff;

	assert(receive);
	// assert(buff);

	buff = NULL;
	__RX_QUEUE_LOCK();
	// tls_list_insert_tail(&receive->queue_receive_);
	list_head = &receive->queue_receive_;
	if (tls_list_empty(list_head))
	{
		__RX_QUEUE_UNLOCK();
		return NULL;
	}
	list_node = tls_list_remove_head(&receive->queue_receive_);
	__RX_QUEUE_UNLOCK();
	if (list_node)
	{
		buff = __LIST_CONTAIN_OF(list_node, tls_buff_t, list_node_);
	}
	return buff;
}

void* tls_receive_thread_work(void* arg)
{
	tls_receive_t*			receive;
	tls_server_t*			server;
	tls_statis_t*			tls_statis;
	tls_buff_t*				buff;
	tls_udp_socket_t*		udp_socket;
	S32						ret;
	
	assert(arg);
	receive = (tls_receive_t*)arg;
	server = (tls_server_t*)receive->tls_server_;
	tls_statis = (tls_statis_t*)server;
	udp_socket = server->udp_socket_;

	tls_event_reset(&receive->evt_stop_);
	while (receive->run_)
	{
		if (!receive->run_)
			break;

		buff = tls_server_acquire_recv_buff(server);
		if (NULL == buff)
		{
			LOG_ERROR("FATAL ERROR!!!-can not acquire receive buffer.");
			continue;
		}

		assert(buff->buf_size_ > 0);
		ret = tls_udp_socket_receive(udp_socket, buff->buf_, buff->buf_size_);
		if (ret > 0)
		{
			buff->size_ = ret;
			tls_receive_push(receive, buff);
			tls_statis_tls_rx(tls_statis, ret);
			LOG_DEBUG("UDP receive size:%d.\n", ret);
		}
		else
		{
			tls_server_release_buff(server, buff);
		}
	}
	tls_event_set(&receive->evt_stop_);
	return NULL;
}
