#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/epoll.h>
#include "tcp_server.h"

tcp_server_t* tcp_server_allocate(void)
{
	tcp_server_t*	tcp = NULL;
	tcp = (tcp_server_t*)malloc(sizeof(tcp_server_t));
	return tcp;
}
void tcp_server_free(tcp_server_t* tcp)
{
	if (tcp)
	{
		free(tcp);
	}
}

TLS_RESULT tcp_server_initialize(tcp_server_t* tcp, const tcp_param_t* tcp_param)
{
	TLS_RESULT			result = TLS_RESULT_E_FAIL;
	int					oldvalue;
	int					newvalue;

	do
	{
		if (NULL == tcp || NULL == tcp_param)
		{
			__TLSDBG("ERROR-Invalid parameters.");
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}
		oldvalue = 0;
		newvalue = 1;
		if (__sync_val_compare_and_swap(&tcp->init_, oldvalue, newvalue))
		{
			__TLSDBG("TCP Server has already initialize.");
			result = TLS_RESULT_S_CONTINUE;
			break;
		}
		if (0 == strlen(tcp_param->server_ip_) || tcp_param->server_port_ <= 0)
		{
			__TLSDBG("ERROR-Invalid tcp param, server ip:%s, server port:%d.",
					tcp_param->server_ip_, tcp_param->server_port_);
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}

		tcp->socketfd_ = socket(AF_INET, SOCK_STREAM, 0);
		if (tcp->socketfd_)
		{
			__TLSDBG("ERROR-Create tcp socket fd fail, errno:%d, errstr:%s.",
					errno, strerror(errno));
			result = TLS_RESULT_E_SOCKET;
			break;
		}
	}while(0);

	if (TlsResultFail(result))
	{
		tcp_server_uninitialize(tcp);
	}
	return result;
}
TLS_RESULT tcp_server_uninitialize(tcp_server_t* tcp)
{
	return TLS_RESULT_S_OK;
}

TLS_RESULT tcp_server_start(tcp_server_t* tcp)
{
	return TLS_RESULT_S_OK;
}
TLS_RESULT tcp_server_stop(tcp_server_t* tcp)
{
	return TLS_RESULT_S_OK;
}
