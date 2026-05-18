#ifndef __FILE_ENTRY_H__
#define __FILE_ENTRY_H__

#ifdef __cplusplus
extern "C"{
#endif // __cplusplus

#include "tls_list.h"
#include <sys/time.h>

	
	typedef struct __file_entry
	{
		tls_list_node_t			list_node_;
		void*					file_transmit_;
		S8						file_path_[FILE_NAME_SIZE];
		S8						file_name_[FILE_NAME_SIZE];
		U32						file_size_;
		U32						file_block_count_;

		U32						tls_dest_addr_;
		U32						tls_dest_entry_;
		char					publish_url_[FILE_NAME_SIZE];


		tls_list_node_t			list_write_block_;
		int						file_write_fd_;
		U32						file_write_count_;
		U32						file_write_size_;

		int						file_read_fd_;
		U32						file_read_size_;
		U32						file_read_count_;

		U8						file_read_buff_[FILE_DATA_MTU * 2];
		U32						file_read_buff_size_;

		int						file_complete_status_;

		U32						intervalus_;
		U32						sleepus_;

		U64						tv_start_time_;
		U64						tv_tick_time_;
	}file_entry_t;


	extern file_entry_t*		file_entry_allocate(void);
	extern void					file_entry_free(file_entry_t* file_entry);

	extern void					file_entry_add_file_block(file_entry_t* file_entry, void* block);
	extern void					file_entry_handle(file_entry_t* file_entry);
	extern int					file_entry_handle_complete(file_entry_t* file_entry);

	extern void					file_entry_show_block_info(void* block);


#ifdef __cplusplus
}
#endif // __cplusplus


#endif // __FILE_ENTRY_H__
