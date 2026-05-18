#ifndef __TLS_EVENT_H__
#define __TLS_EVENT_H__

#ifdef __cplusplus
extern "C"{
#endif // __cplusplus


#include <unistd.h>
#include <pthread.h>

#define TLS_EVENT_STATE_PASSIVE	(0)
#define TLS_EVENT_STATE_ACTIVE	(1)

	struct tls_event
	{
		int					fds_[2];
		pthread_mutex_t		mutex_;
		int					auto_reset_;
		int					state_;
	};

	typedef struct tls_event tls_event_t;

	extern void			tls_event_initialize(tls_event_t* evt);
	extern void			tls_event_uninitialize(tls_event_t* evt);

	extern void			tls_event_wait(tls_event_t* evt);
	extern int			tls_event_timewait(tls_event_t* evt, int timeout);
	extern int			tls_event_trywait(tls_event_t* evt);

	extern void			tls_event_set(tls_event_t* evt);
	extern void			tls_event_reset(tls_event_t* evt);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __TLS_EVENT_H__
