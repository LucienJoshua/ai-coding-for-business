#ifndef __APP_UDP_ENDPOINT_H__
#define __APP_UDP_ENDPOINT_H__

#ifdef __cplusplus			
extern "C" {
#endif // __cplusplus

	#include "tls_com.h"
	#include "tls_type.h"
	#include "tls_udp_socket.h"
	#include "tls_thread.h"
	
	typedef struct __app_udp_param
	{
		char							local_ip_[32];
		U16								local_port_;
		char							remote_ip_[32];
		U16								remote_port_;
		void*							userdata;
		void							(*udp_endpoint_receive)(void* userdata, PU8 data, U32 size);

		U8								tls_session_id_[16];
		U32								tls_remote_id_;
		U16								tls_remote_sub_id_;
	}app_udp_param_t;

	typedef struct __app_udp_enpoint
	{
		app_udp_param_t					udp_param_;
		tls_udp_socket_t*				udp_socket_;
		tls_thread_t					udp_recv_thread_;
		U8								udp_recv_data_[4096];
		U32								udp_recv_size_;
	}app_udp_endpoint_t;

	extern app_udp_endpoint_t* udp_endpoint_create(const app_udp_param_t* udp_param);

	extern S32	udp_endpoint_send(app_udp_endpoint_t* udp_endpoint, PU8 data, U32 size);

	extern void udp_endpoint_delete(app_udp_endpoint_t* udp_endpoint);

#ifdef __cplusplus
}
#endif // __cplusplus


#endif // __APP_UDP_ENDPOINT_H__
