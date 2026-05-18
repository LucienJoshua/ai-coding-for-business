#ifndef _APP_TRANSPORT_H__
#define __APP_TRANSPORT_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


#include "tls_com.h"
#include "tls_type.h"



	extern TLS_RESULT			api_transport_initialize(void);

	extern void					api_transport_uninitialize(void);

	extern void					api_transport_tls_publish_test(void);

	extern void					api_transport_app_publish_test(void);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __APP_TRANSPORT_H__
