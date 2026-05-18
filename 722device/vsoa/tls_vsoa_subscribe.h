#ifndef __TLS_VSOA_SUBSCRIBE_H__
#define __TLS_VSOA_SUBSCRIBE_H__

#ifdef __cplusplus
extern "C"{
#endif // __cplusplus


#include "tls_com.h"
#include "tls_type.h"
#include "vsoa_cliauto.h"


	typedef struct tls_vsoa_subscribe
	{
		char						server_name_[TLS_VSOA_NAME_SISE];
		char						server_password_[TLS_VSOA_NAME_SISE];
		char						subscribe_url_[TLS_VSOA_MAX_STR_SIZE]; 
		char*						urls_[TLS_MAX_STR_SIZE];
		int							url_count_;
		void*						arg_;

		void						(*on_subscribe_connect)(void* arg, int connect);
		void						(*on_subscribe_message)(void* arg, vsoa_url_t* url, vsoa_payload_t* payload);
		void						(*on_subscribe_receive_data)(void* arg, PU8 data, U32 size);

		void*						tls_server_;

		int							vsoa_start_;
		vsoa_client_t*				vsoa_client_;
		vsoa_client_auto_t*			vsoa_client_auto_;
	}tls_vsoa_subscribe_t;


	tls_vsoa_subscribe_t*			vsoa_subscribe_allocate(void);
	void							vsoa_subscribe_free(tls_vsoa_subscribe_t* subscribe);

	TLS_RESULT						vsoa_subscribe_start(tls_vsoa_subscribe_t* subscribe);
	void							vsoa_subscribe_stop(tls_vsoa_subscribe_t* subscribe);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __TLS_VSOA_SUBSCRIBE_H__
