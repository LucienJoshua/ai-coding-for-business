#ifndef __TLS_LOCK_H__
#define __TLS_LOCK_H__


#define __TLS_LOCK(tls_locker)		tls_lock(&(tls_locker));
#define __TLS_UNLOCK(tls_locker)	tls_unlock(&(tls_locker));

struct tls_lock
{
	pthread_mutex_t			mutex_;
};

typedef struct tls_lock tls_lock_t;

extern void		tls_lock_init(tls_lock_t* lock);
extern void		tls_lock_uninit(tls_lock_t* lock);

extern void		tls_lock(tls_lock_t* lock);
extern void		tls_unlock(tls_lock_t* lock);

#endif // __TLS_LOCK_H__
