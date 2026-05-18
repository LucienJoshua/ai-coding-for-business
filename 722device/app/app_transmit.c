#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "tls_server.h"
#include "vsoa_server.h"
#include "app_config.h"
#include "app_vsoa_rpc_server.h"
#include "app_vsoa_publish.h"
#include "tls_vsoa.h"
#include "file_transmit.h"
#include "tls_log.h"
#include "tls_statis_rate.h"
#include "tls_net_proxy.h"
#include "app_transmit.h"


#define API_APP_CONFIG_NAME		"app_config.xml"
#define API_TLS_CONFIG_NAME		"tls_config.xml"
#define API_FILE_CONFIG_NAME	"file_config.xml"

#define __APPVALID_TRANSMIT(app_transmit) \
	if (NULL == (app_transmit) || 0 == (app_transmit->initialize_)) \
		return TLS_RESULT_E_INVALID_PARAM;


static TLS_RESULT			api_transmit_get_app_path(char* app_path, U32 app_path_size);

static TLS_RESULT			api_transmit_create_tls_log(app_transmit_t* app_transmit);
static void					api_transmit_delete_tls_log(app_transmit_t* app_transmit);

static TLS_RESULT			api_transmit_load_app_config(app_transmit_t* app_transmit);
static TLS_RESULT			api_transmit_load_tls_config(app_transmit_t* app_transmit);
static TLS_RESULT			api_transmit_load_file_config(app_transmit_t* app_transmit);

static TLS_RESULT			api_transmit_start_tls_server(app_transmit_t* app_transmit);
static void					api_transmit_stop_tls_server(app_transmit_t* app_transmit);

static TLS_RESULT			api_transmit_start_app_rpc_server(app_transmit_t* app_transmit);
static void					api_transmit_stop_app_rpc_server(app_transmit_t* app_transmit);

static TLS_RESULT			api_transmit_start_app_publish(app_transmit_t* app_transmit);
static TLS_RESULT			api_transmit_stop_app_publish(app_transmit_t* app_transmit);

static TLS_RESULT			api_transmit_start_file_transmit(app_transmit_t* app_transmit);
static TLS_RESULT			api_transmit_stop_file_transmit(app_transmit_t* app_transmit);

static void					api_transmit_callback_on_tls_register(void* priv_dta, U32 status);
static TLS_RESULT			api_transmit_callback_on_tls_receive(void* priv_data, PU8 data, U32 size);
static void					api_transmit_callback_on_tls_subscribe_message(void* arg, PU8 data, U32 size);

static void					api_transmit_callback_on_app_rpc_server_message(void* arg, vsoa_url_t* url, vsoa_payload_t* payload);
static TLS_RESULT			api_transmit_app_rpc_server_parse_url(const vsoa_url_t* url, U32* addr, U16* entry, U8* priority, char* newurl);

static TLS_RESULT			api_transmit_start_statis_rate(app_transmit_t* app_transmit);
static void					api_transmit_stop_statis_rate(app_transmit_t* app_transmit);

static TLS_RESULT			api_transmit_start_app_subscribe(app_transmit_t* app_transmit);
static TLS_RESULT			api_transmit_stop_app_subscribe(app_transmit_t* app_transmit);
static void					api_transmit_callback_on_app_subscribe_connect(void* arg, int connect);
static void					api_transmit_callback_on_app_subscribe_message(void* arg, vsoa_url_t* url, vsoa_payload_t* payload);

TLS_RESULT api_transmit_initialize(app_transmit_t* app_transmit)
{
	TLS_RESULT					result;
	int							oldvalue;
	int							newvalue;
	void*						log = NULL;

	do
	{
		if (NULL == app_transmit)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			__TLSDBG("ERROR-Invalid parameters.");
			break;
		}
		oldvalue = 0;
		newvalue = 1;
		if (__sync_val_compare_and_swap(&app_transmit->initialize_, oldvalue, newvalue))
		{
			result = TLS_RESULT_S_CONTINUE;
			__TLSDBG("App transmit has initialized already.");
			break;
		}

		api_transmit_get_app_path(app_transmit->app_path_, sizeof(app_transmit->app_path_));

		result = api_transmit_create_tls_log(app_transmit);
		if (TlsResultFail(result))
		{
			__TLSDBG("ERROR-Can not create tls log entry");
			break;
		}
		log = app_transmit->tls_log_;

		LOG_INFO("Create zlog entry ok");

		result = api_transmit_load_app_config(app_transmit);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-Load app config failed.");
			break;
		}
		result = api_transmit_load_tls_config(app_transmit);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-Load tls config failed.");
			break;
		}
		result = api_transmit_load_file_config(app_transmit);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-Load file config failed.");
			break;
		}
		result = api_transmit_start_tls_server(app_transmit);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-Create tls server failed.");
			break;
		}
		result = api_transmit_start_app_publish(app_transmit);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-Create app publish server failed.");
			break;
		}
		result = api_transmit_start_app_rpc_server(app_transmit);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-Create app rpc server failed.");
			break;
		}
		result = api_transmit_start_file_transmit(app_transmit);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-Create file transmit service failed.");
			break;
		}

		result = api_transmit_start_statis_rate(app_transmit);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-start statis rate task failed.");
			break;
		}

		result = api_transmit_start_app_subscribe(app_transmit);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-start app subscribe failed.");
			break;
		}

		result = api_transmit_start_net_proxy(app_transmit);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-start net proxy failed.");
			break;
		}
	}while(0);

	if (TlsResultOk(result))
	{
		LOG_DEBUG("All modules create and initialize OK!");
	}
	return result;
}

int api_transmit_running_status(app_transmit_t* app_transmit)
{
	return 1;
}

void api_transmit_poll(app_transmit_t* app_transmit)
{
	// api_transmit_tls_server_publish_test(app_transmit);

	// LOG_INFO("[%s] transmit poll\n", __func__);
	sleep(1);
}

TLS_RESULT api_transmit_uninitialize(app_transmit_t* app_transmit)
{
	int				oldvalue;
	int				newvalue;

	if (NULL == app_transmit)
		return TLS_RESULT_E_INVALID_PARAM;

	oldvalue = 1;
	newvalue = 0;
	if (!__sync_val_compare_and_swap(&app_transmit->initialize_, oldvalue, newvalue))
	{
		LOG_INFO("App transmit has uninitializeed already.");
		return TLS_RESULT_S_CONTINUE;
	}

	api_transmit_stop_file_transmit(app_transmit);
	api_transmit_stop_app_publish(app_transmit);
	api_transmit_stop_app_rpc_server(app_transmit);
	api_transmit_stop_tls_server(app_transmit);
	api_transmit_delete_tls_log(app_transmit);
	api_transmit_stop_statis_rate(app_transmit);
	tls_net_proxy_uninit();
	memset(app_transmit, 0, sizeof(*app_transmit));

	return TLS_RESULT_S_OK;
}
void api_transmit_tls_server_publish_test(app_transmit_t* app_transmit)
{
	vsoa_url_t			url;
	vsoa_payload_t		payload;
	char				param[100];
	int					roll = 1, pitch = 1, yaw = 1;

	if (NULL == app_transmit || NULL == app_transmit->tls_server_)
		return;

	url.url="/csfw54_sub";
	url.url_len = strlen(url.url);
	payload.data = NULL;
	payload.data_len = 0;
	payload.param = param;

	payload.param_len = snprintf(param, 100, 
				"{\"roll\":%d, \"pitch\":%d, \"yaw\":%d}", 
				roll++, pitch++, yaw++);

	tls_server_vsoa_publish((tls_server_t*)app_transmit->tls_server_, &url, &payload, 0);
}


TLS_RESULT	api_transmit_get_app_path(char* app_path, U32 app_path_size)
{
	ssize_t					len = 0;
	char					flag = '/';
	char*					str = NULL;
	char					path[4096];

	if (NULL == app_path || 0 == app_path_size)
		return TLS_RESULT_E_INVALID_PARAM;

	len = readlink("/proc/self/exe", path, sizeof(path)-1);
	if (len < 0)
		return TLS_RESULT_E_FAIL;

	path[len] = '\0';

	str = strrchr(path, flag);
	if (NULL == str)
		return TLS_RESULT_E_FAIL;

	str++;
	*str = '\0';

	strcpy(app_path, path);

	return TLS_RESULT_S_OK;
}

TLS_RESULT api_transmit_create_tls_log(app_transmit_t* app_transmit)
{
	TLS_RESULT			result = TLS_RESULT_E_FAIL;
	tls_log_t*			log = NULL;
	do
	{
		if (NULL == app_transmit)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}

		log = tls_log_allocate();
		if (NULL == log)
		{
			result = TLS_RESULT_E_ALLOCATE;
			break;
		}
		memset(log, 0, sizeof(tls_log_t));
		result = tls_log_initialize(log);

	}while(0);
	if (TlsResultOk(result))
	{
		app_transmit->tls_log_ = log;
	}
	else
	{
		if (log)
		{
			tls_log_uninitialize(log);
			tls_log_free(log);
		}
		app_transmit->tls_log_ = NULL;
	}
	return result;
}
void api_transmit_delete_tls_log(app_transmit_t* app_transmit)
{
	TLS_RESULT			result = TLS_RESULT_E_FAIL;
	tls_log_t*			log = NULL;

	if (NULL == app_transmit)
	{
		__TLSDBG("ERROR-Invalid parameters.");
		return;
	}
	log = app_transmit->tls_log_;
	if (log)
	{
		tls_log_uninitialize(log);
		tls_log_free(log);
		app_transmit->tls_log_ = NULL;
	}
}

TLS_RESULT api_transmit_load_app_config(app_transmit_t* app_transmit)
{
	TLS_RESULT					result = TLS_RESULT_E_FAIL;
	app_config_t*				app_config;
	char						app_config_path[4096];

	__APPVALID_TRANSMIT(app_transmit);

	strcpy(app_config_path, app_transmit->app_path_);
	strcat(app_config_path, "tlsconfig/");
	strcat(app_config_path, API_APP_CONFIG_NAME);

	app_config = &app_transmit->app_config_;
	memset(app_config, 0, sizeof(app_config_t));
	result = api_app_config_load_all(app_config_path, app_config);

	return result;
}
TLS_RESULT api_transmit_load_tls_config(app_transmit_t* app_transmit)
{
	TLS_RESULT					result = TLS_RESULT_E_FAIL;
	tls_config_t*				tls_config;
	char						tls_config_path[4096];	

	__APPVALID_TRANSMIT(app_transmit);

	strcpy(tls_config_path, app_transmit->app_path_);
	strcat(tls_config_path, "tlsconfig/");
	strcat(tls_config_path, API_TLS_CONFIG_NAME);

	tls_config = &app_transmit->tls_config_;
	memset(tls_config, 0, sizeof(tls_config_t));
	result = tls_config_load_all(tls_config_path, tls_config);

	return result;
}
TLS_RESULT api_transmit_load_file_config(app_transmit_t* app_transmit)
{
	TLS_RESULT					result = TLS_RESULT_E_FAIL;
	file_config_t*				file_config;
	char						file_config_path[4096];

	__APPVALID_TRANSMIT(app_transmit);

	strcpy(file_config_path, app_transmit->app_path_);
	strcat(file_config_path, "tlsconfig/");
	strcat(file_config_path, API_FILE_CONFIG_NAME);

	file_config = &app_transmit->file_config_;
	memset(file_config, 0, sizeof(file_config_t));
	result = file_config_load_all(file_config_path, file_config);

	return result;
}

TLS_RESULT api_transmit_start_tls_server(app_transmit_t* app_transmit)
{
	TLS_RESULT							result = TLS_RESULT_E_FAIL;

	tls_config_t*						tls_config;
	tls_param_t							tls_server_param;
	struct tls_vsoa_subscribe_param*	vsoa_subscribe_param = NULL;
	struct tls_vsoa_publish_param*		vsoa_publish_param = NULL;
	tls_server_t*						tls_server = NULL;

	int									index;
	tle_queue_param_t*					queue_param;
	tls_queue_config_t*					queue_config;

	do
	{
		if (app_transmit->tls_server_ != NULL)
			api_transmit_stop_tls_server(app_transmit);

		tls_config = &app_transmit->tls_config_;
		
		// initialize the tls server param
		memset(&tls_server_param, 0, sizeof(tls_server_param));
		strcpy(tls_server_param.local_ip_, tls_config->tls_local_ip);
		tls_server_param.local_port_ = tls_config->tls_local_port;
		strcpy(tls_server_param.remote_ip_, tls_config->tls_remote_ip);
		tls_server_param.remote_port_ = tls_config->tls_remote_port;
		tls_server_param.tls_local_id_ = tls_config->tls_local_id;
		tls_server_param.tls_local_sub_id_ = tls_config->tls_local_sub_id;
		tls_server_param.priv_data_ = app_transmit;	
		tls_server_param.on_tls_register_callback	= api_transmit_callback_on_tls_register;
		tls_server_param.on_tls_receive_callback	= api_transmit_callback_on_tls_receive;	

		for (index = 0; index < TLS_QUEUE_COUNT; index++)
		{
			queue_param = &tls_server_param.queue_param_[index];
			queue_config = &tls_config->queue_config_[index];
			queue_param->priority_ = queue_config->priority_; 
			queue_param->buff_size_ = queue_config->buffsize_;
			queue_param->buff_count_ = queue_config->buffcount_;
		}

		// initialize the vsoa subscribe param
		vsoa_subscribe_param = &tls_server_param.subscribe_param_;
		memset(vsoa_subscribe_param, 0, sizeof(*vsoa_subscribe_param));
		strcpy(vsoa_subscribe_param->server_name_, tls_config->vsoa_sub_servername);
		strcpy(vsoa_subscribe_param->server_password_, tls_config->vsoa_sub_serverpass);
		strcpy(vsoa_subscribe_param->subscribe_url_, tls_config->vsoa_sub_url);
		vsoa_subscribe_param->arg = (void*)app_transmit;
		vsoa_subscribe_param->on_subscribe_receive_data = api_transmit_callback_on_tls_subscribe_message;

		// initialize the vsoa publish param
		vsoa_publish_param = &tls_server_param.publish_param_;
		memset(vsoa_publish_param, 0, sizeof(*vsoa_publish_param));
		strcpy(vsoa_publish_param->server_name_, tls_config->vsoa_pub_servername);
		strcpy(vsoa_publish_param->server_password_, tls_config->vsoa_pub_serverpass);
		strcpy(vsoa_publish_param->server_ip_, tls_config->vsoa_pub_serverip);
		vsoa_publish_param->server_port_ = tls_config->vsoa_pub_serverport;
		strcpy(vsoa_publish_param->url_, tls_config->vsoa_pub_url);
	
		tls_server = tls_server_allocate();
		assert(tls_server);
		memset(tls_server, 0, sizeof(*tls_server));
	
		result = tls_server_initialize(tls_server, &tls_server_param);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-TLS server initialize failed.");
			break;
		}
	
		LOG_INFO("TLS server initialize OK\n");

		result = tls_server_start(tls_server);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-TLS server start failed.");
			break;
		}		

		tls_server_register(tls_server, tls_server_param.tls_local_id_, tls_server_param.tls_local_sub_id_);

		tls_request_initialize((void*)tls_server);

	}while(0);

	if (TlsResultOk(result))
	{
		app_transmit->tls_server_ = (void*)tls_server;
		LOG_INFO("Server start OK\n");
	}
	else
	{
		tls_server_uninitialize(tls_server);
		app_transmit->tls_server_ = NULL;
	}
	return result;
}
void api_transmit_stop_tls_server(app_transmit_t* app_transmit)
{
	tls_server_t*		tls_server;

	if (NULL == app_transmit || NULL == app_transmit->tls_server_)
		return;

	tls_server = (tls_server_t*)app_transmit->tls_server_;
	tls_request_uninitialize((void*)tls_server);
	tls_server_stop(tls_server);
	tls_server_uninitialize(tls_server);
	tls_server_free(tls_server);
	app_transmit->tls_server_ = NULL;
}

TLS_RESULT api_transmit_start_app_rpc_server(app_transmit_t* app_transmit)
{
	TLS_RESULT					result = TLS_RESULT_E_FAIL;
	app_vsoa_rpc_server_t*		app_rpc_server;
	app_config_t*				app_config;
	app_rpc_server_config_t*	app_rpc_server_config;
	app_rpc_server_param_t		app_rpc_server_param;

	if (app_transmit->app_rpc_server_)
		api_transmit_stop_app_rpc_server(app_transmit);

	app_config = &app_transmit->app_config_;
	app_rpc_server_config = &app_config->rpc_server_config;

	memset(&app_rpc_server_param, 0, sizeof app_rpc_server_param);
	strcpy(app_rpc_server_param.server_name_, app_rpc_server_config->servername);	
	strcpy(app_rpc_server_param.server_password_, app_rpc_server_config->serverpass);	
	strcpy(app_rpc_server_param.server_ip_, app_rpc_server_config->serverip);	
	app_rpc_server_param.server_port_ = app_rpc_server_config->serverport;
	strcpy(app_rpc_server_param.listener_url_, app_rpc_server_config->listenerurl);	
	app_rpc_server_param.rpc_server_arg_ = app_transmit;
	app_rpc_server_param.rpc_server_command_ = api_transmit_callback_on_app_rpc_server_message;

	app_rpc_server = app_vsoa_rpc_server_allocate();
	if (NULL == app_rpc_server)
	{
		LOG_ERROR("ERROR-allocate rpc server entry fail.");
		return TLS_RESULT_E_ALLOCATE;
	}

	memset(app_rpc_server, 0, sizeof(*app_rpc_server));
	result = app_vsoa_rpc_server_start(&app_rpc_server_param, app_rpc_server);
	if (TlsResultOk(result))
		app_transmit->app_rpc_server_ = app_rpc_server;
	return result;
}
void api_transmit_stop_app_rpc_server(app_transmit_t* app_transmit)
{
	if (NULL == app_transmit || NULL == app_transmit->app_rpc_server_)
		return;	

	app_vsoa_rpc_server_stop(app_transmit->app_rpc_server_);
	app_vsoa_rpc_server_free(app_transmit->app_rpc_server_);
	app_transmit->app_rpc_server_ = NULL;
}
TLS_RESULT api_transmit_start_app_publish(app_transmit_t* app_transmit)
{
	TLS_RESULT					result = TLS_RESULT_E_FAIL;
	app_vsoa_publish_t*			app_publish;
	app_config_t*				app_config;
	app_publish_config_t*		app_publish_config;
	app_vsoa_publish_param_t	app_publish_param;


	if (app_transmit->app_publish_ != NULL)
		api_transmit_stop_app_publish(app_transmit);

	app_config = &app_transmit->app_config_;
	app_publish_config = &app_config->publish_config;

	memset(&app_publish_param, 0, sizeof app_publish_param);
	strcpy(app_publish_param.server_name_, app_publish_config->servername);	
	strcpy(app_publish_param.server_password_, app_publish_config->serverpass);	
	strcpy(app_publish_param.server_ip_, app_publish_config->serverip);	
	app_publish_param.server_port_ = app_publish_config->serverport;
	strcpy(app_publish_param.server_url_, app_publish_config->publishurl);

	app_publish = app_vsoa_publish_allocate();
	if (NULL == app_publish)
	{
		LOG_ERROR("ERROR-allocate publish server entry failed.");
		return TLS_RESULT_E_ALLOCATE;
	}
	memset(app_publish, 0, sizeof(*app_publish));
	result = app_vsoa_publish_start(&app_publish_param, app_publish);
	if (TlsResultOk(result))
		app_transmit->app_publish_ = app_publish;
	return result;
}
TLS_RESULT api_transmit_stop_app_publish(app_transmit_t* app_transmit)
{
	app_vsoa_publish_t*			app_publish;

	if (NULL == app_transmit || NULL == app_transmit->app_publish_)
		return TLS_RESULT_E_INVALID_DATA;

	app_publish = app_transmit->app_publish_;
	app_vsoa_publish_stop(app_publish);
	app_vsoa_publish_free(app_publish);
	app_transmit->app_publish_ = NULL;

	return TLS_RESULT_S_OK;
}
TLS_RESULT api_transmit_start_file_transmit(app_transmit_t* app_transmit)
{
	TLS_RESULT					result = TLS_RESULT_E_FAIL;
	file_transmit_t*			file_transmit;
	file_transmit_param_t		file_transmit_param;
	file_config_t*				file_config;

	if (app_transmit->file_transmit_ != NULL)
		api_transmit_stop_file_transmit(app_transmit);

	file_config = &app_transmit->file_config_;
	memset(&file_transmit_param, 0, sizeof file_transmit_param);
	strcpy(file_transmit_param.file_dir_, file_config->file_dir);
	file_transmit_param.file_block_count_ = file_config->file_block_count;
	file_transmit_param.file_block_size_ = file_config->file_block_size;
	
	strcpy(file_transmit_param.file_subscribe_name_, file_config->app_sub_servername);
	strcpy(file_transmit_param.file_subscribe_pass_, file_config->app_sub_serverpass);
	strcpy(file_transmit_param.file_subscribe_url_,  file_config->app_sub_url);

	file_transmit_param.file_send_interval_ = file_config->interval_ms_;
	file_transmit_param.file_publish_dst_addr_ = file_config->publish_dst_addr;
	file_transmit_param.file_publish_dst_entry_ = file_config->publish_dst_entry;
	strcpy(file_transmit_param.file_publish_url_,  file_config->publish_url);

	file_transmit = file_transmit_allocate();
	if (NULL == file_transmit)
	{
		LOG_ERROR("ERROR-allocate file transmit entry failed.");
		return TLS_RESULT_E_ALLOCATE;
	}
	memset(file_transmit, 0, sizeof(file_transmit_t));
	file_transmit->app_transmit_ = (void*)app_transmit;
	result = file_transmit_initialize(&file_transmit_param, file_transmit);
	if (TlsResultFail(result))
	{
		LOG_ERROR("ERROR-initialize file transmit failed.");
		return result;
	}
	result = file_transmit_start_service(file_transmit);
	if (TlsResultOk(result))
		app_transmit->file_transmit_ = (void*)file_transmit;
	return result;
}
TLS_RESULT api_transmit_stop_file_transmit(app_transmit_t* app_transmit)
{
	file_transmit_t*			file_transmit;

	if (NULL == app_transmit || NULL == app_transmit->file_transmit_)
		return TLS_RESULT_E_INVALID_DATA;

	file_transmit = app_transmit->file_transmit_;
	file_transmit_stop_service(file_transmit);
	file_transmit_uninitialize(file_transmit);
	file_transmit_free(file_transmit);
	app_transmit->file_transmit_ = NULL;

	return TLS_RESULT_S_OK;
}


void api_transmit_callback_on_tls_register(void* priv_dta, U32 status)
{
	if (TLS_REGISTER_OK == status)
	{
		LOG_DEBUG("On register ok...");
	}
	else
	{
		LOG_DEBUG("On register fail...");
	}
}
TLS_RESULT api_transmit_callback_on_tls_receive(void* priv_data, PU8 data, U32 size)
{
	LOG_DEBUG("TLS receive data size:%d.", size);

	return TLS_RESULT_S_OK;
}
void api_transmit_callback_on_tls_subscribe_message(void* arg, PU8 data, U32 size)
{
	app_transmit_t*				app_transmit;
	app_vsoa_publish_t*			app_publish;
	app_vsoa_publish_param_t*	app_publish_param;
	tls_vsoa_hdr_t*				tls_vsoa_hdr = NULL;
	vsoa_url_t					vsoa_url;
	vsoa_payload_t				vsoa_payload;

	char						urlbuf[128];
	char						publish_url[128];

	if (NULL == arg || NULL == data || 0 == size)
	{
		LOG_ERROR("ERROR-Invalid parameters");
		return;
	}


	app_transmit = (app_transmit_t*)arg;
	app_publish = app_transmit->app_publish_;
	app_publish_param = &app_publish->app_publish_param_;

	tls_vsoa_hdr = (tls_vsoa_hdr_t*)data;
	tls_vsoa_hdr->vsoa_url = tls_vsoa_hdr->tls_vsoa_data;
	tls_vsoa_hdr->vsoa_param = tls_vsoa_hdr->tls_vsoa_data + tls_vsoa_hdr->vsoa_url_size;
	tls_vsoa_hdr->vsoa_data = tls_vsoa_hdr->tls_vsoa_data + tls_vsoa_hdr->vsoa_url_size + tls_vsoa_hdr->vsoa_param_size;

	memset(urlbuf, 0, sizeof urlbuf);
	memcpy(urlbuf, tls_vsoa_hdr->vsoa_url, tls_vsoa_hdr->vsoa_url_size);
	urlbuf[tls_vsoa_hdr->vsoa_url_size] = '\0';

	memset(publish_url, 0, sizeof publish_url);
	if (strlen(app_publish_param->server_url_) > 0)
		strcat(publish_url, app_publish_param->server_url_);
	if ('/' == publish_url[strlen(publish_url)-1])
		publish_url[strlen(publish_url)-1] = '\0';
	strcat(publish_url, urlbuf);

	vsoa_url.url			= publish_url;
	vsoa_url.url_len		= strlen(publish_url);

	vsoa_payload.data		= tls_vsoa_hdr->vsoa_data;
	vsoa_payload.data_len	= tls_vsoa_hdr->vsoa_data_size;
	vsoa_payload.param		= tls_vsoa_hdr->vsoa_param;
	vsoa_payload.param_len  = tls_vsoa_hdr->vsoa_param_size;
     
    LOG_INFO("------------------------------ subcribe callback data:%s   size:%d \r\n",  data, size);
	app_vsoa_publish_data(app_publish, &vsoa_url, &vsoa_payload);
}
void api_transmit_callback_on_app_rpc_server_message(void* arg, vsoa_url_t* url, vsoa_payload_t* payload)
{
	app_transmit_t*				app_transmit;
	int							tls_dst_addr = 0;
	int							tls_dst_entry = 0;
	U8							tls_priority = 0;
	char						newurl_buf[128];
	TLS_RESULT					result;
	static link_map_config_t	s_link_map_config;
	link_map_entry_t*			entry = NULL;
	char						config_path[4096];

	if (NULL == arg)
		return;

	app_transmit = (app_transmit_t*)arg;

	tls_statis_rate_tx_input(payload->param_len + payload->data_len);

	result = api_transmit_app_rpc_server_parse_url(url, &tls_dst_addr, &tls_dst_entry, &tls_priority, newurl_buf);
	if (TlsResultFail(result))
	{
		LOG_ERROR("parse the rpc server url failed.");
		return;
	}

	LOG_DEBUG("parse the rpc server url, dstaddr:%d, dstentry:%d, newurl:%s", tls_dst_addr, tls_dst_entry, newurl_buf);

	// Load config on first call
	if (s_link_map_config.count == 0)
	{
		api_transmit_get_app_path(app_transmit->app_path_, sizeof(app_transmit->app_path_));
		strcpy(config_path, app_transmit->app_path_);
		strcat(config_path, "tlsconfig/link_map.json");
		if (tls_net_proxy_load_config_json(config_path, &s_link_map_config) == 0)
		{
			tls_net_proxy_init(&s_link_map_config);
			tls_net_proxy_set_server(app_transmit->tls_server_);
			tls_net_proxy_set_publish(app_transmit->app_publish_);
			tls_net_proxy_start_listening(&g_net_proxy);
			LOG_INFO("Net proxy config loaded from %s", config_path);
		}
		else
		{
			LOG_ERROR("Failed to load net proxy config from %s", config_path);
			return;
		}
	}

	// Look up destination by id
	entry = tls_net_proxy_find_entry(&s_link_map_config, tls_dst_addr);
	if (NULL == entry)
	{
		LOG_ERROR("Link ID %d not found in link map", tls_dst_addr);
		return;
	}

	LOG_INFO("VSOA RPC received, pushing to priority queue: id=%d, ip=%s, port=%d, proto=%d, priority=%d, topic=%s, data_size=%d",
		tls_dst_addr, entry->dest_ip, entry->dest_port, tls_dst_entry, tls_priority, newurl_buf, payload->data_len);

	// Push to priority queue for async TCP/UDP transmission
	// Parameters: dest_ip, dest_port, proto, priority, url, url_len, data, data_len, param, param_len
	tls_net_proxy_push_to_prio_queue(&g_net_proxy,
		entry->dest_ip, entry->dest_port,  // destination
		(link_proto_t)tls_dst_entry,       // protocol (1=UDP, 2=TCP)
		tls_priority,                      // priority (0-7, 7 is highest)
		newurl_buf, strlen(newurl_buf),     // topic URL for VSOA publish
		payload->data, payload->data_len,   // payload data
		payload->param, payload->param_len);// payload param
}
TLS_RESULT api_transmit_app_rpc_server_parse_url(const vsoa_url_t* url, U32* addr, U16* entry, U8* priority, char* newurl)
{
	TLS_RESULT				result;
	char					urlsrc[128];
	char					urlbuf[32][128];
	char					urlnew[128];
	int						index;
	int						count;
	int						NEED_TOPIC_COUNT = 4;  // /toLink/<id>/<proto>/<priority>/<topic>/...
	const char*				slash = "/";
	char*					token;

	do
	{
		if (NULL == url || NULL == addr || NULL == entry || NULL == priority || NULL == newurl)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			LOG_ERROR("ERROR-Invalid parameters");
			break;
		}
		if (0 == url->url_len || NULL == url->url || url->url_len >= sizeof(urlsrc))
		{
			result = TLS_RESULT_E_INVALID_DATA;
			LOG_ERROR("ERROR-Invalid url data");
			break;
		}

		memset(urlsrc, 0, sizeof urlsrc);
		memset(urlbuf, 0, sizeof urlbuf);

		memcpy(urlsrc, url->url, url->url_len);
		urlsrc[url->url_len] = '\0';


		index = 0;
		count = 0;
		token = strtok(urlsrc, slash);
		while (token != NULL)
		{
			strcpy(urlbuf[index], token);
			token = strtok(NULL, slash);
			index++;
			count++;
		}
		if (count < NEED_TOPIC_COUNT)
		{
			result = TLS_RESULT_E_INVALID_DATA;
			LOG_ERROR("ERROR-Invalid url topic, url is too short, source url:%s", urlsrc);
			break;
		}
		*addr = atoi(urlbuf[1]);        // id
		*entry = atoi(urlbuf[2]);      // proto (1=UDP, 2=TCP)
		*priority = (U8)atoi(urlbuf[3]); // priority (0-7, 7 is highest)
		memset(urlnew, 0, sizeof urlnew);
		for (index = NEED_TOPIC_COUNT; index < count; index++)
		{
			strcat(urlnew, "/");
			strcat(urlnew, urlbuf[index]);
		}
		strcpy(newurl, urlnew);
		result = TLS_RESULT_S_OK;
	}while(0);

	return result;
}



TLS_RESULT api_transmit_start_statis_rate(app_transmit_t* app_transmit)
{
	TLS_RESULT				result = TLS_RESULT_E_FAIL;
	tls_statis_rate_t*		statis_rate = NULL;
	
	do
	{
		if (NULL == app_transmit)
		{
			LOG_ERROR("ERROR-Invalid parameters\n");
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}
	
		if (app_transmit->statis_rate_ != NULL)
			api_transmit_stop_statis_rate(app_transmit);

		statis_rate = tls_statis_rate_allocate();
		if (NULL == statis_rate)
		{
			LOG_ERROR("ERROR-Allocate statis rate entry failed\n");
			result = TLS_RESULT_E_ALLOCATE;
			break;
		}
		memset(statis_rate, 0, sizeof(*statis_rate));
		result = tls_statis_rate_start(statis_rate);
		if (TlsResultFail(result))
		{
			LOG_ERROR("ERROR-start statis rate failed\n");
			break;
		}
		app_transmit->statis_rate_ = statis_rate;
	}while(0);
	if (TlsResultFail(result))
	{
		if (statis_rate)
		{
			tls_statis_rate_stop(statis_rate);
			tls_statis_rate_free(statis_rate);
		}
		app_transmit->statis_rate_ = NULL;
	}
	return result;
}
void api_transmit_stop_statis_rate(app_transmit_t* app_transmit)
{
	if (NULL == app_transmit || NULL == app_transmit->statis_rate_)
		return;

	tls_statis_rate_stop((tls_statis_rate_t*)app_transmit->statis_rate_);
	tls_statis_rate_free((tls_statis_rate_t*)app_transmit->statis_rate_);
	app_transmit->statis_rate_ = NULL;
}


TLS_RESULT api_transmit_start_app_subscribe(app_transmit_t* app_transmit)
{
	TLS_RESULT						result = TLS_RESULT_E_FAIL;
	app_vsoa_subscribe_t*			app_subscribe;
	app_config_t*					app_config;
	app_subscribe_config_t*			app_subscribe_config;
	app_vsoa_subscribe_param_t		app_subscribe_param;

	if (app_transmit->app_subscribe_ != NULL)
		api_transmit_stop_app_subscribe(app_transmit);

	app_config = &app_transmit->app_config_;
	app_subscribe_config = &app_config->subscribe_config;

	memset(&app_subscribe_param, 0, sizeof app_subscribe_param);
	strcpy(app_subscribe_param.server_name_, app_subscribe_config->servername);
	strcpy(app_subscribe_param.server_password_, app_subscribe_config->serverpass);
	strcpy(app_subscribe_param.subscribe_url_, app_subscribe_config->subscribeurl);
	app_subscribe_param.app_subscribe_arg_ = (void*)app_transmit;
	app_subscribe_param.on_app_subscribe_connect = api_transmit_callback_on_app_subscribe_connect;
	app_subscribe_param.on_app_subscribe_message = api_transmit_callback_on_app_subscribe_message;

	app_subscribe = app_vsoa_subscribe_allocate();
	if (NULL == app_subscribe)
	{
		LOG_ERROR("ERROR-allocate subscribe client entry failed.");
		return TLS_RESULT_E_ALLOCATE;
	}
	memset(app_subscribe, 0, sizeof(*app_subscribe));

	result = app_vsoa_subscribe_start(&app_subscribe_param, app_subscribe);
	if (TlsResultOk(result))
		app_transmit->app_subscribe_ = app_subscribe;
	return result;
}
TLS_RESULT api_transmit_stop_app_subscribe(app_transmit_t* app_transmit)
{
	app_vsoa_subscribe_t*		app_subscribe;
	if (NULL == app_transmit || NULL == app_transmit->app_subscribe_)
		return TLS_RESULT_E_INVALID_DATA;
	
	app_subscribe = app_transmit->app_subscribe_;
	app_vsoa_subscribe_stop(app_subscribe);
	app_vsoa_subscribe_free(app_subscribe);
	app_transmit->app_subscribe_ = NULL;

	return TLS_RESULT_S_OK;
}
TLS_RESULT api_transmit_start_net_proxy(app_transmit_t* app_transmit)
{
	TLS_RESULT					result = TLS_RESULT_E_FAIL;
	char						config_path[4096];
	link_map_config_t			link_map_config;

	if (NULL == app_transmit)
		return TLS_RESULT_E_INVALID_PARAM;

	memset(&link_map_config, 0, sizeof(link_map_config_t));
	strcpy(config_path, app_transmit->app_path_);
	strcat(config_path, "tlsconfig/link_map.json");

	if (tls_net_proxy_load_config_json(config_path, &link_map_config) == 0)
	{
		tls_net_proxy_init(&link_map_config);
		tls_net_proxy_set_server(app_transmit->tls_server_);
		tls_net_proxy_set_publish(app_transmit->app_publish_);  // 新增
		if (tls_net_proxy_start_listening(&g_net_proxy) == 0)
		{
			LOG_INFO("Net proxy started, TCP port %d, UDP port %d",
				link_map_config.listen_tcp_port, link_map_config.listen_udp_port);
			result = TLS_RESULT_S_OK;
		}
		else
		{
			LOG_ERROR("Net proxy start listening failed");
		}
	}
	else
	{
		LOG_ERROR("Net proxy load config failed: %s", config_path);
	}

	return result;
}
void api_transmit_callback_on_app_subscribe_connect(void* arg, int connect)
{
	__TLSDBG("On subscribe connect:%d.", connect);
}
void api_transmit_callback_on_app_subscribe_message(void* arg, vsoa_url_t* url, vsoa_payload_t* payload)
{
	app_transmit_t*				app_transmit;
	tls_vsoa_sesstion_t			tls_vsoa_session;
	int							tls_dst_addr = 0;
	int							tls_dst_entry = 0;
	U8							tls_priority = 0;
	char						newurl_buf[128];
	vsoa_url_t					new_url;
	TLS_RESULT					result;

	if (NULL == arg)
		return;

	app_transmit = (app_transmit_t*)arg;


	tls_statis_rate_tx_input(payload->param_len + payload->data_len);


	result = api_transmit_app_rpc_server_parse_url(url, &tls_dst_addr, &tls_dst_entry, &tls_priority, newurl_buf);
	if (TlsResultFail(result))
	{
		LOG_ERROR("parse the rpc server url failed.");
		return;
	}

	LOG_DEBUG("parse the rpc server url, dstaddr:%d, dstentry:%d, newurl:%s", tls_dst_addr, tls_dst_entry, newurl_buf);
	memset(&tls_vsoa_session, 0, sizeof tls_vsoa_session);
	tls_vsoa_session.tls_dest_addr_ = tls_dst_addr;
	tls_vsoa_session.tls_dest_entry_ = tls_dst_entry;

	memset(&new_url, 0, sizeof(new_url));
	new_url.url = newurl_buf;
	new_url.url_len = strlen(newurl_buf);

	tls_vsoa_session.vsoa_url_ = &new_url;
	tls_vsoa_session.vsoa_payload_ = payload;
	
	tls_server_vsoa_publish_transmit((tls_server_t*)app_transmit->tls_server_, &tls_vsoa_session);
}
