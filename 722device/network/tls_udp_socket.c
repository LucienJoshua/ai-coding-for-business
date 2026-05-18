#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tls_type.h"
#include "tls_com.h"
#include "tls_log.h"
#include "tls_udp_socket.h"

tls_udp_socket_t* tls_udp_socket_allocate(void)
{
	tls_udp_socket_t*  udp;
	udp = (tls_udp_socket_t*)malloc(sizeof(tls_udp_socket_t));
	return udp;
}
void tls_udp_socket_free(tls_udp_socket_t* udp)
{
	if (udp)
		free(udp);
}

TLS_RESULT tls_udp_socket_open(tls_udp_socket_t* udp)
{
	TLS_RESULT			result = TLS_RESULT_E_FAIL;
	int					ret;
	int					reuse_addr = 1;
	do
	{
		if (!udp)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}
		udp->fd_ = socket(AF_INET, SOCK_DGRAM, 0);
		if (udp->fd_ < 0)
		{
			LOG_ERROR("ERROR-Create socket fail.");
			result = TLS_RESULT_E_SOCKET;
			break;
		}

		memset(&udp->local_, 0, sizeof(udp->local_));
		udp->local_.sin_family = AF_INET;
		udp->local_.sin_addr.s_addr = inet_addr(udp->sip_);
		udp->local_.sin_port = htons(udp->sport_);

		memset(&udp->remote_, 0, sizeof(udp->remote_));
		udp->remote_.sin_family = AF_INET;
		udp->remote_.sin_addr.s_addr = inet_addr(udp->dip_);
		udp->remote_.sin_port = htons(udp->dport_);

		setsockopt(udp->fd_, SOL_SOCKET, SO_REUSEADDR,  &reuse_addr, sizeof(reuse_addr));
		ret = bind(udp->fd_, (struct sockaddr*)&udp->local_, sizeof(udp->local_));
		if (ret < 0)
		{
			LOG_ERROR("ERROR-Bind local address fail, local ip:%s, local port:%d.", udp->sip_, udp->sport_);
			result = TLS_RESULT_E_FAIL;
			break;
		}
		result = TLS_RESULT_S_OK;
	}while(0);

	if (result < 0)
	{
		tls_udp_socket_close(udp);
	}
	return result;
}
TLS_RESULT tls_udp_socket_close(tls_udp_socket_t* udp)
{
	if (NULL == udp)
		return TLS_RESULT_E_INVALID_PARAM;
	if (udp->fd_ <= 0)
		return TLS_RESULT_E_INVALID_DATA;
	shutdown(udp->fd_, SHUT_RD);
	close(udp->fd_);
	return TLS_RESULT_S_OK;
}

S32	tls_udp_socket_transmit(tls_udp_socket_t* udp_socket, U8* data, U32 size)
{
	return sendto(udp_socket->fd_, (char*)data, (size_t)size, 0, 
	(const struct sockaddr*)&udp_socket->remote_, sizeof(udp_socket->remote_));
}
S32 tls_udp_socket_transmit_to(tls_udp_socket_t* udp_socket, U8* data, U32 size, struct sockaddr_in remote)
{
	return sendto(udp_socket->fd_, (char*)data, (size_t)size, 0, 
	(const struct sockaddr*)&remote, sizeof(remote));
}
S32	tls_udp_socket_receive(tls_udp_socket_t* udp_socket, U8* data, U32 size)
{
	int					ret;
	socklen_t			remote_size;
	struct sockaddr_in	remote;
	remote_size = sizeof(remote);
	ret = recvfrom(udp_socket->fd_, (char*)data, size,0, (struct sockaddr*)&remote, &remote_size);
	return ret;
}
