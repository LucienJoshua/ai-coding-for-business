#ifndef __TLS_VSOA_PUBLISH_H__
#define __TLS_VSOA_PUBLISH_H__

#ifdef __cplusplus
extern "C"{
#endif

#include "tls_com.h"
#include "tls_type.h"
#include "vsoa_server.h"
#include "vsoa_platform.h"
#include "tls_thread.h"


	typedef struct __tls_vsoa_publish
	{
		char						server_name_[TLS_VSOA_NAME_SISE];
		char						server_password_[TLS_VSOA_NAME_SISE];

		char						server_ip_[TLS_MAX_IP_SIZE];
		U16							server_port_;

		char						publish_url_[TLS_MAX_STR_SIZE];


		void*						tls_server_;

		int							vsoa_start_;
		vsoa_server_t*				vsoa_server_;

		S32							run_;
		tls_thread_t				thread_server_;
		tls_event_t					evt_loop_;
		tls_event_t					evt_stop_;

	}tls_vsoa_publish_t;


	extern tls_vsoa_publish_t*		vsoa_publish_allocate(void);
	extern void						vsoa_publish_free(tls_vsoa_publish_t* publish);

	extern TLS_RESULT				vsoa_publish_start(tls_vsoa_publish_t* publish);
	extern void						vsoa_publish_stop(tls_vsoa_publish_t* publish);

	extern int						vsoa_publish_data(tls_vsoa_publish_t* publish, PU8 data, U32 size);

#ifdef __cplusplus
}
#endif


#endif // __TLS_VSOA_PUBLISH_H__
