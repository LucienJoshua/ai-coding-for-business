#ifndef __APP_VSOA_PUBLISH_H__
#define __APP_VSOA_PUBLISH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tls_com.h"
#include "tls_type.h"
#include "vsoa_server.h"
#include "vsoa_platform.h"
#include "tls_thread.h"
#include "tls_event.h"


	typedef struct __app_vsoa_publish_param
	{
		char						server_name_[TLS_VSOA_NAME_SISE];
		char						server_password_[TLS_VSOA_NAME_SISE];

		char						server_ip_[TLS_MAX_IP_SIZE];
		U16							server_port_;
		char						server_url_[TLS_MAX_STR_SIZE];
	}app_vsoa_publish_param_t;


	typedef struct _app_vsoa_publish
	{
		app_vsoa_publish_param_t	app_publish_param_;
		int							app_vsoa_publish_start_;

		vsoa_server_t*				app_vsoa_server_;

		S32							run_;
		tls_thread_t				thread_app_vsoa_publish_;
		tls_event_t					evt_loop_;
		tls_event_t					evt_stop_;
	}app_vsoa_publish_t;


	extern app_vsoa_publish_t*		app_vsoa_publish_allocate(void);
	extern void						app_vsoa_publish_free(app_vsoa_publish_t* publish);

	extern TLS_RESULT				app_vsoa_publish_start(app_vsoa_publish_param_t* param,  app_vsoa_publish_t* publish);
	extern void						app_vsoa_publish_stop(app_vsoa_publish_t* publish);

	extern TLS_RESULT				app_vsoa_publish_data(app_vsoa_publish_t* publish, const vsoa_url_t* url, const vsoa_payload_t* payload);


#ifdef __cplusplus
}
#endif

#endif // __APP_VSOA_PUBLISH_H__
