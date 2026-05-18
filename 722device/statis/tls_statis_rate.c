#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <assert.h>
#include "tls_afx.h"
#include "tls_log.h"
#include "tls_statis_rate.h"



#define __LOCK_TX_INPUT(t)				tls_lock(&((t)->input_lock_))
#define __UNLOCK_TX_INPUT(t)			tls_unlock(&((t)->input_lock_))

#define __LOCK_TX_OUTPUT(t)				tls_lock(&((t)->output_lock_))
#define __UNLOCK_TX_OUTPUT(t)			tls_unlock(&((t)->output_lock_))

#define __LOCK_RX_RATE(r)				tls_lock(&((r)->rx_bytes_lock_))
#define __UNLOCK_RX_RATE(r)				tls_unlock(&((r)->rx_bytes_lock_))


static void			tsr_tx_start_thread(tls_statis_rate_tx_t* tsr_tx);
static void			tsr_tx_stop_thread(tls_statis_rate_tx_t* tsr_tx);
static void*		tsr_tx_work_thread(void* arg);
static void			tsr_tx_input_poll(tls_statis_rate_tx_t* tsr_tx);
static void			tsr_tx_output_poll(tls_statis_rate_tx_t* tsr_tx);

static void			tsr_rx_start_thread(tls_statis_rate_rx_t* tsr_rx);
static void			tsr_rx_stop_thread(tls_statis_rate_rx_t* tsr_rx);
static void*		tsr_rx_work_thread(void* arg);
static void			tsr_rx_thread_poll(tls_statis_rate_rx_t* tsr_rx);

static inline U64	tsr_get_time_value(void);
static inline void	tsr_get_rate_text(char* text, U64 rate);


static tls_statis_rate_tx_t*	s_tsr_tx = NULL;
static tls_statis_rate_rx_t*	s_tsr_rx = NULL;


tls_statis_rate_t*	tls_statis_rate_allocate(void)
{
	tls_statis_rate_t*	tsr = NULL;
	tsr = (tls_statis_rate_t*)malloc(sizeof(tls_statis_rate_t));
	return tsr;
}
void tls_statis_rate_free(tls_statis_rate_t* tsr)
{
	if (tsr)
		free(tsr);
}
TLS_RESULT tls_statis_rate_start(tls_statis_rate_t* tsr)
{
	TLS_RESULT						result;
	int								oldvalue;
	int								newvalue;

	tls_statis_rate_tx_t*			tsr_tx = NULL;
	tls_statis_rate_rx_t*			tsr_rx = NULL;

	do
	{
		if (NULL == tsr)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			LOG_ERROR("ERROR-file transmit invalid parameters.");
			break;
		}
		oldvalue = 0;
		newvalue = 1;
		if (__sync_val_compare_and_swap(&tsr->start_, oldvalue, newvalue))
		{
			result = TLS_RESULT_S_CONTINUE;
			LOG_ERROR("rate statis has already started.");
			break;
		}
		tsr_tx = &tsr->tsr_tx_;
		tsr_rx = &tsr->tsr_rx_;
		memset(tsr_tx, 0, sizeof(*tsr_tx));
		memset(tsr_rx, 0, sizeof(*tsr_rx));
		tsr_tx->parent_ = tsr;
		tsr_rx->parent_ = tsr;

		tls_lock_init(&tsr_tx->input_lock_);
		tls_lock_init(&tsr_tx->output_lock_);

		tls_event_initialize(&tsr_tx->tsr_tx_evt_loop_);	
		tls_event_initialize(&tsr_tx->tsr_tx_evt_stop_);	

		tls_lock_init(&tsr_rx->rx_bytes_lock_);
		tls_event_initialize(&tsr_rx->tsr_rx_evt_loop_);
		tls_event_initialize(&tsr_rx->tsr_rx_evt_stop_);

		tsr_tx->input_tx_interval_ = 1000;
		tsr_tx->output_tx_interval_ = 1000;
		tsr_rx->rx_interval_ = 1000;

		s_tsr_tx = tsr_tx;
		s_tsr_rx = tsr_rx;

		tsr_tx_start_thread(tsr_tx);
		tsr_rx_start_thread(tsr_rx);

		result = TLS_RESULT_S_OK;
		
	}while(0);
	if (TlsResultFail(result))
	{
		tls_statis_rate_stop(tsr);
	}
	return result;
}
void tls_statis_rate_stop(tls_statis_rate_t* tsr)
{
	int								oldvalue;
	int								newvalue;

	tls_statis_rate_tx_t*			tsr_tx = NULL;
	tls_statis_rate_rx_t*			tsr_rx = NULL;

	if (NULL == tsr)
		return;

	oldvalue = 1;
	newvalue = 0;
	if (!__sync_val_compare_and_swap(&tsr->start_, oldvalue, newvalue))
	{
		LOG_INFO("statis rate has  already stopd.");
		return;
	}

	s_tsr_tx = NULL;
	s_tsr_rx = NULL;

	tsr_tx = &tsr->tsr_tx_;
	tsr_rx = &tsr->tsr_rx_;

	tsr_tx_stop_thread(&tsr_tx);
	tsr_rx_stop_thread(&tsr_rx);

	tls_event_uninitialize(&tsr_tx->tsr_tx_evt_loop_);
	tls_event_uninitialize(&tsr_tx->tsr_tx_evt_stop_);

	tls_event_uninitialize(&tsr_rx->tsr_rx_evt_loop_);
	tls_event_uninitialize(&tsr_rx->tsr_rx_evt_stop_);

	tls_lock_uninit(&tsr_tx->input_lock_);
	tls_lock_uninit(&tsr_tx->output_lock_);

	tls_lock_uninit(&tsr_rx->rx_bytes_lock_);

}

void tls_statis_rate_tx_input(U64 bytes)
{
	if (s_tsr_tx)
	{
		__LOCK_TX_INPUT(s_tsr_tx);
		s_tsr_tx->input_tx_bytes_ += bytes;
		__UNLOCK_TX_INPUT(s_tsr_tx);
	}
}
void tls_statis_rate_tx_output(U64 bytes)
{
	if (s_tsr_tx)
	{
		__LOCK_TX_OUTPUT(s_tsr_tx);
		s_tsr_tx->output_tx_bytes_ += bytes;
		__UNLOCK_TX_OUTPUT(s_tsr_tx);
	}
}

void tls_statis_rate_rx(U64 bytes)
{
	if (s_tsr_rx)
	{
		__LOCK_RX_RATE(s_tsr_rx);
		s_tsr_rx->rx_bytes_ += bytes;
		__UNLOCK_RX_RATE(s_tsr_rx);
	}
}
U64 tls_statis_rate_get_tx_input(void)
{
	return s_tsr_tx ? s_tsr_tx->input_tx_rate_ : 0;
}
U64 tls_statis_rate_get_tx_output(void)
{
	return s_tsr_tx ? s_tsr_tx->output_tx_rate_ : 0;
}
U64 tls_statis_rate_get_rx(void)
{
	return s_tsr_rx ? s_tsr_rx->rx_rate_ : 0;
}
void tls_statis_rate_get_text(char* text, U64 rate)
{
	assert(text);

	if (rate < 1e3)
	{
		sprintf(text, "%lld Bps", rate);
	}
	else if (rate >= 1e3 && rate < 1e6)
	{
		sprintf(text, "%.2f KBps", rate / 1000.0);
	}
	else if (rate >= 1e6 && rate < 1e9)
	{
		sprintf(text, "%.2f MBps", rate / 1000000.0);
	}
	else 
	{
		sprintf(text, "%.2f GBps", rate / 1000000000.0);
	}
}

void tsr_tx_start_thread(tls_statis_rate_tx_t* tsr_tx)
{
	if (NULL == tsr_tx)
		return;

	memset(&tsr_tx->tsr_tx_thread_, 0, sizeof(tsr_tx->tsr_tx_thread_));
	tsr_tx->tsr_tx_thread_.arg_ = (void*)tsr_tx;
	tsr_tx->tsr_tx_thread_.thread_routine_ =  tsr_tx_work_thread;
	tsr_tx->tsr_tx_run_ = 1;
	tls_event_set(&tsr_tx->tsr_tx_evt_loop_);
	tls_thread_start(&tsr_tx->tsr_tx_thread_);
}
void tsr_tx_stop_thread(tls_statis_rate_tx_t* tsr_tx)
{
	if (NULL == tsr_tx)
		return;

	tsr_tx->tsr_tx_run_ = 0;
	tls_event_set(&tsr_tx->tsr_tx_evt_loop_);
	tls_event_timewait(&tsr_tx->tsr_tx_evt_stop_, 3000);
	tls_thread_stop(&tsr_tx->tsr_tx_thread_);

}
void* tsr_tx_work_thread(void* arg)
{
	tls_statis_rate_tx_t*	tsr_tx;

	if (NULL == arg)
		return NULL;

	tsr_tx = (tls_statis_rate_tx_t*)arg;

	tls_event_reset(&tsr_tx->tsr_tx_evt_stop_);
	tsr_tx->input_tv_tick_ = tsr_get_time_value();
	tsr_tx->output_tv_tick_ = tsr_get_time_value();
	while (tsr_tx->tsr_tx_run_)
	{
		tls_event_wait(&tsr_tx->tsr_tx_evt_loop_);
		if (0 == tsr_tx->tsr_tx_run_)
			break;

		tsr_tx_input_poll(tsr_tx);
		tsr_tx_output_poll(tsr_tx);
		usleep(10);
	}
	tls_event_set(&tsr_tx->tsr_tx_evt_stop_);
	return NULL;
}
void tsr_tx_input_poll(tls_statis_rate_tx_t* tsr_tx)
{
	if (tsr_tx->input_tv_tick_- tsr_tx->input_tv_start_ > tsr_tx->input_tx_interval_ * 1000)
	{
		__LOCK_TX_INPUT(tsr_tx);
		tsr_tx->input_tx_rate_ = tsr_tx->input_tx_bytes_ * 1000.0 / tsr_tx->input_tx_interval_;
		tsr_tx->input_tx_bytes_ = 0;
		__UNLOCK_TX_INPUT(tsr_tx);

		tsr_tx->input_tv_start_ = tsr_get_time_value();
	}
	tsr_tx->input_tv_tick_ = tsr_get_time_value();
}
void tsr_tx_output_poll(tls_statis_rate_tx_t* tsr_tx)
{
	char output_rate_text[256];

	if (tsr_tx->output_tv_tick_- tsr_tx->output_tv_start_ > tsr_tx->output_tx_interval_ * 1000)
	{
		__LOCK_TX_OUTPUT(tsr_tx);
		tsr_tx->output_tx_rate_ = tsr_tx->output_tx_bytes_ * 1000 / tsr_tx->output_tx_interval_;
		tsr_tx->output_tx_bytes_ = 0;
		__UNLOCK_TX_OUTPUT(tsr_tx);

#if 0
		memset(output_rate_text, 0, sizeof(output_rate_text));
		tsr_get_rate_text(output_rate_text, tsr_tx->output_tx_rate_);
		printf("output rate:%s\n", output_rate_text);
#endif	
		tsr_tx->output_tv_tick_ = tsr_tx->output_tv_start_ = tsr_get_time_value();
	}
	tsr_tx->output_tv_tick_ = tsr_get_time_value();
}

void tsr_rx_start_thread(tls_statis_rate_rx_t* tsr_rx)
{
	if (NULL == tsr_rx)
		return;

	memset(&tsr_rx->tsr_rx_thread_, 0, sizeof(tsr_rx->tsr_rx_thread_));
	tsr_rx->tsr_rx_thread_.arg_ = (void*)tsr_rx;
	tsr_rx->tsr_rx_thread_.thread_routine_ =  tsr_rx_work_thread;
	tsr_rx->tsr_rx_run_ = 1;
	tls_event_set(&tsr_rx->tsr_rx_evt_loop_);
	tls_thread_start(&tsr_rx->tsr_rx_thread_);
}
void tsr_rx_stop_thread(tls_statis_rate_rx_t* tsr_rx)
{
	if (NULL == tsr_rx)
		return;

	tsr_rx->tsr_rx_run_ = 0;
	tls_event_set(&tsr_rx->tsr_rx_evt_loop_);
	tls_event_timewait(&tsr_rx->tsr_rx_evt_stop_, 3000);
	tls_thread_stop(&tsr_rx->tsr_rx_thread_);
}
void* tsr_rx_work_thread(void* arg)
{
	tls_statis_rate_rx_t*	tsr_rx;

	if (NULL == arg)
		return NULL;

	tsr_rx = (tls_statis_rate_rx_t*)arg;

	tls_event_reset(&tsr_rx->tsr_rx_evt_stop_);
	tsr_rx->tv_tick_ = tsr_get_time_value();
	while (tsr_rx->tsr_rx_run_)
	{
		tls_event_wait(&tsr_rx->tsr_rx_evt_loop_);
		if (0 == tsr_rx->tsr_rx_run_)
			break;

		tsr_rx_thread_poll(tsr_rx);
		usleep(10);
	}
	tls_event_set(&tsr_rx->tsr_rx_evt_stop_);
	return NULL;
}
void tsr_rx_thread_poll(tls_statis_rate_rx_t* tsr_rx)
{
	if (tsr_rx->tv_tick_- tsr_rx->tv_start_ > tsr_rx->rx_interval_ * 1000)
	{
		__LOCK_RX_RATE(tsr_rx);
		tsr_rx->rx_rate_ = tsr_rx->rx_bytes_ * 1000.0 / tsr_rx->rx_interval_;
		tsr_rx->rx_bytes_ = 0;
		__UNLOCK_RX_RATE(tsr_rx);

		tsr_rx->tv_start_ = tsr_get_time_value();
	}
	tsr_rx->tv_tick_ = tsr_get_time_value();
}

inline U64	tsr_get_time_value(void)
{
	struct timeval		tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1e6 + tv.tv_usec;
}
inline void	tsr_get_rate_text(char* text, U64 rate)
{
	assert(text);

	if (rate < 1e3)
	{
		sprintf(text, "%lld Bps", rate);
	}
	else if (rate >= 1e3 && rate < 1e6)
	{
		sprintf(text, "%.2f KBps", rate / 1000.0);
	}
	else if (rate >= 1e6 && rate < 1e9)
	{
		sprintf(text, "%.2f MBps", rate / 1000000.0);
	}
	else 
	{
		sprintf(text, "%.2f GBps", rate / 1000000000.0);
	}
}
