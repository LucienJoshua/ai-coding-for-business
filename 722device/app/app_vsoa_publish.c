#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "tls_log.h"
#include "app_vsoa_publish.h"

static void			app_vsoa_publish_start_thread(app_vsoa_publish_t* publish);
static void			app_vsoa_publish_stop_thread(app_vsoa_publish_t* publish);
static void*		app_vsoa_publish_thread_work(void* arg);

app_vsoa_publish_t* app_vsoa_publish_allocate(void)
{
	app_vsoa_publish_t*			publish;
	publish = (app_vsoa_publish_t*)malloc(sizeof(app_vsoa_publish_t));
	assert(publish);
	memset(publish, 0, sizeof(*publish));
	return publish;
}
void app_vsoa_publish_free(app_vsoa_publish_t* publish)
{
	if (publish)
		free(publish);
}

TLS_RESULT app_vsoa_publish_start(app_vsoa_publish_param_t* param,  app_vsoa_publish_t* publish)
{
	TLS_RESULT						result;
	int								oldvalue;
	int								newvalue;
	int								ret;
	app_vsoa_publish_param_t*		app_publish_param;

	struct sockaddr_in				server_addr;
	char							server_name[256] = {0};

	do
	{
		if (NULL == param || NULL == publish)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			LOG_ERROR("ERROR-VSOA app publish invalid parameters.");
			break;
		}
		oldvalue = 0;
		newvalue = 1;
		if (__sync_val_compare_and_swap(&publish->app_vsoa_publish_start_, oldvalue, newvalue))
		{
			result = TLS_RESULT_S_CONTINUE;
			LOG_ERROR("VSOA app publish is already started.");
			break;
		}
		if (0 == strlen(param->server_name_) || 0 == param->server_port_ || 0 == strlen(param->server_url_))
		{
			result = TLS_RESULT_E_INVALID_DATA;
			LOG_ERROR("ERROR-VSOA app publish invalid data.");
			break;
		}
		memcpy(&publish->app_publish_param_, param, sizeof(app_vsoa_publish_param_t));	
		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(param->server_port_);
		if (strlen(param->server_ip_) > 0)
		{
			server_addr.sin_addr.s_addr = inet_addr(param->server_ip_);
		}
		else
		{
			server_addr.sin_addr.s_addr = INADDR_ANY;
		}

		LOG_DEBUG("------------------- app publish server config -------------------");
		LOG_DEBUG("app publish server name:%s.", param->server_name_);
		LOG_DEBUG("app pubish server pass:%s.", param->server_password_);
		LOG_DEBUG("app publish server ip:%s.", param->server_ip_);
		LOG_DEBUG("app publish server port:%d.", param->server_port_);
		LOG_DEBUG("app publish server url:%s.", param->server_url_);
		LOG_DEBUG("----------------------------------------------------------------");

		sprintf(server_name, "{\"name\":\"%s\"}", param->server_name_);

		LOG_DEBUG("app publish server name:%s.", server_name);
		publish->app_vsoa_server_ = vsoa_server_create(server_name);
		if (NULL == publish->app_vsoa_server_)
		{
			result = TLS_RESULT_E_FAIL;
			LOG_ERROR("ERROR-VSOA app publish create server failed.");
			break;
		}
		if (strlen(param->server_password_) > 0)
		{
			vsoa_server_passwd(publish->app_vsoa_server_, param->server_password_);
		}

		ret = vsoa_server_start(publish->app_vsoa_server_, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in));
		if (!ret)
		{
			result = TLS_RESULT_E_FAIL;
			LOG_ERROR("ERROR-VSOA app publish start failed.");
			break;
		}
		app_vsoa_publish_start_thread(publish);
		result = TLS_RESULT_S_OK;
		LOG_DEBUG("VSOA app publish server start OK!");	


	}while (0);
	if (TlsResultFail(result))
	{
		app_vsoa_publish_stop(publish);
	}
	return result;
}
void app_vsoa_publish_stop(app_vsoa_publish_t* publish)
{
	int					oldvalue;
	int					newvalue;

	if (NULL == publish)
		return;

	oldvalue = 1;
	newvalue = 0;
	if (!__sync_val_compare_and_swap(&publish->app_vsoa_publish_start_, oldvalue, newvalue))
	{
		LOG_INFO("VSOA app publish has already stop.");
		return;
	}

	app_vsoa_publish_stop_thread(publish);
	if (publish->app_vsoa_server_)
	{
		vsoa_server_close(publish->app_vsoa_server_);
		publish->app_vsoa_server_ = NULL;
	}
}
TLS_RESULT app_vsoa_publish_data(app_vsoa_publish_t* publish, const vsoa_url_t* url, const vsoa_payload_t* payload)
{
	if (NULL == publish || NULL == url || NULL == payload)
		return TLS_RESULT_E_INVALID_PARAM;

	return vsoa_server_publish(publish->app_vsoa_server_, url, payload) ? TLS_RESULT_S_OK : TLS_RESULT_E_FAIL;
}


void app_vsoa_publish_start_thread(app_vsoa_publish_t* publish)
{
	if (NULL == publish)
		return;
	
	tls_event_initialize(&publish->evt_loop_);
	tls_event_initialize(&publish->evt_stop_);

	memset(&publish->thread_app_vsoa_publish_, 0, sizeof(publish->thread_app_vsoa_publish_));
	publish->thread_app_vsoa_publish_.run_ = 1;
	publish->thread_app_vsoa_publish_.thread_routine_ = app_vsoa_publish_thread_work;
	publish->thread_app_vsoa_publish_.arg_ = publish;
	publish->run_ = 1;

	tls_event_set(&publish->evt_loop_);

	tls_thread_start(&publish->thread_app_vsoa_publish_);
	
}
void app_vsoa_publish_stop_thread(app_vsoa_publish_t* publish)
{
	if (NULL == publish)	
		return;

	publish->run_ = 0;
	tls_event_set(&publish->evt_loop_);
	tls_event_timewait(&publish->evt_stop_, 3000);
	tls_thread_stop(&publish->thread_app_vsoa_publish_);

	tls_event_uninitialize(&publish->evt_loop_);
	tls_event_uninitialize(&publish->evt_stop_);
}
void* app_vsoa_publish_thread_work(void* arg)
{
	app_vsoa_publish_t*		publish;
	struct timespec			timeout = {1, 0};
	int						cnt;
	int						max_fd;
	fd_set					fds;

	if (NULL == arg)
		return NULL;

	publish = (app_vsoa_publish_t*)arg;

	tls_event_reset(&publish->evt_stop_);
	while (publish->run_)
	{
		tls_event_wait(&publish->evt_loop_);
		if (!publish->run_)
			break;

		FD_ZERO(&fds);
		max_fd = vsoa_server_fds(publish->app_vsoa_server_, &fds);
		cnt = pselect(max_fd+1, &fds, NULL, NULL, &timeout, NULL);
		if (cnt > 0)
		{
			vsoa_server_input_fds(publish->app_vsoa_server_, &fds);
		}

	}
	tls_event_set(&publish->evt_stop_);

	return NULL;
}
