#ifndef __TLS_TRANSMIT_H__
#define __TLS_TRANSMIT_H__

#include "tls_list.h"
#include "tls_lock.h"
#include "tls_udp_socket.h"
#include "tls_thread.h"
#include "tls_event.h"

struct tls_buff;
typedef struct tls_buff tls_buff_t;

typedef struct tls_transmit_queue
{
	tls_list_node_t			list_buff_;
	tls_lock_t				list_buff_lock_;
	U8						queue_priority_;
}tls_transmit_queue_t;

struct tls_transmit
{
	S32						start_;
	
	tls_transmit_queue_t	queue_transmits_[TLS_QUEUE_MAX_COUNT];

	void*					tls_server_;
	tls_udp_socket_t*		udp_socket_;

	S32						run_;
	tls_thread_t			thread_transmit_;
	tls_event_t				evt_loop_;
	tls_event_t				evt_stop_;

	tls_transmit_queue_t*	queue_transmit_;
	tls_lock_t				queue_transmit_lock_;
};

typedef struct tls_transmit	tls_transmit_t;

extern tls_transmit_t*		tls_transmit_allocate(void);
extern void					tls_transmit_free(tls_transmit_t* t);

extern TLS_RESULT			tls_transmit_start(void* s, tls_transmit_t* t);
extern TLS_RESULT			tls_transmit_stop(tls_transmit_t* t);
extern void					tls_transmit_release_buff(tls_transmit_t* t);

extern S32					tls_transmit_xmit_data(tls_transmit_t* t, PU8 data, U32 size);
extern TLS_RESULT			tls_transmit_xmit(tls_transmit_t* t, tls_buff_t* buff);
extern TLS_RESULT			tls_transmit_push(tls_transmit_t* t, tls_buff_t* buff);

extern void					tls_transmit_active(tls_transmit_t* t);
extern void					tls_transmit_deactive(tls_transmit_t* t);

#endif // __TLS_TRANSMIT_H__
