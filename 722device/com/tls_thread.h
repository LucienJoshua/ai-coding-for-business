#ifndef __TLS_THREAD_H__
#define __TLS_THREAD_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "tls_com.h"
#include "tls_type.h"

typedef void*(*tls_thread_routine_t)(void* arg);

struct __tls_thread
{
    volatile U32				run_;
    pthread_t					threadid_;
    tls_thread_routine_t        thread_routine_;
    void*						arg_;
};
typedef struct __tls_thread tls_thread_t;

extern tls_thread_t*	tls_thread_allocate(void);

extern void             tls_thread_free(tls_thread_t* thread);

extern TLS_RESULT		tls_thread_start(tls_thread_t* thread);

extern TLS_RESULT		tls_thread_stop(tls_thread_t* thread);

#ifdef __cplusplus
}
#endif // __cplusplus 

#endif // __TLS_THREAD_H__
