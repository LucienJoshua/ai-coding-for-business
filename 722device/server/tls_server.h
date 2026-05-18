#ifndef __TLS_SERVER_H__
#define __TLS_SERVER_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "tls_com.h"
#include "tls_type.h"
#include "tls_list.h"
#include "tls_thread.h"
#include "tls_event.h"
#include "tls_lock.h"
#include "tls_udp_socket.h"
#include "tls_buff_queue.h"
#include "vsoa_platform.h"
#include "vsoa_server.h"

	struct tls_queue_param
	{
		U32								priority_;
		U32								buff_size_;
		U32								buff_count_;
	};

	struct tls_vsoa_subscribe_param
	{
		char							server_name_[TLS_VSOA_NAME_SISE];
		char							server_password_[TLS_VSOA_NAME_SISE];
		char							subscribe_url_[TLS_VSOA_MAX_STR_SIZE];

		void*							arg;
		void							(*on_subscribe_connect)(void* arg, int connect);
		void							(*on_subscribe_message)(void* arg, vsoa_url_t* url, vsoa_payload_t* payload);
		void							(*on_subscribe_receive_data)(void* arg, PU8 data, U32 size);
	};
	struct tls_vsoa_publish_param
	{
		char							server_name_[TLS_VSOA_NAME_SISE];
		char							server_password_[TLS_VSOA_NAME_SISE];
		char							server_ip_[TLS_MAX_IP_SIZE];
		U16								server_port_;
		char							url_[TLS_MAX_STR_SIZE];
	};
	struct tls_param
	{
		S8								local_ip_[TLS_MAX_IP_SIZE];
		U16								local_port_;
		S8								remote_ip_[TLS_MAX_IP_SIZE];
		U16								remote_port_;

		U32								tls_local_id_;
		U16								tls_local_sub_id_;

		void*							priv_data_;
		void							(*on_tls_register_callback)(void* priv_dta, U32 register_status);
		TLS_RESULT						(*on_tls_receive_callback)(void* priv_data, PU8 data, U32 size);

		struct tls_queue_param			queue_param_[TLS_QUEUE_COUNT];
		struct tls_vsoa_subscribe_param	subscribe_param_;
		struct tls_vsoa_publish_param	publish_param_;
	};


	struct tls_server
	{
		struct tls_param				svr_param_;

		S32								init_;
		S32								start_;

		tls_udp_socket_t*				udp_socket_;

		U32								send_buff_count_;
		U32								recv_buff_count_;
		U32								buff_size_;

		tls_buff_queue_t				queue_send_buff_;
		tls_buff_queue_t				queue_recv_buff_;

		tls_buff_queue_t				queue_transmit_buff_[TLS_QUEUE_MAX_COUNT];

		void*							tls_transmit_;
		void*							tls_receive_;

		S32								work_run_;
		tls_thread_t					thread_work_;
		tls_event_t						evt_work_loop_;
		tls_event_t						evt_work_stop_;

		U16								sequence_;

		void*							tls_statis_;

		void*							tls_vsoa_subscribe_;
		void*							tls_vsoa_publish_;

		void*							tls_request_;
	};

	struct tls_session
	{
		S8								tls_session_id_[16];
		U32								tls_remote_id_;
		U16								tls_remote_sub_id_;
		U8								tls_priority_;
	};

	struct tls_vsoa_session
	{
		U32								tls_dest_addr_;
		U16								tls_dest_entry_;
		vsoa_url_t*						vsoa_url_;
		vsoa_payload_t*					vsoa_payload_;
		U8								tls_priority_;
	};

	typedef struct tls_param			tls_param_t;
	typedef struct tls_queue_param		tle_queue_param_t;
	typedef struct tls_server			tls_server_t;
	typedef struct tls_session			tls_session_t;
	typedef struct tls_vsoa_session		tls_vsoa_sesstion_t;


	extern tls_server_t*	tls_server_allocate(void);
	extern void				tls_server_free(tls_server_t* server);

	extern TLS_RESULT		tls_server_initialize(tls_server_t* server, const tls_param_t* param);
	extern TLS_RESULT		tls_server_uninitialize(tls_server_t* server);

	extern TLS_RESULT		tls_server_start(tls_server_t* server);
	extern TLS_RESULT		tls_server_stop(tls_server_t* server);
	
	extern void				tls_server_active(tls_server_t* server);
	extern void				tls_server_deactive(tls_server_t* server);

	extern tls_buff_t*		tls_server_acquire_send_buff(tls_server_t* server);
	extern tls_buff_t*		tls_server_acquire_recv_buff(tls_server_t* server);
	extern tls_buff_t*		tls_server_acquire_transmit_buff(tls_server_t* server, U8 priority);
	extern void				tls_server_release_buff(tls_server_t* server, tls_buff_t* buff);

	extern TLS_RESULT		tls_server_register(tls_server_t* server, U32 id, U16 subid);
	extern TLS_RESULT		tls_server_transmit(tls_server_t* server, const tls_session_t* session,PU8 data, U32 size);
	extern TLS_RESULT		tls_server_parsedata(tls_server_t* server, tls_buff_t* buff);


	extern TLS_RESULT		tls_server_vsoa_publish(tls_server_t* server, const vsoa_url_t* url, const vsoa_payload_t* payload, U8 priority);
	extern TLS_RESULT		tls_server_vsoa_publish_overtls(tls_server_t* server, const vsoa_url_t* url, const vsoa_payload_t* payload, U8 priority);
	extern TLS_RESULT		tls_server_vsoa_publish_transmit(tls_server_t* server, const tls_vsoa_sesstion_t* session);

	extern TLS_RESULT		tls_server_vsoa_publish_transmit_for_722(tls_server_t* server, const tls_vsoa_sesstion_t* session);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __TLS_SERVER_H__
