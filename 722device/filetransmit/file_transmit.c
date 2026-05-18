#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include "file_block.h"
#include "file_entry.h"
#include "file_transmit.h"
#include "tls_log.h"

#define FILE_BLOCK_SIZE_MIN			(5120) // (4096+1024)
#define FILE_ENTRY_READ_SLEEPUS		(10)



static TLS_RESULT				file_transmit_init_subscribe(file_transmit_t* file_transmit); 
static void						file_transmit_uninit_subscribe(file_transmit_t* file_transmit); 

static TLS_RESULT				file_transmit_create_queue_file_block(file_transmit_t* file_transmit);
static void						file_transmit_delete_queue_file_block(file_transmit_t* file_transmit);
static TLS_RESULT				file_transmit_start_file_consume(file_transmit_t* file_transmit);
static void						file_transmit_stop_file_consume(file_transmit_t* file_transmit);

static TLS_RESULT				file_transmit_init_file_consume(file_transmit_t* file_transmit);
static void						file_transmit_uninit_file_consume(file_transmit_t* file_transmit);

static file_block_t*			file_transmit_alloc_file_block(U32 block_size);
static void						file_transmit_free_file_block(file_block_t* file_block);

static void						file_transmit_start_work_thread(file_transmit_t* file_transmit);
static void						file_transmit_stop_work_thread(file_transmit_t* file_transmit);
static void						file_transmit_active_work_thread(file_transmit_t* file_transmit);
static void						file_transmit_deactive_work_thread(file_transmit_t* file_transmit);

static TLS_RESULT				file_transmit_start_subscribe(file_transmit_t* file_transmit);
static void						file_transmit_stop_subscribe(file_transmit_t* file_transmit);
static void						on_file_transmit_subscribe_connect(void* arg, int connect);
static void						on_file_transmit_subscribe_message(void* arg, vsoa_url_t* url, vsoa_payload_t* payload);
static inline void				on_file_transmit_receive_file_data(file_transmit_t* file_transmit, PU8 data, U32 size);

static void*					file_transmit_thread_work(void* arg);
static void						file_transmit_thread_poll(file_transmit_t* file_transmit);


static inline file_block_t*		file_transmit_grab_file_block(file_transmit_t* file_transmit);
static inline void				file_transmit_drop_file_block(file_transmit_t* file_transmit, file_block_t* file_block);

static inline void				file_transmit_push_file_block(file_transmit_t* file_transmit, file_block_t* file_block);
static inline file_block_t*		file_transmit_pop_file_block(file_transmit_t* file_transmit);
static inline void				file_transmit_handle_file_block(file_transmit_t* file_transmit, file_block_t* file_block);

static inline void				file_transmit_file_entry_poll(file_transmit_t* file_transmit);

static file_entry_t*			file_transmit_find_file_entry(file_transmit_t* file_transmit, const char* filename);
static file_entry_t*			file_transmit_create_file_entry(file_transmit_t* file_transmit, file_block_t* file_block);
static void						file_transmit_delete_file_entry(file_entry_t* file_entry);
static void						file_transmit_add_file_entry_list(file_transmit_t* file_transmit, file_entry_t* file_entry);

file_transmit_t* file_transmit_allocate(void)
{
	file_transmit_t*	file_transmit = NULL;
	file_transmit = (file_transmit_t*)malloc(sizeof(file_transmit_t));
	return file_transmit;
}
void file_transmit_free(file_transmit_t* file_transmit)
{
	if (file_transmit)
		free(file_transmit);
}

TLS_RESULT file_transmit_initialize(const file_transmit_param_t* param, file_transmit_t* file_transmit)
{
	TLS_RESULT						result;
	file_transmit_param_t*			transmit_param;
	int								oldvalue;
	int								newvalue;

	do
	{
		if (NULL == param || NULL == file_transmit)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			LOG_ERROR("ERROR-file transmit invalid parameters.");
			break;
		}
		oldvalue = 0;
		newvalue = 1;
		if (__sync_val_compare_and_swap(&file_transmit->file_transmit_init_, oldvalue, newvalue))
		{
			result = TLS_RESULT_S_CONTINUE;
			LOG_INFO("file transmit is already initialized.");
			break;
		}

		if (0 == strlen(param->file_subscribe_name_) || 0 == strlen(param->file_subscribe_url_))
		{
			result = TLS_RESULT_E_INVALID_DATA;
			LOG_ERROR("ERROR-file transmit invalid subscribe data, name:%s url:%s", 
					param->file_subscribe_name_, param->file_subscribe_url_);
			break;
		}

		memcpy(&file_transmit->file_transmit_param_, param, sizeof(file_transmit_param_t));
		transmit_param = &file_transmit->file_transmit_param_;
		if (0 == strlen(transmit_param->file_dir_))
			strcpy(transmit_param->file_dir_, "./");
		if (0 == transmit_param->file_block_count_)
			transmit_param->file_block_count_ = 1024;
		if (transmit_param->file_block_size_ < FILE_BLOCK_SIZE_MIN)
			transmit_param->file_block_size_ = FILE_BLOCK_SIZE_MIN;

		LOG_INFO("---------------- file transmit config ----------------");
		LOG_INFO("file storage base dir:%s", transmit_param->file_dir_);
		LOG_INFO("file block count:%d",transmit_param->file_block_count_);
		LOG_INFO("file block size:%d", transmit_param->file_block_size_);
		LOG_INFO("file subscribe name:%s", transmit_param->file_subscribe_name_);
		LOG_INFO("file subscribe pass:%s", transmit_param->file_subscribe_pass_);
		LOG_INFO("file subscribe url:%s", transmit_param->file_subscribe_url_);
		LOG_INFO("file publish interval: %dms", transmit_param->file_send_interval_);
		LOG_INFO("file publish dest addr: %d", transmit_param->file_publish_dst_addr_);
		LOG_INFO("file publish dest entry: %d", transmit_param->file_publish_dst_entry_);
		LOG_INFO("file publish url: %s", transmit_param->file_publish_url_);
		LOG_INFO("------------------------------------------------------\n");

		tls_list_initialize(&file_transmit->queue_alloc_block_);
		tls_lock_init(&file_transmit->queue_alloc_block_lock_);

		tls_list_initialize(&file_transmit->queue_file_block_);
		tls_lock_init(&file_transmit->queue_file_block_lock_);

		tls_list_initialize(&file_transmit->queue_file_entry_);

		result = file_transmit_init_subscribe(file_transmit);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-file transmit init subscribe failed.");
			break;
		}
		result = file_transmit_create_queue_file_block(file_transmit);
		if (TlsResultFail(result))
		{
			LOG_ERROR("file transmit create file block queue failed.");
			break;
		}
		result = file_transmit_init_file_consume(file_transmit);
		if (TlsResultFail(result))
		{
			LOG_ERROR("file transmit init file consume failed.");
			break;
		}

		LOG_DEBUG("file transmit initialize OK");
	}while (0);
	if (TlsResultFail(result))
	{
		file_transmit_uninitialize(file_transmit);
	}
	return result;
}
void file_transmit_uninitialize(file_transmit_t* file_transmit)
{
	int								oldvalue;
	int								newvalue;

	if (NULL == file_transmit)
		return;

	oldvalue = 1;
	newvalue = 0;
	if (!__sync_val_compare_and_swap(&file_transmit->file_transmit_init_, oldvalue, newvalue))
	{
		LOG_INFO("File transmit has already uninitialize.");
		return;
	}

	file_transmit_stop_service(file_transmit);
	file_transmit_delete_queue_file_block(file_transmit);

	tls_list_initialize(&file_transmit->queue_alloc_block_);
	tls_lock_uninit(&file_transmit->queue_alloc_block_lock_);

	tls_list_initialize(&file_transmit->queue_file_block_);
	tls_lock_uninit(&file_transmit->queue_file_block_lock_);

	tls_list_initialize(&file_transmit->queue_file_entry_);

	file_transmit_uninit_subscribe(file_transmit);

	file_transmit_uninit_file_consume(file_transmit);
}

TLS_RESULT file_transmit_start_service(file_transmit_t* file_transmit)
{
	TLS_RESULT					result; 
	int							oldvalue;
	int							newvalue;

	do
	{
		if (NULL == file_transmit || 0 == file_transmit->file_transmit_init_)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			LOG_ERROR("ERROR-Invalid parameters.");
			break;
		}
		oldvalue = 0;
		newvalue = 1;
		if (__sync_val_compare_and_swap(&file_transmit->file_transmit_start_, oldvalue, newvalue))
		{
			result = TLS_RESULT_S_CONTINUE;
			LOG_ERROR("File transmit service has already started.");
			break;
		}

		file_transmit_start_work_thread(file_transmit);
		result = file_transmit_start_subscribe(file_transmit);
		if (TlsResultFail(result))
		{
			LOG_ERROR("File transmit start subscribe failed");
			break;
		}
		result = file_transmit_start_file_consume(file_transmit);
		if (TlsResultFail(result))
		{
			LOG_ERROR("File transmit start file consume failed");
			break;
		}

		LOG_DEBUG("File transmit service start ok!.\n");
	}while (0);
	if (TlsResultFail(result))
	{
		file_transmit_stop_service(file_transmit);
	}
	return result;
}
void file_transmit_stop_service(file_transmit_t* file_transmit)
{
	int					oldvalue;
	int					newvalue;

	if (NULL == file_transmit)
		return;

	oldvalue = 1;
	newvalue = 0;
	if (!__sync_val_compare_and_swap(&file_transmit->file_transmit_start_, oldvalue, newvalue))
	{
		LOG_INFO("File transmit service has already stopd.");
		return;
	}
	file_transmit_stop_file_consume(file_transmit);
	file_transmit_stop_subscribe(file_transmit);
	file_transmit_stop_work_thread(file_transmit);
}
void file_transmit_release_file_block(file_transmit_t* file_transmit, void* file_block)
{
	file_transmit_drop_file_block(file_transmit, file_block);
}

TLS_RESULT file_transmit_init_subscribe(file_transmit_t* file_transmit)
{
	app_vsoa_subscribe_t*	app_subscribe = NULL;
	app_subscribe = app_vsoa_subscribe_allocate();
	if (NULL == app_subscribe)
		return TLS_RESULT_E_ALLOCATE;
	memset(app_subscribe, 0, sizeof(*app_subscribe));
	file_transmit->file_vsoa_subscribe_ = app_subscribe;
	return TLS_RESULT_S_OK;	
}
void file_transmit_uninit_subscribe(file_transmit_t* file_transmit)
{
	app_vsoa_subscribe_t*	app_subscribe = NULL;
	if (NULL == file_transmit)
		return;
	app_subscribe = file_transmit->file_vsoa_subscribe_;
	app_vsoa_subscribe_free(app_subscribe);
	file_transmit->file_vsoa_subscribe_ = NULL;
}
TLS_RESULT file_transmit_create_queue_file_block(file_transmit_t* file_transmit)
{
	TLS_RESULT				result = TLS_RESULT_E_FAIL;
	file_transmit_param_t*	file_transmit_param;
	tls_list_node_t*		queue_head;
	U32						block_count;
	U32						block_size;
	U32						index;
	file_block_t*			file_block;

	do
	{
		if (NULL == file_transmit)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}
		queue_head = &file_transmit->queue_alloc_block_;
		file_transmit_param = &file_transmit->file_transmit_param_;
		block_count = file_transmit_param->file_block_count_;
		block_size = file_transmit_param->file_block_size_;
		LOG_DEBUG("Now allocate file block, block count:%d, block size:%d", block_count, block_size);
		result = TLS_RESULT_S_OK;
		for (index = 0; index < block_count; index++)
		{
			file_block = file_transmit_alloc_file_block(block_size);
			if (NULL == file_block)
			{
				LOG_ERROR("ERROR-allocate file block failed, index:%d.", index);
				result = TLS_RESULT_E_ALLOCATE;
				break;
			}
			tls_list_initialize(&file_block->block_node_);
			tls_list_insert_tail(queue_head, &file_block->block_node_);
		}
	}while(0);

	if (TlsResultFail(result))
	{
		LOG_ERROR("allocate file block list fail");
		file_transmit_delete_queue_file_block(file_transmit);
	}
	return result;
}
void file_transmit_delete_queue_file_block(file_transmit_t* file_transmit)
{
	if (NULL == file_transmit)
		return;

	tls_list_node_t*		queue_head;
	tls_list_node_t*		queue_node;
	file_block_t*			file_block;

	queue_head = &file_transmit->queue_alloc_block_;
	queue_node = __LIST_START_NODE(queue_head);
	while (!tls_list_empty(queue_head))
	{
		queue_node = tls_list_remove_head(queue_head);
		file_block = __LIST_CONTAIN_OF(queue_node, file_block_t, block_node_);
		if (file_block)
		{
			file_transmit_free_file_block(file_block);
		}
	}
}
TLS_RESULT file_transmit_init_file_consume(file_transmit_t* file_transmit)
{
#if 0
	TLS_RESULT				result;
	file_consume_t*			file_consume = NULL;
	file_consume = file_consume_allocate();
	if (NULL == file_consume)
		return TLS_RESULT_E_ALLOCATE;
	memset(file_consume, 0, sizeof(*file_consume));
	result = file_consume_initialize(file_transmit, file_consume);
	if (TlsResultOk(result))
		file_transmit->file_consume_ = (void*)file_consume;
	return result;
#endif

	return 0;
}
void file_transmit_uninit_file_consume(file_transmit_t* file_transmit)
{
#if 0
	file_consume_t*			file_consume;
	if (NULL == file_transmit)
		return;
	file_consume = file_transmit->file_consume_;
	file_consume_uninitialize(file_consume);
	file_consume_free(file_consume);
	file_transmit->file_consume_ = NULL;
#endif
}
TLS_RESULT file_transmit_start_file_consume(file_transmit_t* file_transmit)
{
#if 0
	file_consume_t*			file_consume;
	
	if (NULL == file_transmit || NULL == file_transmit->file_consume_)
		return TLS_RESULT_E_INVALID_PARAM;

	file_consume = file_transmit->file_consume_;
	return file_consume_start(file_consume);
#endif

	return 0;
}
void file_transmit_stop_file_consume(file_transmit_t* file_transmit)
{
#if 0
	if (NULL == file_transmit || NULL == file_transmit->file_consume_)
		return;
	file_consume_stop((file_consume_t*)file_transmit->file_consume_);
#endif
}
file_block_t* file_transmit_alloc_file_block(U32 block_size)
{
	file_block_t*		block = NULL;

	if (0 == block_size)
	{
		LOG_ERROR("ERROR-block size is 0");
		goto __ERROR;
	}

	block = (file_block_t*)malloc(sizeof(file_block_t));
	if (NULL == block)
	{
		LOG_ERROR("Allocate a file_block_t struct fail");
		goto __ERROR;
	}
	
	memset(block, 0, sizeof(*block));
	block->block_size_ = block_size;
	block->block_data_ = (PU8)malloc(block_size);
	if (NULL == block->block_data_)
	{
		LOG_ERROR("allocate the block data fail");
		goto __ERROR;
	}
	return block;

__ERROR:
	if (block != NULL)
	{
		if (block->block_data_)
			free(block->block_data_);
		free(block);
	}
	return NULL;
}
void file_transmit_free_file_block(file_block_t* file_block)
{
	if (file_block)
	{
		if (file_block->block_data_)
			free(file_block->block_data_);
		free(file_block);
	}
}


void file_transmit_start_work_thread(file_transmit_t* file_transmit)
{
	if (NULL == file_transmit)
		return;

	tls_event_initialize(&file_transmit->evt_thread_loop_);
	tls_event_initialize(&file_transmit->evt_thread_stop_);

	memset(&file_transmit->thread_work_, 0, sizeof(file_transmit->thread_work_));
	file_transmit->thread_work_.arg_ = (void*)file_transmit;
	file_transmit->thread_work_.thread_routine_ = file_transmit_thread_work;
	file_transmit->thread_run_ = 1;
	tls_event_set(&file_transmit->evt_thread_loop_);
	tls_thread_start(&file_transmit->thread_work_);
}
void file_transmit_stop_work_thread(file_transmit_t* file_transmit)
{
	if (NULL == file_transmit)
		return;

	file_transmit->thread_run_ = 0;
	tls_event_set(&file_transmit->evt_thread_loop_);
	tls_event_timewait(&file_transmit->evt_thread_stop_, 3000);
	tls_thread_stop(&file_transmit->thread_work_);

	tls_event_uninitialize(&file_transmit->evt_thread_loop_);
	tls_event_uninitialize(&file_transmit->evt_thread_stop_);
}
void file_transmit_active_work_thread(file_transmit_t* file_transmit)
{
	assert(file_transmit);
	tls_event_set(&file_transmit->evt_thread_loop_);
}
void file_transmit_deactive_work_thread(file_transmit_t* file_transmit)
{
	assert(file_transmit);
	tls_event_reset(&file_transmit->evt_thread_loop_);
}
TLS_RESULT file_transmit_start_subscribe(file_transmit_t* file_transmit)
{
	app_vsoa_subscribe_param_t	file_subscribe_param;
	app_vsoa_subscribe_t*		file_subscribe;
	TLS_RESULT					result = TLS_RESULT_E_FAIL;

	do
	{
		if (NULL == file_transmit || NULL == file_transmit->file_vsoa_subscribe_)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			LOG_ERROR("ERROR-Invalid parameters");
			break;
		}
		file_subscribe = file_transmit->file_vsoa_subscribe_;

		memset(&file_subscribe_param, 0, sizeof(file_subscribe_param));

		strcpy(file_subscribe_param.server_name_, file_transmit->file_transmit_param_.file_subscribe_name_);
		strcpy(file_subscribe_param.server_password_, file_transmit->file_transmit_param_.file_subscribe_pass_);
		strcpy(file_subscribe_param.subscribe_url_, file_transmit->file_transmit_param_.file_subscribe_url_);

		file_subscribe_param.app_subscribe_arg_ = (void*)file_transmit;
		file_subscribe_param.on_app_subscribe_connect = on_file_transmit_subscribe_connect;
		file_subscribe_param.on_app_subscribe_message = on_file_transmit_subscribe_message;

		result = app_vsoa_subscribe_start(&file_subscribe_param, file_subscribe);
	}while(0);
	if (TlsResultFail(result))
	{
		LOG_ERROR("app vsoa subscribe start failed");
		file_transmit_stop_subscribe(file_transmit);
	}

	return result;
}
void file_transmit_stop_subscribe(file_transmit_t* file_transmit)
{
	if (NULL == file_transmit || NULL == file_transmit->file_vsoa_subscribe_)
		return;
	app_vsoa_subscribe_stop(file_transmit->file_vsoa_subscribe_);
}

void on_file_transmit_subscribe_connect(void* arg, int connect)
{
	file_transmit_t*		file_transmit;

	if (NULL == arg)
		return;

	LOG_DEBUG("on subscribe connect status:%s", connect ? "CONNECT" : "DISCONNECT");
	file_transmit = (file_transmit_t*)arg;
	file_transmit->file_vsoa_connect_ = connect;
}
void on_file_transmit_subscribe_message(void* arg, vsoa_url_t* url, vsoa_payload_t* payload)
{
	file_transmit_t*		file_transmit;

	if (NULL == arg || NULL == url || NULL == payload)
	{
		LOG_ERROR("Invalid parameters");
		return;
	}

	file_transmit = (file_transmit_t*)arg;
	if (NULL == payload->data || 0 == payload->data_len)
	{
		LOG_ERROR("Invalid datas");
		return;
	}

	on_file_transmit_receive_file_data(file_transmit, (PU8)payload->data, (U32)payload->data_len);
}
inline void	on_file_transmit_receive_file_data(file_transmit_t* file_transmit, PU8 data, U32 size)
{
	file_block_t*			file_block;
	file_block_hdr_t*		block_hdr;
	block_hdr = (file_block_hdr_t*)data;


	file_block = file_transmit_grab_file_block(file_transmit);
	if (NULL == file_block)
	{
		file_block = file_transmit_alloc_file_block(file_transmit->file_transmit_param_.file_block_size_);
		if (NULL == file_block)
		{
			LOG_ERROR("Allocate file block data fail");
			return;
		}
	}

	file_block->size_ = size;
	memcpy(file_block->block_data_, data, size);
	file_transmit_push_file_block(file_transmit, file_block);
	file_transmit_active_work_thread(file_transmit);
}
void* file_transmit_thread_work(void* arg)
{
	file_transmit_t*		file_transmit;

	if (NULL == arg)
		return NULL;
	file_transmit = (file_transmit_t*)arg;

	tls_event_reset(&file_transmit->evt_thread_stop_);
	while (file_transmit->thread_run_)
	{
		tls_event_wait(&file_transmit->evt_thread_loop_);
		if (0 == file_transmit->thread_run_)
			break;
		file_transmit_thread_poll(file_transmit);
	}
	tls_event_set(&file_transmit->evt_thread_loop_);
	return NULL;
}
void file_transmit_thread_poll(file_transmit_t* file_transmit)
{
	file_block_t*			file_block;

	file_block = file_transmit_pop_file_block(file_transmit);
#if 0
	if (file_block)
	{
		file_entry_show_block_info(file_block);
		file_transmit_drop_file_block(file_transmit, file_block);
	}
	else
	{
		file_transmit_deactive_work_thread(file_transmit);
	}
#endif

	if (file_block)
		file_transmit_handle_file_block(file_transmit,  file_block);
	file_transmit_file_entry_poll(file_transmit);
}


inline file_block_t* file_transmit_grab_file_block(file_transmit_t* file_transmit)
{
	file_block_t*			file_block = NULL;
	tls_list_node_t*		list_head;
	tls_list_node_t*		list_node;

	assert(file_transmit);
	list_head = &file_transmit->queue_alloc_block_;
	tls_lock(&file_transmit->queue_alloc_block_lock_);
	if (!tls_list_empty(list_head))
	{
		list_node = tls_list_remove_head(list_head);
		if (list_node)
		{
			file_block = __LIST_CONTAIN_OF(list_node, file_block_t, block_node_);
		}
	}
	else
	{
		LOG_WARN("Resource warnning: The queue alloc block list is empty!");
	}
	tls_unlock(&file_transmit->queue_alloc_block_lock_);
	
	return file_block;
}
inline void file_transmit_drop_file_block(file_transmit_t* file_transmit, file_block_t* file_block)
{
	assert(file_transmit);
	assert(file_block);

	tls_list_initialize(&file_block->block_node_);
	tls_lock(&file_transmit->queue_alloc_block_lock_);
	tls_list_insert_tail(&file_transmit->queue_alloc_block_, &file_block->block_node_);		
	tls_unlock(&file_transmit->queue_alloc_block_lock_);
}
inline void file_transmit_push_file_block(file_transmit_t* file_transmit, file_block_t* file_block)
{
	assert(file_transmit);
	assert(file_block);
	tls_list_initialize(&file_block->block_node_);
	tls_lock(&file_transmit->queue_file_block_lock_);
	tls_list_insert_tail(&file_transmit->queue_file_block_, &file_block->block_node_);		
	tls_unlock(&file_transmit->queue_file_block_lock_);
}
inline file_block_t* file_transmit_pop_file_block(file_transmit_t* file_transmit)
{
	file_block_t*			file_block = NULL;
	tls_list_node_t*		list_head;
	tls_list_node_t*		list_node;

	assert(file_transmit);
	tls_lock(&file_transmit->queue_file_block_lock_);
	list_head = &file_transmit->queue_file_block_;
	if (!tls_list_empty(list_head))
	{
		list_node = tls_list_remove_head(list_head);
		if (list_node)
		{
			file_block = __LIST_CONTAIN_OF(list_node, file_block_t, block_node_);
		}
	}
	tls_unlock(&file_transmit->queue_file_block_lock_);
	
	return file_block;
}
inline void file_transmit_handle_file_block(file_transmit_t* file_transmit, file_block_t* file_block)
{
	TLS_RESULT			result = TLS_RESULT_E_FAIL;
	file_block_hdr_t*	file_block_hdr;
	file_entry_t*		file_entry = NULL;

	assert(file_transmit);
	assert(file_block);

	file_block_hdr = (file_block_hdr_t*)file_block->block_data_;
	if (file_block_hdr->flag_ != 0x55a0 || 0 == strlen(file_block_hdr->filename_) || 0 == file_block_hdr->filesize_)
		goto __ERROR;

	if (0 == file_block_hdr->sequence_)
	{
		// create new file entry and open the file, than add to file entry list, add file block to entry's block list
		file_entry = file_transmit_create_file_entry(file_transmit, file_block);
		if (NULL == file_entry)
		{
			LOG_ERROR("ERROR-create file entry fail");
			goto __ERROR;
		}
		file_transmit_add_file_entry_list(file_transmit, file_entry);
	}
	else
	{
		// find the file entry by filename, add file block to it's block list
		file_entry = file_transmit_find_file_entry(file_transmit, file_block_hdr->filename_);
		if (NULL == file_entry)
		{
			LOG_ERROR("ERROR-find the file entry fail");
			goto __ERROR;
		}
	}

	file_entry_add_file_block(file_entry, file_block);
	result = TLS_RESULT_S_OK;

__ERROR:
	if (TlsResultFail(result))
	{
		LOG_ERROR("ERROR-drop the file block");
		file_transmit_drop_file_block(file_transmit, file_block);
	}
}
inline void file_transmit_file_entry_poll(file_transmit_t* file_transmit)
{
	tls_list_node_t*			list_head;
	tls_list_node_t*			list_node;
	tls_list_node_t*			next_node;
	file_entry_t*				file_entry;

	assert(file_transmit);

	list_head = &file_transmit->queue_file_entry_;
	list_node = __LIST_START_NODE(list_head);
	while (__LIST_IS_LAST_NODE(list_head, list_node))
	{
		file_entry = __LIST_CONTAIN_OF(list_node, file_entry_t, list_node_);
		if (file_entry)
		{
			file_entry_handle(file_entry);
		}
		list_node = __LIST_NEXT_NODE(list_node);
	}


	list_head = &file_transmit->queue_file_entry_;
	list_node = __LIST_START_NODE(list_head);
	while (__LIST_IS_LAST_NODE(list_head, list_node))
	{
		file_entry = __LIST_CONTAIN_OF(list_node, file_entry_t, list_node_);
		if (file_entry && file_entry_handle_complete(file_entry))
		{
			next_node = __LIST_NEXT_NODE(list_node);
			tls_list_remove_node(list_head, list_node);
			list_node = next_node;

			file_transmit_delete_file_entry(file_entry);			
		}
		else
		{
			list_node = __LIST_NEXT_NODE(list_node);
		}		
	}

	if (tls_list_empty(&file_transmit->queue_file_entry_))
	{
		LOG_ERROR("queue file entry is empty");
		file_transmit_deactive_work_thread(file_transmit);
	}
}

file_entry_t* file_transmit_find_file_entry(file_transmit_t* file_transmit, const char* filename)
{
	file_entry_t*			find_entry = NULL;
	file_entry_t*			file_entry = NULL;
	tls_list_node_t*		list_head;
	tls_list_node_t*		list_node;

	if (NULL == file_transmit || NULL == filename || 0 == strlen(filename))
		return find_entry;

	list_head = &file_transmit->queue_file_entry_;
	list_node = __LIST_START_NODE(list_head);
	while (__LIST_IS_LAST_NODE(list_head, list_node))
	{
		file_entry = __LIST_CONTAIN_OF(list_node, file_entry_t, list_node_);
		if (file_entry)
		{
			if (!strcmp(file_entry->file_name_, filename))
			{
				find_entry = file_entry;
				break;
			}
		}
		list_node = __LIST_NEXT_NODE(list_node);
	}
	return find_entry;
}
file_entry_t* file_transmit_create_file_entry(file_transmit_t* file_transmit, file_block_t* file_block)
{
	file_block_hdr_t*		file_block_hdr;
	file_entry_t*			file_entry = NULL;
	file_transmit_param_t*	file_transmit_param;

	do
	{
		if (NULL == file_transmit || NULL == file_block)
			break;

		file_block_hdr = (file_block_hdr_t*)file_block->block_data_;
		file_transmit_param = &file_transmit->file_transmit_param_;
		
		file_entry = file_entry_allocate();
		if (NULL == file_entry)
			break;

		memset(file_entry, 0, sizeof(*file_entry));
		tls_list_initialize(&file_entry->list_node_);
		tls_list_initialize(&file_entry->list_write_block_);
		file_entry->file_transmit_ = (void*)file_transmit;
		strcpy(file_entry->file_path_, file_transmit->file_transmit_param_.file_dir_);	
		if (file_entry->file_path_[strlen(file_entry->file_path_)-1] != '/')
			strcat(file_entry->file_path_, "/");	
		strcat(file_entry->file_path_, file_block_hdr->filename_);
		strcpy(file_entry->file_name_, file_block_hdr->filename_);

		file_entry->file_size_ = file_block_hdr->filesize_;
		file_entry->file_block_count_ = file_block_hdr->count_;
		
		file_entry->sleepus_ = FILE_ENTRY_READ_SLEEPUS;
		file_entry->intervalus_ = file_transmit_param->file_send_interval_ * 1000;
		file_entry->intervalus_ = 0 == file_entry->intervalus_ ? 1000 : file_entry->intervalus_;

		file_entry->tls_dest_addr_ = file_transmit_param->file_publish_dst_addr_;
		file_entry->tls_dest_entry_ = file_transmit_param->file_publish_dst_entry_;
		strcpy(file_entry->publish_url_, file_transmit_param->file_publish_url_);
	}while(0);

	return file_entry;
}
void file_transmit_delete_file_entry(file_entry_t* file_entry)
{
	assert(file_entry);
	file_entry_free(file_entry);
}
void file_transmit_add_file_entry_list(file_transmit_t* file_transmit, file_entry_t* file_entry)
{
	assert(file_transmit);
	assert(file_entry);

	tls_list_initialize(&file_entry->list_node_);
	tls_list_insert_tail(&file_transmit->queue_file_entry_, &file_entry->list_node_);
}
