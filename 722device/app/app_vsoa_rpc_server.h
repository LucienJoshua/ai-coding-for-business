#ifndef __APP_VSOA_RPC_SERVER_H__
#define __APP_VSOA_RPC_SERVER_H__


#ifdef __cplusplus
extern "C" {
#endif

#include "tls_com.h"
#include "tls_type.h"
#include "vsoa_server.h"
#include "vsoa_platform.h"
#include "tls_thread.h"
#include "tls_event.h"


	typedef void (*rpc_server_command_t)(void* arg, vsoa_url_t* url, vsoa_payload_t* payload);


	typedef struct __app_rpc_server_param
	{
		char						server_name_[TLS_VSOA_NAME_SISE];
		char						server_password_[TLS_VSOA_NAME_SISE];

		char						server_ip_[TLS_MAX_IP_SIZE];
		U16							server_port_;

		char						listener_url_[TLS_MAX_STR_SIZE];  /* 目前只维护一个url, 后续拓展多个url */

		void*						rpc_server_arg_;
		rpc_server_command_t		rpc_server_command_;


	}app_rpc_server_param_t;



	typedef struct __app_vsoa_rpc_server
	{
		int							rpc_server_start_;
		app_rpc_server_param_t		rpc_server_param_;
		vsoa_server_t*				vsoa_rpc_server_;

		S32							run_;
		tls_thread_t				thread_rpc_server_;
		tls_event_t					evt_loop_;
		tls_event_t					evt_stop_;
	}app_vsoa_rpc_server_t;


	extern app_vsoa_rpc_server_t*	app_vsoa_rpc_server_allocate(void);
	extern void						app_vsoa_rpc_server_free(app_vsoa_rpc_server_t* rpc_server);

	extern TLS_RESULT				app_vsoa_rpc_server_start(const app_rpc_server_param_t* rpc_server_param, app_vsoa_rpc_server_t* rpc_server);
	extern void						app_vsoa_rpc_server_stop(app_vsoa_rpc_server_t* rpc_server);


#ifdef __cplusplus
}
#endif


#endif // __APP_VSOA_RPC_SERVER_H__
