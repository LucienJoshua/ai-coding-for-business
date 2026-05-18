#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

#ifdef __cplusplus
extern "C"{
#endif //__cplusplus 

#include <arpa/inet.h>
#include <sys/epoll.h>
#include "tls_com.h"
#include "tls_type.h"
#include "tls_thread.h"
#include "tls_event.h"

#define SERVER_EPOLL_SIZE 1024


	typedef struct __tcp_param
	{
		char						server_ip_[TLS_MAX_IP_SIZE];
		U16							server_port_;
	}tcp_param_t;


	typedef struct __tcp_server
	{
		int							init_;
		int							start_;

		tcp_param_t					tcpparam_;

		int							socketfd_;
		struct sockaddr_in			local_addr_;

		int							epoll_fd_;
		struct epoll_event			epoll_events_[SERVER_EPOLL_SIZE];
		struct epoll_event			epoll_event_;
		int							epoll_size_;

		S32							thread_run_;
		tls_thread_t				work_thread_;
		tls_event_t					evt_loop_;
		tls_event_t					evt_stop_;

	}tcp_server_t;


	extern tcp_server_t*			tcp_server_allocate(void);
	extern void						tcp_server_free(tcp_server_t* tcp);

	extern TLS_RESULT				tcp_server_initialize(tcp_server_t* tcp, const tcp_param_t* tcp_param);
	extern TLS_RESULT				tcp_server_uninitialize(tcp_server_t* tcp);

	extern TLS_RESULT				tcp_server_start(tcp_server_t* tcp);
	extern TLS_RESULT				tcp_server_stop(tcp_server_t* tcp);


#ifdef __cplusplus
}
#endif //__cplusplus 

#endif // __TCP_SERVER_H__
