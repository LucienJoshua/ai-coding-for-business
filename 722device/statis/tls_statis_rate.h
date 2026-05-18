#ifndef __TLS_RATE_STATIS_H__
#define __TLS_RATE_STATIS_H__


#ifdef __cplusplus
extern "C"
{
#endif

#include "tls_com.h"
#include "tls_type.h"
#include "tls_thread.h"
#include "tls_event.h"
#include "tls_lock.h"

	typedef struct __tls_statis_rate_tx
	{
		void*					parent_;

		U64						input_tx_bytes_;
		U64						input_tx_rate_;
		tls_lock_t				input_lock_;
		U32						input_tx_interval_; // 单位: ms
		U64						input_tv_start_;
		U64						input_tv_tick_;

		U64						output_tx_bytes_;
		U64						output_tx_rate_;
		tls_lock_t				output_lock_;
		U32						output_tx_interval_;
		U64						output_tv_start_;
		U64						output_tv_tick_;

		S32						tsr_tx_run_;
		tls_thread_t			tsr_tx_thread_;
		tls_event_t				tsr_tx_evt_loop_;
		tls_event_t				tsr_tx_evt_stop_;
	}tls_statis_rate_tx_t;

	typedef struct __tls_statis_rate_rx
	{
		void*					parent_;

		U64						rx_bytes_;
		U64						rx_rate_;
		tls_lock_t				rx_bytes_lock_;

		U32						rx_interval_;
		U64						tv_start_;
		U64						tv_tick_;

		S32						tsr_rx_run_;
		tls_thread_t			tsr_rx_thread_;
		tls_event_t				tsr_rx_evt_loop_;
		tls_event_t				tsr_rx_evt_stop_;
	}tls_statis_rate_rx_t;


	typedef struct __tls_statis_rate 
	{
		S32						start_;
		tls_statis_rate_tx_t	tsr_tx_;
		tls_statis_rate_rx_t	tsr_rx_;
	}tls_statis_rate_t;

	extern tls_statis_rate_t*	tls_statis_rate_allocate(void);
	extern void					tls_statis_rate_free(tls_statis_rate_t* tsr);
	extern TLS_RESULT			tls_statis_rate_start(tls_statis_rate_t* tsr);
	extern void					tls_statis_rate_stop(tls_statis_rate_t* tsr);

	extern void					tls_statis_rate_tx_input(U64 bytes);
	extern void					tls_statis_rate_tx_output(U64 bytes);

	extern void					tls_statis_rate_rx(U64 bytes);

	extern U64					tls_statis_rate_get_tx_input(void);
	extern U64					tls_statis_rate_get_tx_output(void);

	extern U64					tls_statis_rate_get_rx(void);

	extern void					tls_statis_rate_get_text(char* text, U64 rate);

#ifdef __cplushplus
}
#endif

#endif // __TLS_RATE_STATIS_H__
