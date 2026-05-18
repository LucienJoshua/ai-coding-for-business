#include <stdio.h>
#include <string.h>
#include "tls_com.h"
#include "tls_type.h"
#include "zlog.h"
#include "tls_log.h"

tls_log_t*			g_tls_log = NULL;

tls_log_t* tls_log_allocate(void)
{
	tls_log_t*		log;
	log = (tls_log_t*)malloc(sizeof(tls_log_t));
	return log;
}
void tls_log_free(tls_log_t* log)
{
	if (log)
		free(log);
}

TLS_RESULT tls_log_initialize(tls_log_t* log)
{
	int					ret;
	TLS_RESULT			result;
	int					oldvalue;
	int					newvalue;

	do
	{
		oldvalue = 0;
		newvalue = 1;
		if (__sync_val_compare_and_swap(&log->init_, oldvalue, newvalue))
		{
			result = TLS_RESULT_S_CONTINUE;
			__TLSDBG("tls log has initialized already.\n");
			break;
		}

		ret = zlog_init("/usr/local/bin/zlog.conf");
		if (ret)
		{
			__TLSDBG("ERROR-zlog init failed\n");
			result = TLS_RESULT_E_FAIL;
			break;
		}

		log->zlog_ = (void*)zlog_get_category("my_cat");
		if (NULL == log->zlog_)
		{
			__TLSDBG("ERROR-zlog get category failed\n");
			result = TLS_RESULT_E_FAIL;
			break;
		}

		g_tls_log = log;

		result = TLS_RESULT_S_OK;
		TLSLOG_INFO(log, "Create tls zlog OK");

	}while(0);
	return result;
}
void tls_log_uninitialize(tls_log_t* log)
{
	int					oldvalue;
	int					newvalue;

	if (NULL == log)
		return;

	oldvalue = 1;
	newvalue = 0;
	if (!__sync_val_compare_and_swap(&log->init_, oldvalue, newvalue))
	{
		__TLSDBG("tls loghas uninitializeed already.\n");
		return;
	}

	if (NULL == log->zlog_)
		return;

	g_tls_log = NULL;
	zlog_fini();
	log->zlog_ = NULL;
}
