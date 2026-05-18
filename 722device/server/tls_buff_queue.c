#include <string.h>
#include "tls_afx.h"
#include "tls_log.h"
#include "tls_buff_queue.h"

static tls_buff_t*		tls_buff_allocate_struct(void);
static void				tls_buff_free_struct(tls_buff_t* buff);
static TLS_RESULT		tls_buff_allocate_buffer(tls_buff_t* buff);
static void				tls_buff_free_buffer(tls_buff_t* buff);

TLS_RESULT tls_buff_queue_initialize(tls_buff_queue_t* queue)
{
	int					oldvalue;
	int					newvalue;

	if (NULL == queue)
		return TLS_RESULT_E_INVALID_PARAM;

	oldvalue = 0;
	newvalue = 1;
	if (__sync_val_compare_and_swap(&queue->queue_init_, oldvalue, newvalue))
	{
		LOG_INFO("Tls buffer queue has already initialized.");
		return TLS_RESULT_S_CONTINUE;
	}

	memset(queue, 0, sizeof(tls_buff_queue_t));
	tls_list_initialize(&queue->list_buff_);
	tls_lock_init(&queue->queue_lock_);

	return TLS_RESULT_S_OK;
}

void tls_buff_queue_uninitialize(tls_buff_queue_t* queue)
{
	int					oldvalue;
	int					newvalue;

	if (NULL == queue)
		return;

	oldvalue = 1;
	newvalue = 0;
	if (!__sync_val_compare_and_swap(&queue->queue_init_, oldvalue, newvalue))
	{
		LOG_INFO("TLS buffer queue has already Uninitialized.");
		return;
	}

	tls_buff_queue_free(queue);
}

TLS_RESULT	tls_buff_queue_allocate(tls_buff_queue_t* queue, U32 size, U32 count)
{
	TLS_RESULT			result = TLS_RESULT_E_FAIL;
	tls_buff_t*			buff = NULL;
	U32					index;

	do
	{
		if (NULL  == queue || 0 == size || 0 == count)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}
		result = TLS_RESULT_S_OK;
		for (index = 0; index < count; index++)	
		{
			buff = tls_buff_allocate_struct();
			if (NULL == buff)
			{
				/* The memory resource is not available */
				LOG_ERROR("allocate tls buffer memory fail, index:%d", index);
				result = TLS_RESULT_E_ALLOCATE;
				break;
			}
			memset(buff, 0, sizeof(tls_buff_t));
			buff->buf_size_ = size;
			buff->priority_ = queue->priority_;
			result = tls_buff_allocate_buffer(buff);
			if (TlsResultFail(result))
			{
				/* The memory resource is not available */
				LOG_ERROR("allocate tls buffer fail, index:%d", index);
				result = TLS_RESULT_E_ALLOCATE;
				break;
			}

			tls_list_initialize(&buff->list_node_);
			tls_list_insert_tail(&queue->list_buff_, &buff->list_node_);
		}
		if (TlsResultFail(result))
		{
			if (buff != NULL)
			{
				tls_buff_free_buffer(buff);
				tls_buff_free_struct(buff);
				buff = NULL;
			}	
		}
	}while(0);
	if (TlsResultFail(result))
	{
		tls_buff_queue_free(queue);
	}
	return result;
}
TLS_RESULT tls_buff_queue_allocate_t(tls_buff_queue_t* queue ,U32 size, U32 count, U32 type)
{
	TLS_RESULT			result = TLS_RESULT_E_FAIL;
	tls_buff_t*			buff = NULL;
	U32					index;

	do
	{
		if (NULL  == queue || 0 == size || 0 == count)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}
		result = TLS_RESULT_S_OK;
		for (index = 0; index < count; index++)	
		{
			buff = tls_buff_allocate_struct();
			if (NULL == buff)
			{
				/* The memory resource is not available */
				LOG_ERROR("allocate tls buffer memory fail, index:%d", index);
				result = TLS_RESULT_E_ALLOCATE;
				break;
			}
			memset(buff, 0, sizeof(tls_buff_t));
			buff->buf_size_ = size;
			buff->type_ = type;
			buff->priority_ = queue->priority_;
			
			result = tls_buff_allocate_buffer(buff);
			if (TlsResultFail(result))
			{
				/* The memory resource is not available */
				LOG_ERROR("allocate tls buffer fail, index:%d", index);
				result = TLS_RESULT_E_ALLOCATE;
				break;
			}

			tls_list_initialize(&buff->list_node_);
			tls_list_insert_tail(&queue->list_buff_, &buff->list_node_);
		}
		if (TlsResultFail(result))
		{
			if (buff != NULL)
			{
				tls_buff_free_buffer(buff);
				tls_buff_free_struct(buff);
				buff = NULL;
			}	
		}
	}while(0);
	if (TlsResultFail(result))
	{
		tls_buff_queue_free(queue);
	}
	return result;
}

TLS_RESULT	tls_buff_queue_free(tls_buff_queue_t* queue)
{
	tls_buff_t*				buff;
	tls_list_node_t*		list_head;
	tls_list_node_t*		list_node;

	if (NULL == queue)
		return TLS_RESULT_E_INVALID_PARAM;
	
	list_head = &queue->list_buff_;
	list_node = __LIST_START_NODE(list_head);
	while (!tls_list_empty(list_head))
	{
		list_node = tls_list_remove_head(list_head);
		buff = __LIST_CONTAIN_OF(list_node, tls_buff_t, list_node_);
		if (buff)
		{
			tls_buff_delete(buff);
		}
	}
	return TLS_RESULT_S_OK;
}

tls_buff_t* tls_buff_queue_acquire(tls_buff_queue_t* queue)
{
	tls_buff_t*			buff = NULL;
	tls_list_node_t*	list_head = NULL;
	tls_list_node_t*	list_node = NULL;

	if (NULL == queue)
		return NULL;

	tls_lock(&queue->queue_lock_);
	list_head = &queue->list_buff_;
	if (tls_list_empty(list_head))
	{
		tls_unlock(&queue->queue_lock_);
		return NULL;
	}
	list_node = tls_list_remove_head(list_head);
	tls_unlock(&queue->queue_lock_);

	if (list_node != NULL)
	{
		buff = __LIST_CONTAIN_OF(list_node, tls_buff_t, list_node_);
	}	
	return buff;
}

void tls_buff_queue_release(tls_buff_queue_t* queue, tls_buff_t* buf)
{
	if (NULL == queue || NULL == buf || NULL == buf->buf_)
		return;

	tls_lock(&queue->queue_lock_);
	tls_list_insert_tail(&queue->list_buff_, &buf->list_node_);	
	tls_unlock(&queue->queue_lock_);
}

tls_buff_t*	tls_buff_create(U32 size, U32 type)
{
	tls_buff_t*		buff = NULL;

	if (0 == size)
		return NULL;
	
	buff = tls_buff_allocate_struct();
	if (NULL ==buff)
		return NULL;

	memset(buff, 0, sizeof(tls_buff_t));
	buff->buf_size_ = size;
	buff->type_ = type;
	if (TlsResultOk(tls_buff_allocate_buffer(buff)))
	{
		return buff;
	}
	else
	{
		if (buff)
			tls_buff_free_struct(buff);
		return NULL;
	}
}

void tls_buff_delete(tls_buff_t* buff)
{
	tls_buff_free_buffer(buff);
	tls_buff_free_struct(buff);
}

tls_buff_t*	tls_buff_allocate_struct(void)
{
	tls_buff_t* buff = NULL;
	buff = (tls_buff_t*)malloc(sizeof(tls_buff_t));
	return buff;
}
void tls_buff_free_struct(tls_buff_t* buff)
{
	if (buff)
	{
		free(buff);
	}
}
TLS_RESULT tls_buff_allocate_buffer(tls_buff_t* buff)
{
	if (NULL == buff)
		return TLS_RESULT_E_INVALID_PARAM;
	if (0 == buff->buf_size_)
		return TLS_RESULT_E_INVALID_DATA;

	buff->buf_ = (PU8)malloc(buff->buf_size_);
	if (NULL == buff->buf_)
		return TLS_RESULT_E_ALLOCATE;

	return TLS_RESULT_S_OK;
}
void tls_buff_free_buffer(tls_buff_t* buff)
{
	if (NULL == buff)
		return;
	if (NULL == buff->buf_)
		return;

	free(buff->buf_);
	buff->buf_ = NULL;
}
