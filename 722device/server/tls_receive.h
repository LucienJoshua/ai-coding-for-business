#ifndef __TLS_RECEIVE_H__
#define __TLS_RECEIVE_H__

#include "tls_list.h"
#include "tls_lock.h"
#include "tls_udp_socket.h"
#include "tls_thread.h"
#include "tls_event.h"

struct tls_receive
{
	S32						start_;
	
	tls_list_node_t			queue_receive_;
	tls_lock_t				queue_receive_lock_;

	void*					tls_server_;
	tls_udp_socket_t*		udp_socket_;

	S32						run_;
	tls_thread_t			thread_receive_;
	tls_event_t				evt_loop_;
	tls_event_t				evt_stop_;
};

typedef struct tls_receive	tls_receive_t;

extern tls_receive_t*		tls_receive_allocate(void);
extern void					tls_receive_free(tls_receive_t* r);

extern TLS_RESULT			tls_receive_start(void* s, tls_receive_t* r);
extern TLS_RESULT			tls_receive_stop(tls_receive_t* r);
extern void					tls_receive_release_buff(tls_receive_t* r);

extern void					tls_receive_push(tls_receive_t* r, tls_buff_t* buff);
extern tls_buff_t*			tls_receive_pop(tls_receive_t* r);

#endif // __TLS_RECEIVE_H__
