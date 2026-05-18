#include "tls_afx.h"
#include "tls_lock.h"

#define __TLS_LOCK_PARAMETER(lock) \
	if (NULL == (lock)) \
		return;

void tls_lock_init(tls_lock_t* lock)
{
	__TLS_LOCK_PARAMETER(lock);
	pthread_mutex_init(&lock->mutex_, NULL);
}
void tls_lock_uninit(tls_lock_t* lock)
{
	__TLS_LOCK_PARAMETER(lock);
	pthread_mutex_destroy(&lock->mutex_);
}

void tls_lock(tls_lock_t* lock)
{
	__TLS_LOCK_PARAMETER(lock);
	pthread_mutex_lock(&lock->mutex_);
}
void tls_unlock(tls_lock_t* lock)
{
	__TLS_LOCK_PARAMETER(lock);
	pthread_mutex_unlock(&lock->mutex_);
}
