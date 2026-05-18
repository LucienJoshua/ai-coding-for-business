#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "tls_statis.h"

tls_statis_t* tls_statis_allocate(void)
{
	tls_statis_t*	s = NULL;
	s = (tls_statis_t*)malloc(sizeof(tls_statis_t));
	return s;
}
void tls_statis_free(tls_statis_t* statis)
{
	if (statis != NULL)
	{
		free(statis);
	}
}
	
TLS_RESULT tls_statis_initialize(tls_statis_t* statis)
{
	if (NULL == statis)
		return TLS_RESULT_E_INVALID_PARAM;

	tls_statis_clean(statis);
	return TLS_RESULT_S_OK;
}
TLS_RESULT tls_statis_clean(tls_statis_t* statis)
{
	if (NULL == statis)
		return TLS_RESULT_E_INVALID_PARAM;
	memset(statis, 0, sizeof(*statis));
	return TLS_RESULT_S_OK;
}
void tls_statis_uninitialize(tls_statis_t* statis)
{
	if (NULL == statis)
		return;

	tls_statis_clean(statis);
}
void tls_statis_app_tx(tls_statis_t* statis, U64 app_tx_bytes)
{
	// assert(statis);
	if (NULL == statis)
		return;
	statis->app_tx_bytes_ += app_tx_bytes;
	statis->app_tx_count_++;
}
void tls_statis_app_rx(tls_statis_t* statis, U64 app_rx_bytes)
{
	if (NULL == statis)
		return;
	assert(statis);
	statis->app_rx_bytes_ += app_rx_bytes;
	statis->app_rx_count_++;
}
void tls_statis_tls_tx(tls_statis_t* statis, U64 tls_tx_bytes)
{
	// assert(statis);
	if (NULL == statis)
		return;
	statis->tls_tx_bytes_ += tls_tx_bytes;
	statis->tls_tx_count_++;
}
void tls_statis_tls_rx(tls_statis_t* statis, U64 tls_rx_bytes)
{
	assert(statis);
	statis->tls_rx_bytes_ += tls_rx_bytes;
	statis->tls_rx_count_++;
}
