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
#include <sys/time.h>	
#include "app_transmit.h"
#include "tls_log.h"
#include "tls_server.h"
#include "file_transmit.h"
#include "file_block.h"
#include "file_entry.h"

static void					file_entry_write_handle(file_entry_t* file_entry);
static void					file_entry_read_handle(file_entry_t* file_entry);

static TLS_RESULT			file_entry_w_file_open(file_entry_t* file_entry);
static int					file_entry_w_file_is_open(file_entry_t* file_entry);
static void					file_entry_w_file_close(file_entry_t* file_entry);
static int					file_entry_w_file_write_block(file_entry_t* file_entry, file_block_t* file_block);

static TLS_RESULT			file_entry_r_file_open(file_entry_t* file_entry);
static int					file_entry_r_file_is_open(file_entry_t* file_entry);
static void					file_entry_r_file_close(file_entry_t* file_entry);
static int					file_entry_r_file_read_data(file_entry_t* file_entry);

static inline U64			file_entry_get_time_value(void);


file_entry_t* file_entry_allocate(void)
{
	file_entry_t*		file_entry;
	file_entry = (file_entry_t*)malloc(sizeof(file_entry_t));
	return file_entry;
}
void file_entry_free(file_entry_t* file_entry)
{
	if (file_entry)
	{
		// file_entry_w_file_close(file_entry);
		free(file_entry);
	}
}

void file_entry_add_file_block(file_entry_t* file_entry, void* block)
{
	file_block_t*			file_block;

	if (NULL == file_entry || NULL == block)
		return;
	file_block = (file_block_t*)block;
	tls_list_initialize(&file_block->block_node_);
	tls_list_insert_tail(&file_entry->list_write_block_, &file_block->block_node_);
}
void file_entry_handle(file_entry_t* file_entry)
{
	file_entry_write_handle(file_entry);
	file_entry_read_handle(file_entry);
}
int file_entry_handle_complete(file_entry_t* file_entry)
{
	assert(file_entry);
	return file_entry->file_complete_status_;
}
void file_entry_show_block_info(void* block)
{
	file_block_t*		file_block;
	file_block_hdr_t*	hdr;

	if (NULL == block)
		return;
	file_block = block;
	hdr = (file_block_hdr_t*)file_block->block_data_;

	LOG_INFO("=============== file block info ===============");
	LOG_INFO("block size:%d, data size:%d", file_block->block_size_, file_block->size_);
	LOG_INFO("flag:0x%04x", hdr->flag_);
	LOG_INFO("block count:%d", hdr->count_);
	LOG_INFO("seqence:%d", hdr->sequence_);
	LOG_INFO("filename:%s", hdr->filename_);
	LOG_INFO("file size:%d", hdr->filesize_);
	LOG_INFO("data size:%d", hdr->size_);
	LOG_INFO("===============================================");
}

void file_entry_write_handle(file_entry_t* file_entry)
{
	TLS_RESULT				result;
	file_transmit_t*		file_transmit;
	file_block_t*			file_block = NULL;
	tls_list_node_t*		list_head;
	tls_list_node_t*		list_node;
	int						ret;

	assert(file_entry);

	if (file_entry->file_size_ == file_entry->file_write_size_ &&
		tls_list_empty(&file_entry->list_write_block_))
	{
		file_entry_w_file_close(file_entry);
		return;
	}

	if (!file_entry_w_file_is_open(file_entry))
	{
		result = file_entry_w_file_open(file_entry);
		if (TlsResultFail(result))
		{
			file_entry->file_complete_status_ = 1;
			LOG_ERROR("ERROR-open write file fail, filename:%s", file_entry->file_name_);
			return;
		}
	}	

	list_head = &file_entry->list_write_block_;	
	if (tls_list_empty(list_head))
		return;

	list_node = tls_list_remove_head(list_head);
	file_block = __LIST_CONTAIN_OF(list_node, file_block_t, block_node_);
	if (file_block)
	{
		file_transmit = (file_transmit_t*)file_entry->file_transmit_;
		ret = file_entry_w_file_write_block(file_entry, file_block);
		if (ret > 0)
		{
			file_entry->file_write_size_ += ret;
			file_entry->file_write_count_++;
		}
		file_transmit_release_file_block(file_transmit, file_block);
	}
}
void file_entry_read_handle(file_entry_t* file_entry)
{
	assert(file_entry);

	if (file_entry->file_read_fd_ > 0 && file_entry->file_read_size_ == file_entry->file_size_)
	{
		LOG_DEBUG("read file finish and complete, close the file");
		file_entry_r_file_close(file_entry);
		file_entry->file_complete_status_ = 1;
		return;
	}

	if (0 == file_entry->file_read_fd_ && 0 == file_entry->file_read_size_ && file_entry->file_size_ > 0)
	{
		file_entry_r_file_open(file_entry);
		file_entry->tv_start_time_ = file_entry_get_time_value();
		file_entry->tv_tick_time_ = file_entry->tv_start_time_;
	}

	if (file_entry->tv_tick_time_ - file_entry->tv_start_time_ > file_entry->intervalus_)
	{	
		file_entry_r_file_read_data(file_entry);
		file_entry->tv_start_time_ = file_entry_get_time_value();
	}
	file_entry->tv_tick_time_ = file_entry_get_time_value();
	usleep(file_entry->sleepus_);
}




TLS_RESULT file_entry_w_file_open(file_entry_t* file_entry)
{
	assert(file_entry);
	file_entry->file_write_fd_ = open(file_entry->file_path_, O_CREAT | O_WRONLY | O_TRUNC, 0777);
	if (file_entry->file_write_fd_ < 0)
	{
		LOG_ERROR("open write file:%s fail", file_entry->file_path_);
		return TLS_RESULT_E_FAIL;
	}
	return TLS_RESULT_S_OK;
}
int file_entry_w_file_is_open(file_entry_t* file_entry)
{
	assert(file_entry);
	return file_entry->file_write_fd_ > 0;
}
void file_entry_w_file_close(file_entry_t* file_entry)
{
	assert(file_entry);
	if (file_entry->file_write_fd_ > 0)
	{
		LOG_DEBUG("On close the write file entry\n");
		close(file_entry->file_write_fd_);
		file_entry->file_write_fd_ = 0;
	}
}
int	file_entry_w_file_write_block(file_entry_t* file_entry, file_block_t* file_block)
{
	int						ret;
	file_block_hdr_t*		file_block_hdr;

	assert(file_entry);
	assert(file_block);

	if (file_entry->file_write_fd_ <= 0)
		return 0;

	file_block_hdr = (file_block_hdr_t*)file_block->block_data_;

#if 0
	__TLSDBG("write: block flag:0x%04x,  count:%d, sequence:%d, filename:%s, block size:%d\n",
			file_block_hdr->flag_, file_block_hdr->count_, file_block_hdr->sequence_, file_block_hdr->filename_, file_block_hdr->size_);
#endif

	ret = write(file_entry->file_write_fd_, file_block_hdr->data_, file_block_hdr->size_);
#if 0
	if (ret > 0)
	{
		__TLSDBG("write file block data, size:%d\n", ret);
	}
#endif
	return ret;	
}


TLS_RESULT file_entry_r_file_open(file_entry_t* file_entry)
{
	assert(file_entry);
	file_entry->file_read_fd_ = open(file_entry->file_path_, O_RDONLY, 0777);
	if (file_entry->file_read_fd_ < 0)
	{
		LOG_ERROR("open read file:%s fail", file_entry->file_path_);
		return TLS_RESULT_E_FAIL;
	}
	return TLS_RESULT_S_OK;
}
int file_entry_r_file_is_open(file_entry_t* file_entry)
{
	assert(file_entry);
	return file_entry->file_read_fd_ > 0;
}
void file_entry_r_file_close(file_entry_t* file_entry)
{
	assert(file_entry);
	if (file_entry->file_read_fd_ > 0)
	{
		LOG_DEBUG("On close the read  file entry");
		close(file_entry->file_read_fd_);
		file_entry->file_write_fd_ = 0;
	}
}
int file_entry_r_file_read_data(file_entry_t* file_entry)
{
	int							ret = 0;
	int							size;
	file_block_hdr_t*			block_head = NULL;

	file_transmit_t*			file_transmit;
	app_transmit_t*				app_transmit;

	tls_server_t*				tls_server;
	vsoa_url_t					vsoa_url;
	vsoa_payload_t				vsoa_payload;
	tls_vsoa_sesstion_t			tls_vsoa_session;
	TLS_RESULT					result = TLS_RESULT_E_FAIL;

	assert(file_entry);
	if (file_entry->file_read_fd_ <= 0)
	{
		LOG_ERROR("ERROR-The file NOT open!");
		return 0;
	}

	file_transmit = (file_transmit_t*)file_entry->file_transmit_;
	app_transmit = (app_transmit_t*)file_transmit->app_transmit_;
	tls_server = (tls_server_t*)app_transmit->tls_server_;

	block_head = (file_block_hdr_t*)file_entry->file_read_buff_;
	memset(block_head, 0, sizeof(file_block_hdr_t));

	size = FILE_DATA_MTU;
	ret = read(file_entry->file_read_fd_, block_head->data_, size);
	if (ret > 0)
	{
		block_head->flag_ = FILE_BLOCK_FLAG;
		block_head->count_ = file_entry->file_block_count_;
		block_head->sequence_ = file_entry->file_read_count_;
		block_head->filesize_ = file_entry->file_size_;
		block_head->size_ = ret;
		strcpy(block_head->filename_, file_entry->file_name_);

		// todo...
		memset(&vsoa_url, 0, sizeof vsoa_url);
		memset(&vsoa_payload, 0, sizeof vsoa_payload);
		memset(&tls_vsoa_session, 0, sizeof tls_vsoa_session);
		tls_vsoa_session.tls_dest_addr_ = file_entry->tls_dest_addr_;
		tls_vsoa_session.tls_dest_entry_ = file_entry->tls_dest_entry_;

		vsoa_url.url = file_entry->publish_url_;
		vsoa_url.url_len = strlen(file_entry->publish_url_);
		vsoa_payload.data = (void*)block_head;
		vsoa_payload.data_len = sizeof(file_block_hdr_t) + ret;

		tls_vsoa_session.vsoa_url_ = &vsoa_url;
		tls_vsoa_session.vsoa_payload_ = &vsoa_payload;
		tls_vsoa_session.tls_priority_ = 0;

		LOG_INFO("tls server publish data, url:%s, data len:%d", file_entry->publish_url_, vsoa_payload.data_len);
		result = tls_server_vsoa_publish_transmit(tls_server, &tls_vsoa_session);
		if (TlsResultFail(result))
		{
			LOG_ERROR("tls server vsoa publish data fail.");
			return -1;
		}

		LOG_DEBUG("Read file's block and send the file size:%d, read count:%ld", block_head->size_,
				file_entry->file_read_count_);
		file_entry->file_read_count_++;
		file_entry->file_read_size_ += ret;
	}
	return ret;
}
inline U64 file_entry_get_time_value(void)
{
	struct timeval		tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1e6 + tv.tv_usec;
}
