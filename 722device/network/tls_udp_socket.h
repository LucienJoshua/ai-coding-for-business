#ifndef __TLS_UDP_H__
#define __TLS_UDP_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>

	struct tls_thread_t;

	struct tls_udp_param
	{
		S8							sip_[TLS_MAX_IP_SIZE];
		U16							sport_;
		S8							dip_[TLS_MAX_IP_SIZE];
		U16							dport_;
	};

	struct tls_udp_socket
	{
		S8							sip_[TLS_MAX_IP_SIZE];
		U16							sport_;
		S8							dip_[TLS_MAX_IP_SIZE];
		U16							dport_;
	
		int							fd_;
		struct sockaddr_in			local_;
		struct sockaddr_in			remote_;
	};

	typedef struct tls_udp_socket	tls_udp_socket_t;

	extern tls_udp_socket_t*	tls_udp_socket_allocate(void);
	extern void					tls_udp_socket_free(tls_udp_socket_t* udp);

	extern TLS_RESULT			tls_udp_socket_open(tls_udp_socket_t* udp);
	extern TLS_RESULT			tls_udp_socket_close(tls_udp_socket_t* udp);

	extern S32					tls_udp_socket_transmit(tls_udp_socket_t* udp, U8* data, U32 size);
	extern S32					tls_udp_socket_transmit_to(tls_udp_socket_t* udp, U8* data, U32 size, struct sockaddr_in remote);
	extern S32					tls_udp_socket_receive(tls_udp_socket_t* udp, U8* data, U32 size);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __TLS_UDP_H__
