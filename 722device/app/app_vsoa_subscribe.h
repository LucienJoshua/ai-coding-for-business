#ifndef __APP_VSOA_SUBSCRIBE_H__
#define __APP_VSOA_SUBSCRIBE_H__


#ifdef __cplusplus
extern "C" {
#endif

#include "tls_com.h"
#include "tls_type.h"
#include "vsoa_platform.h"
#include "vsoa_cliauto.h"
#include "tls_thread.h"
#include "tls_event.h"


	typedef struct __app_vsoa_subscribe_param
	{
		char						server_name_[TLS_VSOA_NAME_SISE];
		char						server_password_[TLS_VSOA_NAME_SISE];

		char						subscribe_url_[TLS_MAX_STR_SIZE];	

		void*						app_subscribe_arg_;
		void						(*on_app_subscribe_connect)(void* arg, int connect);
		void						(*on_app_subscribe_message)(void* arg, vsoa_url_t* url, vsoa_payload_t* payload);
	}app_vsoa_subscribe_param_t;


	typedef struct __app_vsoa_subscribe
	{
		app_vsoa_subscribe_param_t	app_subscribe_param_;

		char*						urls_[32];
		int							url_count_;
		int							app_vsoa_subscribe_start_;

		vsoa_client_t*				app_vsoa_subscribe_client_;
		vsoa_client_auto_t*			app_vsoa_subscribe_client_auto_;
	}app_vsoa_subscribe_t;


	extern app_vsoa_subscribe_t*	app_vsoa_subscribe_allocate(void);
	extern void						app_vsoa_subscribe_free(app_vsoa_subscribe_t* app_subscribe);

	extern TLS_RESULT				app_vsoa_subscribe_start(const app_vsoa_subscribe_param_t* param, app_vsoa_subscribe_t* subscribe);
	extern void						app_vsoa_subscribe_stop(app_vsoa_subscribe_t* subscribe);

#ifdef __cplusplus
}
#endif

#endif // __APP_VSOA_SUBSCRIBE_H__
