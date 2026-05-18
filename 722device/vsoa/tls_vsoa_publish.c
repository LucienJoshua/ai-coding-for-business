#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "tls_log.h"
#include "tls_server.h"
#include "tls_vsoa_publish.h"


static TLS_RESULT	vsoa_publish_start_thread(tls_vsoa_publish_t* publish);
static void			vsoa_publish_stop_thread(tls_vsoa_publish_t* publish);
static void*		vsoa_publish_thread_work(void* arg);	


tls_vsoa_publish_t* vsoa_publish_allocate(void)
{
	tls_vsoa_publish_t*		publish;
	publish = (tls_vsoa_publish_t*)malloc(sizeof(tls_vsoa_publish_t));
	assert(publish);
	memset(publish, 0, sizeof(*publish));
	return publish;
}
void vsoa_publish_free(tls_vsoa_publish_t* publish)
{
	if (publish)
		free(publish);
}

TLS_RESULT vsoa_publish_start(tls_vsoa_publish_t* publish)
{
	TLS_RESULT						result;
	int								oldvalue;
	int								newvalue;
	int								ret;
	tls_server_t*					tls_server;
	struct tls_vsoa_publish_param*	publish_param;

	struct sockaddr_in				server_addr;
	char							server_name[256] = {0};

	do
	{
		if (NULL == publish)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			LOG_ERROR("ERROR-Invalid parameters.");
			break;
		}
		oldvalue = 0;
		newvalue = 1;
		if (__sync_val_compare_and_swap(&publish->vsoa_start_, oldvalue, newvalue))
		{
			result = TLS_RESULT_S_CONTINUE;
			LOG_INFO("VSOA publish is already started.");
			break;
		}

		tls_server = publish->tls_server_;
		publish_param = &tls_server->svr_param_.publish_param_;
		strcpy(publish->server_name_, publish_param->server_name_);
		strcpy(publish->server_password_, publish_param->server_password_);
		strcpy(publish->server_ip_, publish_param->server_ip_);
		publish->server_port_ = publish_param->server_port_;
		strcpy(publish->publish_url_, publish_param->url_);

		if (0 == strlen(publish->server_name_))
		{
			result = TLS_RESULT_E_INVALID_DATA;
			LOG_ERROR("ERROR-Invalid server data, server name empty.");
			break;
		}

		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(publish->server_port_);
		if (strlen(publish->server_ip_) > 0)
		{
			// server_addr.sin_addr.s_addr = inet_addr(publish->server_ip_);
			server_addr.sin_addr.s_addr = INADDR_ANY;
		}
		else
		{
			server_addr.sin_addr.s_addr = INADDR_ANY;
		}

		LOG_INFO("------------------------ tls publish config ------------------------");
		LOG_INFO("publish server name:%s.", publish->server_name_);
		LOG_INFO("publish server pass:%s.", publish->server_password_);
		LOG_INFO("publish server ip:%s.", publish->server_ip_);
		LOG_INFO("publish server port:%d.", publish->server_port_);

		sprintf(server_name, "{\"name\":\"%s\"}", publish->server_name_);

		LOG_INFO("publish server name:%s.", server_name);

		LOG_INFO("-------------------------------------------------------------------");
		publish->vsoa_server_ = vsoa_server_create(server_name);
		if (NULL == publish->vsoa_server_)
		{
			result = TLS_RESULT_E_FAIL;
			LOG_ERROR("ERROR-VSOA publish create server failed.");
			break;
		}
		if (strlen(publish->server_password_) > 0)
		{
			vsoa_server_passwd(publish->vsoa_server_, publish->server_password_);
		}

		ret = vsoa_server_start(publish->vsoa_server_, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in));
		if (!ret)
		{
			result = TLS_RESULT_E_FAIL;
			LOG_ERROR("ERROR-VSOA publish start failed.");
			break;
		}

		result = vsoa_publish_start_thread(publish);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-VSOA publish start thread fail.");
			break;
		}

		LOG_INFO("VSOA start publish OK.\n");
		
	}while(0);
	if (TlsResultFail(result))
	{
		vsoa_publish_stop(publish);
	}
	return result;
}
void vsoa_publish_stop(tls_vsoa_publish_t* publish)
{
	int					oldvalue;
	int					newvalue;


	if (NULL == publish)
		return;

	oldvalue = 1;
	newvalue = 0;
	if (!__sync_val_compare_and_swap(&publish->vsoa_start_, oldvalue, newvalue))
	{
		LOG_INFO("VSOA publish has already stop.\n");
		return;
	}

	vsoa_publish_stop_thread(publish);
	if (publish->vsoa_server_)
	{
		vsoa_server_close(publish->vsoa_server_);
		publish->vsoa_server_ = NULL;
	}
}

int vsoa_publish_data(tls_vsoa_publish_t* publish, PU8 data, U32 size)
{
	bool				ret = false;
	vsoa_url_t			url;
	vsoa_payload_t		payload;
	
	if (NULL == publish || NULL == data || 0 == size)
		return -1;

	url.url = publish->publish_url_;
	url.url_len = strlen(publish->publish_url_);

	payload.param_len = 0;
	payload.data = data;
	payload.data_len = size;

	LOG_INFO("url:%s, data size:%d\n", publish->publish_url_, size);

	ret = vsoa_server_publish(publish->vsoa_server_, &url, &payload);
	if (ret)
	{
		return size;
	}
	else
	{
		return -2;
	}
}


TLS_RESULT vsoa_publish_start_thread(tls_vsoa_publish_t* publish)
{
	if (NULL == publish)
		return TLS_RESULT_E_INVALID_PARAM;
	
	tls_event_initialize(&publish->evt_loop_);
	tls_event_initialize(&publish->evt_stop_);

	memset(&publish->thread_server_, 0, sizeof(publish->thread_server_));
	publish->thread_server_.run_ = 1;
	publish->thread_server_.thread_routine_ = vsoa_publish_thread_work;
	publish->thread_server_.arg_ = publish;
	publish->run_ = 1;

	tls_event_set(&publish->evt_loop_);

	tls_thread_start(&publish->thread_server_);
	
	return TLS_RESULT_S_OK;
}
void vsoa_publish_stop_thread(tls_vsoa_publish_t* publish)
{
	if (NULL == publish)	
		return;

	publish->run_ = 0;
	tls_event_set(&publish->evt_loop_);
	tls_event_timewait(&publish->evt_stop_, 3000);
	tls_thread_stop(&publish->thread_server_);

	tls_event_uninitialize(&publish->evt_loop_);
	tls_event_uninitialize(&publish->evt_stop_);
}
void* vsoa_publish_thread_work(void* arg)
{
	tls_vsoa_publish_t*		publish;
	struct timespec			timeout = {1, 0};
	int						cnt;
	int						max_fd;
	fd_set					fds;

	if (NULL == arg)
		return NULL;


	publish = (tls_vsoa_publish_t*)arg;

	tls_event_reset(&publish->evt_stop_);
	while (publish->run_)
	{
		tls_event_wait(&publish->evt_loop_);
		if (!publish->run_)
			break;

		FD_ZERO(&fds);
		max_fd = vsoa_server_fds(publish->vsoa_server_, &fds);
		cnt = pselect(max_fd+1, &fds, NULL, NULL, &timeout, NULL);
		if (cnt > 0)
		{
			vsoa_server_input_fds(publish->vsoa_server_, &fds);
		}

	}
	tls_event_set(&publish->evt_stop_);

	return NULL;
}
