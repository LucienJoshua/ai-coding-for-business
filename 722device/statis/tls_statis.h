#ifndef __TLS_STATIS_H__
#define __TLS_STATIS_H__

#ifdef __cplusplus
extern "C"
{
#endif

	#include "tls_com.h"
	#include "tls_type.h"
	#include "tls_lock.h"
	
	typedef struct __tls_statis
	{
		U64							app_rx_count_;			/* 应用层接收次数 */
		U64							app_rx_bytes_;			/* 应用层接收字节数 */
	
		U64							tls_rx_count_;			/* 传输层接收次数 */
		U64							tls_rx_bytes_;			/* 传输层接收字节数 */
	
		U64							app_tx_count_;			/* 应用层发送次数 */
		U64							app_tx_bytes_;			/* 应用层发送字节数 */
	
		U64							tls_tx_count_;			/* 传输层发送次数 */					
		U64							tls_tx_bytes_;			/* 传输层发送字节数 */	
	}tls_statis_t;
	
	extern tls_statis_t*			tls_statis_allocate(void);
	extern void						tls_statis_free(tls_statis_t* statis);
	
	extern TLS_RESULT				tls_statis_initialize(tls_statis_t* statis);
	extern TLS_RESULT				tls_statis_clean(tls_statis_t* statis);
	extern void						tls_statis_uninitialize(tls_statis_t* statis);

	extern void						tls_statis_app_tx(tls_statis_t* statis, U64 app_tx_bytes);
	extern void						tls_statis_app_rx(tls_statis_t* statis, U64 app_rx_bytes);
	extern void						tls_statis_tls_tx(tls_statis_t* statis, U64 tls_tx_bytes);
	extern void						tls_statis_tls_rx(tls_statis_t* statis, U64 tls_rx_bytes);

#ifdef __cplusplus
}
#endif

#endif // __TLS_STATIS_H__
