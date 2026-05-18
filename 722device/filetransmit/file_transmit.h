#ifndef	__FILE_TRANSMIT_H__
#define __FILE_TRANSMIT_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "tls_com.h"
#include "tls_type.h"
#include "tls_list.h"
#include "tls_lock.h"
#include "tls_thread.h"
#include "tls_event.h"
#include "app_vsoa_subscribe.h"


	typedef struct file_transmit_param
	{
		S8									file_dir_[128];
		U32									file_block_count_;
		U32									file_block_size_;

		S8									file_subscribe_name_[32];
		S8									file_subscribe_pass_[32];
		S8									file_subscribe_url_[128];

		U32									file_send_interval_;
		U32									file_publish_dst_addr_;
		U32									file_publish_dst_entry_;
		S8									file_publish_url_[128];
	}file_transmit_param_t;


	typedef struct file_transmit
	{
		void*								app_transmit_;
		file_transmit_param_t				file_transmit_param_;

		S32									file_transmit_init_;
		S32									file_transmit_start_;

		app_vsoa_subscribe_t*				file_vsoa_subscribe_;
		volatile S32						file_vsoa_connect_;

		void*								file_consume_;

		tls_list_node						queue_alloc_block_;
		tls_lock_t							queue_alloc_block_lock_;

		tls_list_node						queue_file_block_;
		tls_lock_t							queue_file_block_lock_;

		tls_list_node						queue_file_entry_;

		S32									thread_run_;
		tls_thread_t						thread_work_;
		tls_event_t							evt_thread_loop_;
		tls_event_t							evt_thread_stop_;
	}file_transmit_t;


	extern file_transmit_t*					file_transmit_allocate(void);
	extern void								file_transmit_free(file_transmit_t* file_transmit);

	extern TLS_RESULT						file_transmit_initialize(const file_transmit_param_t* param, file_transmit_t* file_transmit);
	extern void								file_transmit_uninitialize(file_transmit_t* file_transmit);

	extern TLS_RESULT						file_transmit_start_service(file_transmit_t* file_transmit);
	extern void								file_transmit_stop_service(file_transmit_t* file_transmit);

	extern void								file_transmit_release_file_block(file_transmit_t* file_transmit, void* file_block);

#ifdef __cplusplus
}
#endif

#endif // __FILE_TRANSMIT_H__
