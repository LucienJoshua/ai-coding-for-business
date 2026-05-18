#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "tls_log.h"
#include "app_udp_endpoint.h"

static app_udp_endpoint_t*	udp_endpoint_allocate(void);
static void					udp_endpoint_free(app_udp_endpoint_t* udp_endpoint);
static void*				udp_endpoint_receive_thread(void* arg);

app_udp_endpoint_t* udp_endpoint_create(const app_udp_param_t* udp_param)
{
	app_udp_endpoint_t*			udp_endpoint = NULL;
	tls_udp_socket_t*			udp_socket = NULL;
	tls_thread_t*				tls_thread = NULL;
	TLS_RESULT					result = TLS_RESULT_E_FAIL;
	do
	{
		if (NULL == udp_param || 0 == strlen(udp_param->local_ip_) || 0 == strlen(udp_param->remote_ip_))
			break;	
		udp_endpoint = udp_endpoint_allocate();
		if (NULL == udp_endpoint)
			break;

		memset(udp_endpoint, 0, sizeof(*udp_endpoint));
		memcpy(&udp_endpoint->udp_param_, udp_param, sizeof(udp_endpoint->udp_param_));
		udp_endpoint->udp_socket_ = tls_udp_socket_allocate();
		assert(udp_endpoint->udp_socket_);
		udp_socket =  udp_endpoint->udp_socket_;

		memset(udp_socket, 0, sizeof(*udp_socket));
		strcpy(udp_socket->sip_, udp_param->local_ip_);	
		udp_socket->sport_ = udp_param->local_port_;
		strcpy(udp_socket->dip_, udp_param->remote_ip_);	
		udp_socket->dport_ = udp_param->remote_port_;

		result = tls_udp_socket_open(udp_socket);
		if (TlsResultFail(result))
		{
			LOG_ERROR("transport udp socket create fail.");
			break;
		}
		tls_thread = &udp_endpoint->udp_recv_thread_;
		tls_thread->arg_ = udp_endpoint;
		tls_thread->thread_routine_ = udp_endpoint_receive_thread;
		tls_thread_start(tls_thread);
		result = TLS_RESULT_S_OK;

	}while(0);
	if (TlsResultFail(result))
	{
		udp_endpoint_delete(udp_endpoint);
		udp_endpoint = NULL;
	}
	return udp_endpoint;
}

S32	udp_endpoint_send(app_udp_endpoint_t* udp_endpoint, PU8 data, U32 size)
{
	if (NULL == udp_endpoint || NULL == data || 0 == size)
		return -1;
	
	return tls_udp_socket_transmit(udp_endpoint->udp_socket_, data, size);
}

void udp_endpoint_delete(app_udp_endpoint_t* udp_endpoint)
{
	tls_udp_socket_t*		udp_socket = NULL;
	if (NULL == udp_endpoint)
		return;
	udp_socket = udp_endpoint->udp_socket_;
	if (udp_socket)
	{
		tls_udp_socket_close(udp_socket);
		tls_udp_socket_free(udp_socket);
		udp_endpoint->udp_socket_ = NULL;
	}
	tls_thread_stop(&udp_endpoint->udp_recv_thread_);
	udp_endpoint_free(udp_endpoint);
}

app_udp_endpoint_t*	udp_endpoint_allocate(void)
{
	app_udp_endpoint_t* udp_endpoint;
	udp_endpoint = (app_udp_endpoint_t*)malloc(sizeof(app_udp_endpoint_t));
	return udp_endpoint;
}
void udp_endpoint_free(app_udp_endpoint_t* udp_endpoint)
{
	if (udp_endpoint)
		free(udp_endpoint);
}

void* udp_endpoint_receive_thread(void* arg)
{
	app_udp_endpoint_t*			udp_endpoint = NULL;
	app_udp_param_t*			udp_param;
	tls_udp_socket_t*			udp_socket = NULL;
	S32							ret = -1;


	if (NULL == arg)
		return NULL;
	LOG_DEBUG("Enter the udp endpoint receive thread.");

	udp_endpoint = (app_udp_endpoint_t*)arg;
	udp_param = &udp_endpoint->udp_param_;
	udp_socket = udp_endpoint->udp_socket_;

	while (1)
	{
		LOG_DEBUG("Prepare to receive udp data.");
		udp_endpoint->udp_recv_size_ = sizeof(udp_endpoint->udp_recv_data_);
		ret = tls_udp_socket_receive(udp_socket, udp_endpoint->udp_recv_data_, udp_endpoint->udp_recv_size_);
		if (ret > 0)
		{
			LOG_DEBUG("udp thread, receive data:%d.", ret);
			if (udp_param->udp_endpoint_receive)
			{
				udp_param->udp_endpoint_receive(udp_param->userdata, 
						udp_endpoint->udp_recv_data_, ret);
			}
		}
		else
		{
			LOG_DEBUG("udp thread, receive data fail, ret:%d.", ret);
		}
	}

	return NULL;
}
