#ifndef __APP_TRANSMIT_H__
#define __APP_TRANSMIT_H__

#ifdef __cplusplus
extern "C"{
#endif //__cplusplus

#include "tls_com.h"
#include "tls_type.h"
#include "app_config.h"
#include "tls_config.h"
#include "tls_net_proxy.h"


	typedef struct __app_transmit_entry
	{
		int									initialize_;
		int									runstatus_;

		char								app_path_[256];
		void*								tls_log_;

		app_config_t						app_config_;
		tls_config_t						tls_config_;
		file_config_t						file_config_;

		void*								tls_server_;
		void*								app_rpc_server_;
		void*								app_publish_;
		void*								file_transmit_;

		void*								statis_rate_;

		void*								app_subscribe_;

		net_proxy_t							net_proxy_;
	}app_transmit_t;



	extern TLS_RESULT						api_transmit_initialize(app_transmit_t* app_transmit);
	extern int								api_transmit_running_status(app_transmit_t* app_transmit);
	extern void								api_transmit_poll(app_transmit_t* app_transmit);
	extern TLS_RESULT						api_transmit_uninitialize(app_transmit_t* app_transmit);

	extern void								api_transmit_tls_server_publish_test(app_transmit_t* app_transmit);


	extern app_transmit_t*					api_transmit_instance(void);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif // __APP_TRANSMIT_H__
