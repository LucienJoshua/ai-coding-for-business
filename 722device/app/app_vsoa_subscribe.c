#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "tls_log.h"
#include "app_vsoa_subscribe.h"


static void app_vsoa_on_subscribe_connect(void* arg, vsoa_client_auto_t* client_auto, bool connect, const char* info);

static void app_vsoa_on_subscribe_message(void* arg, struct vsoa_client* client, vsoa_url_t* url, vsoa_payload_t* payload, bool quick);


app_vsoa_subscribe_t* app_vsoa_subscribe_allocate(void)
{
	app_vsoa_subscribe_t*		app_subscribe;
	app_subscribe = (app_vsoa_subscribe_t*)malloc(sizeof(app_vsoa_subscribe_t));
	return app_subscribe;
}
void app_vsoa_subscribe_free(app_vsoa_subscribe_t* app_subscribe)
{
	if (app_subscribe)
		free(app_subscribe);
}

TLS_RESULT app_vsoa_subscribe_start(const app_vsoa_subscribe_param_t* param, app_vsoa_subscribe_t* subscribe)
{
	TLS_RESULT						result;
	int								oldvalue;
	int								newvalue;
	int								ret;
	app_vsoa_subscribe_param_t*		subscribe_param;

	int								vsoa_url_count = 1;
	int								vsoa_keepalive = 1000;
	int								vsoa_conn_timeout = 1000;
	int								vsoa_reccon_delay = 1000;;

	do
	{
		if (NULL == subscribe)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			LOG_ERROR("ERROR-Invalid parameters.");
			break;
		}
		oldvalue = 0;
		newvalue = 1;
		if (__sync_val_compare_and_swap(&subscribe->app_vsoa_subscribe_start_, oldvalue, newvalue))
		{
			result = TLS_RESULT_S_CONTINUE;
			LOG_ERROR("VSOA app subscribe is already started.");
			break;
		}
	
		if (0 == strlen(param->server_name_) || 0 == strlen(param->subscribe_url_))
		{
			result = TLS_RESULT_E_INVALID_DATA;
			LOG_ERROR("ERROR-VSOA app subscribe invalid data.");
			break;
		}
		subscribe_param = &subscribe->app_subscribe_param_;
		memcpy(subscribe_param, param, sizeof(*param));

		subscribe->app_vsoa_subscribe_client_auto_ = vsoa_client_auto_create(app_vsoa_on_subscribe_message, (void*)subscribe);
		if (NULL == subscribe->app_vsoa_subscribe_client_auto_)
		{
			result = TLS_RESULT_E_FAIL;
			LOG_ERROR("ERROR-VSOA client auto create fail.");
			break;
		}
		subscribe->app_vsoa_subscribe_client_ = vsoa_client_auto_handle(subscribe->app_vsoa_subscribe_client_auto_);
		if (NULL == subscribe->app_vsoa_subscribe_client_)
		{
			result = TLS_RESULT_E_INVALID_DATA;
			LOG_ERROR("ERROR-VSOA client auto handle fail.");
			break;
		}
		ret = vsoa_client_auto_setup(subscribe->app_vsoa_subscribe_client_auto_, app_vsoa_on_subscribe_connect, (void*)subscribe);
		if (!ret)
		{
			result = TLS_RESULT_E_FAIL;
			LOG_ERROR("ERROR-VSOA client auto setup fail, errno:%d, errstr:%s.", errno, strerror(errno));
			break;
		}
		subscribe->urls_[0] = subscribe_param->subscribe_url_;
		subscribe->url_count_ = 1;

		LOG_DEBUG("------------------- app subscribe config -------------------");
		LOG_DEBUG("app subscribe server name:%s.", subscribe_param->server_name_);
		LOG_DEBUG("app subscribe server pass:%s.", subscribe_param->server_password_);
		LOG_DEBUG("app subscribe url:%s.", subscribe_param->subscribe_url_);
		LOG_DEBUG("------------------------------------------------------------");
		ret = vsoa_client_auto_start(subscribe->app_vsoa_subscribe_client_auto_, subscribe_param->server_name_, subscribe_param->server_password_, 
				(char* const)subscribe->urls_, 1, 1000, 1000, 1000);
		if (!ret)
		{
			result = TLS_RESULT_E_FAIL;
			LOG_ERROR("ERROR-VSOA app subscribe start fail, errno:%d, errstr:%s.", errno, strerror(errno));
			break;
		}
		LOG_DEBUG("VSOA app subscribe start OK!");
		result = TLS_RESULT_S_OK;
	}while(0);

	if (TlsResultFail(result))
	{
		app_vsoa_subscribe_stop(subscribe);
	}
	return result;
}
void app_vsoa_subscribe_stop(app_vsoa_subscribe_t* subscribe)
{
	int				oldvalue;
	int				newvalue;

	if (NULL == subscribe)
		return;

	oldvalue = 1;
	newvalue = 0;
	if (!__sync_val_compare_and_swap(&subscribe->app_vsoa_subscribe_start_, oldvalue, newvalue))
	{
		LOG_INFO("VSOA app subscribe already stop.");
		return;
	}

	if (subscribe->app_vsoa_subscribe_client_auto_!= NULL)
	{
		vsoa_client_auto_stop(subscribe->app_vsoa_subscribe_client_auto_);
		vsoa_client_auto_delete(subscribe->app_vsoa_subscribe_client_auto_);
		subscribe->app_vsoa_subscribe_client_auto_ = NULL;
	}
}

void app_vsoa_on_subscribe_connect(void* arg, vsoa_client_auto_t* client_auto, bool connect, const char* info)
{
	app_vsoa_subscribe_t*			app_subscribe;
	app_vsoa_subscribe_param_t*		app_subscribe_param;
	if (NULL == arg)
		return;

	app_subscribe = (app_vsoa_subscribe_t*)arg;
	app_subscribe_param = &app_subscribe->app_subscribe_param_;
	if (app_subscribe_param->on_app_subscribe_connect)
	{
		app_subscribe_param->on_app_subscribe_connect(app_subscribe_param->app_subscribe_arg_, (int)connect);
	}
}

void app_vsoa_on_subscribe_message(void* arg, struct vsoa_client* client, vsoa_url_t* url, vsoa_payload_t* payload, bool quick)
{
	app_vsoa_subscribe_t*			app_subscribe;
	app_vsoa_subscribe_param_t*		app_subscribe_param;
	if (NULL == arg)
		return;

	app_subscribe = (app_vsoa_subscribe_t*)arg;
	app_subscribe_param = &app_subscribe->app_subscribe_param_;
	if (app_subscribe_param->on_app_subscribe_message)
	{
		app_subscribe_param->on_app_subscribe_message(app_subscribe_param->app_subscribe_arg_, url, payload);
	}
}

