#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "tls_afx.h"
#include "tls_log.h"
#include "tls_buff_queue.h"
#include "tls_statis.h"
#include "tls_server.h"
#include "tls_udp_socket.h"
#include "tls_work.h"
#include "tls_vsoa_publish.h"
#include "tls_statis_rate.h"
#include "tls_transmit.h"

#define QUEUE_ERROR						(-1)
#define QUEUE_FAIL						(-2)
#define QUEUE_NOT_EMPTY					(-3)
#define QUEUE_EMPTY						(-4)

#define __LOCK_QUEUE_TRANSMIT(t)		tls_lock(&((t)->queue_transmit_lock_))
#define __UNLOCK_QUEUE_TRANSMIT(t)		tls_unlock(&((t)->queue_transmit_lock_))

static void*			tls_transmit_thread_work(void* arg);
static inline void		tls_transmit_thread_poll(tls_transmit_t* transmit);
static inline S32		tls_transmit_handle(tls_transmit_t* transmit, tls_transmit_queue_t* queue);
static inline void		tls_transmit_update_transmit_queue(tls_transmit_t* transmit, U8 priority);

tls_transmit_t* tls_transmit_allocate(void)
{
	tls_transmit_t*		transmit = NULL; 
	transmit = (tls_transmit_t*)malloc(sizeof(tls_transmit_t));
	return transmit;
}
void tls_transmit_free(tls_transmit_t* t)
{
	if (t)
	{
		tls_transmit_stop(t);
		free(t);
	}
}

TLS_RESULT tls_transmit_start(void* s, tls_transmit_t* t)
{
	int						oldvalue = 0;
	int						newvalue = 1;
	U32						index;
	tls_transmit_queue_t*	queue;

	if (NULL == s || NULL == t)
		return TLS_RESULT_E_INVALID_PARAM;

	oldvalue = 0;
	newvalue = 1;
	if (__sync_val_compare_and_swap(&t->start_, oldvalue, newvalue))
	{
		LOG_INFO("Transmit mode  has already started.");
		return TLS_RESULT_S_CONTINUE;
	}

	t->tls_server_ = (tls_server_t*)s;
	t->udp_socket_ = ((tls_server_t*)s)->udp_socket_;

	for (index = TLS_QUEUE_PRI_0; index < TLS_QUEUE_MAX_COUNT; index++)
	{
		queue = &t->queue_transmits_[index];
		tls_list_initialize(&queue->list_buff_);
		tls_lock_init(&queue->list_buff_lock_);
		queue->queue_priority_ = index;
	}
	
	tls_event_initialize(&t->evt_loop_);
	tls_event_initialize(&t->evt_stop_);

	tls_lock_init(&t->queue_transmit_lock_);

	memset(&t->thread_transmit_, 0, sizeof(t->thread_transmit_));
	t->thread_transmit_.run_ = 1;
	t->thread_transmit_.thread_routine_ = tls_transmit_thread_work;
	t->thread_transmit_.arg_ = t;
	t->run_ = 1;
	tls_thread_start(&t->thread_transmit_);

	return TLS_RESULT_S_OK;
}
TLS_RESULT tls_transmit_stop(tls_transmit_t* t)
{
	int						oldvalue = 1;
	int						newvalue = 0;
	U32						index;
	tls_transmit_queue_t*	queue;

	if (NULL == t)
		return TLS_RESULT_E_INVALID_PARAM;

	oldvalue = 1;
	newvalue = 0;
	if (!__sync_val_compare_and_swap(&t->start_, oldvalue, newvalue))
	{
		LOG_INFO("Transmit mode  has already stop.");
		return TLS_RESULT_S_CONTINUE;
	}

	t->run_ = 0;
	tls_event_set(&t->evt_loop_);
	tls_event_timewait(&t->evt_stop_, 2000);
	tls_thread_stop(&t->thread_transmit_);

	tls_event_uninitialize(&t->evt_loop_);
	tls_event_uninitialize(&t->evt_stop_);

	for (index = TLS_QUEUE_PRI_0; index < TLS_QUEUE_MAX_COUNT; index++)
	{
		queue = &t->queue_transmits_[index];
		tls_lock_uninit(&queue->list_buff_lock_);
	}

	tls_lock_uninit(&t->queue_transmit_lock_);
	t->queue_transmit_ = NULL;
	return TLS_RESULT_S_OK;
}
void tls_transmit_release_buff(tls_transmit_t* t)
{
	tls_server_t*				server;
	tls_list_node_t*			head;
	tls_list_node_t*			node;
	tls_buff_t*					buff;
	U32							index;
	tls_transmit_queue_t*		queue;
	tls_buff_queue_t*			buff_queue;

	if (NULL == t)
		return;

	server = (tls_server_t*)t->tls_server_;

#if 0
	head = &t->queue_transmit_;
	while (!tls_list_empty(head))
	{
		node = tls_list_remove_head(head);
		buff = __LIST_CONTAIN_OF(node, tls_buff_t, list_node_);
		if (buff)
		{
			tls_buff_queue_release(&server->queue_send_buff_, buff);
		}
	}
#endif

	for (index = TLS_QUEUE_PRI_0; index < TLS_QUEUE_MAX_COUNT; index++)
	{
		queue = &t->queue_transmits_[index];
		head = &queue->list_buff_;
		while (!tls_list_empty(head))
		{
			node = tls_list_remove_head(head);
			buff = __LIST_CONTAIN_OF(node, tls_buff_t, list_node_);
			if (buff)
			{
				buff_queue = &server->queue_transmit_buff_[buff->priority_];
				tls_buff_queue_release(buff_queue, buff);
			}
		}
	}
}

S32	tls_transmit_xmit_data(tls_transmit_t* t, PU8 data, U32 size)
{
	return 0;
}
TLS_RESULT tls_transmit_xmit(tls_transmit_t* t, tls_buff_t* buff)
{
	return TLS_RESULT_S_OK;
}
TLS_RESULT tls_transmit_push(tls_transmit_t* t, tls_buff_t* buff)
{
	tls_transmit_queue_t*		queue;
	U8							priority;
	
	if (NULL == t || NULL == buff)
	{
		LOG_ERROR("ERROR-Invalid parameters.");
		return TLS_RESULT_E_INVALID_PARAM;
	}
	if (NULL == buff->buf_ || 0 == buff->size_ || buff->priority_ > TLS_QUEUE_PRI_7)
	{
		LOG_ERROR("ERROR-Invalid data.");
		return TLS_RESULT_E_INVALID_DATA;
	}

	if (!t->start_)
		return TLS_RESULT_E_NOTREADY;


	priority = buff->priority_;
	queue = &t->queue_transmits_[priority];

	tls_lock(&queue->list_buff_lock_);
	tls_list_insert_tail(&queue->list_buff_, &buff->list_node_);
	tls_unlock(&queue->list_buff_lock_);

	tls_transmit_update_transmit_queue(t, priority);
	tls_transmit_active(t);

	return TLS_RESULT_S_OK;
}

void tls_transmit_active(tls_transmit_t* t)
{
	assert(t);
	tls_event_set(&t->evt_loop_);
}
void tls_transmit_deactive(tls_transmit_t* t)
{
	assert(t);
	tls_event_reset(&t->evt_loop_);
}

void* tls_transmit_thread_work(void* arg)
{
	tls_transmit_t*			transmit;

	if (NULL == arg)
		return NULL;
	transmit = (tls_transmit_t*)arg;

	tls_event_reset(&transmit->evt_stop_);
	while (transmit->run_)
	{
		tls_event_wait(&transmit->evt_loop_);
		if (!transmit->run_)
			break;
		tls_transmit_thread_poll(arg);
	}
	tls_event_set(&transmit->evt_stop_);

	return NULL;
}
inline void  tls_transmit_thread_poll(tls_transmit_t* transmit)
{
	tls_server_t*					server;
	tls_udp_socket_t*				udp_socket;
	tls_list_node_t*				list_head;
	tls_list_node_t*				list_node;
	tls_buff_t*						buff;

	S32								ret;
	U32								priority = 0;

	assert(transmit);

	__LOCK_QUEUE_TRANSMIT(transmit);
	if (NULL == transmit->queue_transmit_)
		transmit->queue_transmit_ = &transmit->queue_transmits_[TLS_QUEUE_PRI_7];

	ret = tls_transmit_handle(transmit, transmit->queue_transmit_);	
	if (QUEUE_EMPTY == ret)
	{
		priority = transmit->queue_transmit_->queue_priority_;
		if (priority > TLS_QUEUE_PRI_0)
		{
			--priority;
			transmit->queue_transmit_ = &transmit->queue_transmits_[priority];
		}
		else
		{
			tls_transmit_deactive(transmit);
		}
	}
	__UNLOCK_QUEUE_TRANSMIT(transmit);
}
inline S32 tls_transmit_handle(tls_transmit_t* transmit, tls_transmit_queue_t* queue)
{
	tls_server_t*					server;
	tls_statis_t*					tls_statis;
	tls_udp_socket_t*				udp_socket;
	tls_vsoa_publish_t*				vsoa_publish;
	tls_list_node_t*				list_head;
	tls_list_node_t*				list_node;
	tls_buff_t*						buff;

	S32								ret;
	S32								result = QUEUE_NOT_EMPTY;

	if (NULL == transmit || NULL == queue)
		return QUEUE_ERROR;

	server = (tls_server_t*)transmit->tls_server_;
	tls_statis = (tls_statis_t*)server->tls_statis_;
	udp_socket = server->udp_socket_;	
	vsoa_publish = server->tls_vsoa_publish_;

	tls_lock(&queue->list_buff_lock_);
	list_head = &queue->list_buff_;

	if (tls_list_empty(&queue->list_buff_))
	{
		tls_unlock(&queue->list_buff_lock_);	
		return QUEUE_EMPTY;
	}

	list_node = tls_list_remove_head(list_head);
	if (tls_list_empty(&queue->list_buff_))
	{
		result = QUEUE_EMPTY;
	}
	else
	{
		result = QUEUE_NOT_EMPTY;
	}
	tls_unlock(&queue->list_buff_lock_);
	
	if (NULL == list_node)
		return QUEUE_ERROR;

	buff = __LIST_CONTAIN_OF(list_node, tls_buff_t, list_node_);
	if (NULL == buff)
		return QUEUE_ERROR;

	if (TLS_UDP == buff->transmode_)
	{
		ret = tls_udp_socket_transmit(udp_socket, buff->buf_, buff->size_);
		if (ret > 0)
		{
			tls_statis_rate_tx_output(buff->size_);
			LOG_DEBUG("udp send size:%d, queue's pri:%d, buff's pri:%d", ret, queue->queue_priority_, buff->priority_);
			// tls_statis_tls_tx(tls_statis, buff->size_);
		}
	}
	else if (TLS_VSOA == buff->transmode_)
	{
#if 0
		ret = vsoa_publish_data(vsoa_publish, buff->buf_, buff->size_);
		if (ret > 0)
		{
			// tls_statis_tls_tx(tls_statis, buff->size_);
			tls_statis_rate_tx_output(buff->size_);
		}
#endif
	}
	tls_server_release_buff(server, buff);
	return result;
}
inline void tls_transmit_update_transmit_queue(tls_transmit_t* transmit, U8 priority)
{
	tls_transmit_queue_t*		queue;
	
	if (NULL == transmit || priority > TLS_QUEUE_PRI_7)
		return;

	__LOCK_QUEUE_TRANSMIT(transmit);
	queue = transmit->queue_transmit_;
	if (NULL == queue)
	{
		transmit->queue_transmit_ = &transmit->queue_transmits_[priority];
	}
	else if (priority > queue->queue_priority_)
	{
		transmit->queue_transmit_ = &transmit->queue_transmits_[priority];	
	}
	__UNLOCK_QUEUE_TRANSMIT(transmit);
}
