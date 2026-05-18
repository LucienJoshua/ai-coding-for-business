#include <assert.h>
#include "tls_afx.h"
#include "tls_log.h"
#include "tls_thread.h"

tls_thread_t* tls_thread_allocate(void)
{
    tls_thread_t* thread = NULL;
    thread = (tls_thread_t*)malloc(sizeof(tls_thread_t));
    assert(thread);
    return thread;
}

void tls_thread_free(tls_thread_t* thread)
{
	if (thread)
		free(thread);
}

TLS_RESULT tls_thread_start(tls_thread_t* thread)
{
	int ret = -1;

    if (NULL == thread)
        return TLS_RESULT_E_INVALID_PARAM;
    if (NULL == thread->thread_routine_)
        return TLS_RESULT_E_INVALID_DATA;

    thread->run_ = 1;
    ret = pthread_create(&thread->threadid_, NULL, thread->thread_routine_, thread->arg_);
    if (ret != 0)
    {
        LOG_ERROR("pthread_create call failed, ret:%d.", ret);
        return TLS_RESULT_E_FAIL;
    }

	return TLS_RESULT_S_OK;
}

TLS_RESULT tls_thread_stop(tls_thread_t* thread)
{
    if (NULL == thread)
        return TLS_RESULT_E_INVALID_PARAM;

    thread->run_ = 0;

	if (thread->threadid_ != 0)
	{
		pthread_cancel(thread->threadid_);
		pthread_join(thread->threadid_, NULL);
		thread->threadid_ = 0;
	}

	return TLS_RESULT_S_OK;
}
