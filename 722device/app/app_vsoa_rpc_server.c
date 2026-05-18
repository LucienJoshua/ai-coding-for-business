
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "tls_log.h"
#include "app_transmit.h"
#include "app_vsoa_rpc_server.h"
#include "app_linkinfo.h"
#include "tls_linkinfo.h"

static TLS_RESULT	app_vsoa_rpc_server_valid_parameter(const app_rpc_server_param_t* param);
static void			app_vsoa_rpc_server_command(void* arg,vsoa_server_t* server,vsoa_cli_id_t cid,vsoa_header_t* vsoa_hdr,
					vsoa_url_t* url,vsoa_payload_t* payload);

static TLS_RESULT	app_vsoa_rpc_server_start_thread(app_vsoa_rpc_server_t* rpc_server);
static void			app_vsoa_rpc_server_stop_thread(app_vsoa_rpc_server_t* rpc_server);
static void*		app_vsoa_rpc_server_thread_work(void* arg);
static void			app_vsoa_rpc_server_param_show(const app_rpc_server_param_t* param);

static void			app_vsoa_rpc_server_on_linkdeviceinfo(void* arg,vsoa_server_t* server,vsoa_cli_id_t cid,
					vsoa_header_t* vsoa_hdr,vsoa_url_t* url,vsoa_payload_t* payload);




app_vsoa_rpc_server_t*	app_vsoa_rpc_server_allocate(void)
{
	app_vsoa_rpc_server_t*	rpc_server = NULL;
	rpc_server = (app_vsoa_rpc_server_t*)malloc(sizeof(app_vsoa_rpc_server_t));
	return rpc_server;
}
void app_vsoa_rpc_server_free(app_vsoa_rpc_server_t* rpc_server)
{
	if (rpc_server)
		free(rpc_server);
}

TLS_RESULT app_vsoa_rpc_server_start(const app_rpc_server_param_t* rpc_server_param, app_vsoa_rpc_server_t* rpc_server)
{
	TLS_RESULT				result; 
	int						oldvalue;
	int						newvalue;
	int						ret;
	struct sockaddr_in		server_addr;
	char					server_name[256] = { 0 };
	vsoa_url_t				vsoa_url;

	do
	{
		if (NULL == rpc_server_param || NULL == rpc_server)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			LOG_ERROR("ERROR-Invalid parameters.");
			break;
		}
		oldvalue = 0;
		newvalue = 1;
		if (__sync_val_compare_and_swap(&rpc_server->rpc_server_start_, oldvalue, newvalue))
		{
			result = TLS_RESULT_S_CONTINUE;
			LOG_INFO("VSOA rpc server has already started.");
			break;
		}
		result = app_vsoa_rpc_server_valid_parameter(rpc_server_param);
		if (TlsResultFail(result))
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			LOG_ERROR("ERROR-VSOA app rpc server invalid parameters.");
			app_vsoa_rpc_server_param_show(rpc_server_param);
			break;
		}


		memcpy(&rpc_server->rpc_server_param_, rpc_server_param, sizeof(*rpc_server_param));
		memset(&server_addr, 0, sizeof server_addr);
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(rpc_server_param->server_port_);
		if (0 == strlen(rpc_server_param->server_ip_))
		{
			server_addr.sin_addr.s_addr = INADDR_ANY;
		}
		else
		{
			server_addr.sin_addr.s_addr = inet_addr(rpc_server_param->server_ip_);
		}

		LOG_DEBUG("----------------------- app rpc server config -----------------------");
		LOG_DEBUG("app rpc server name:%s.", rpc_server_param->server_name_);
		LOG_DEBUG("app rpc server password:%s.", rpc_server_param->server_password_);
		LOG_DEBUG("app rpc server ip:%s.", rpc_server_param->server_ip_);
		LOG_DEBUG("app rpc server port:%d.", rpc_server_param->server_port_);
		LOG_DEBUG("app rpc server listener url:%s.", rpc_server_param->listener_url_);
		LOG_DEBUG("--------------------------------------------------------------------");


		sprintf(server_name, "{\"name\":\"%s\"}", rpc_server_param->server_name_);
		rpc_server->vsoa_rpc_server_ = vsoa_server_create(server_name);
		if (NULL == rpc_server->vsoa_rpc_server_)
		{
			result = TLS_RESULT_E_FAIL;
			LOG_ERROR("ERROR-VSOA app rpc server create failed.");
			break;
		}
		if (strlen(rpc_server_param->server_password_) > 0)
			vsoa_server_passwd(rpc_server->vsoa_rpc_server_, rpc_server_param->server_password_);

		vsoa_url.url = rpc_server->rpc_server_param_.listener_url_;
		vsoa_url.url_len = strlen(rpc_server->rpc_server_param_.listener_url_);
		ret = vsoa_server_add_listener(rpc_server->vsoa_rpc_server_, &vsoa_url, app_vsoa_rpc_server_command, (void*)rpc_server);
		if (!ret)
		{
			result = TLS_RESULT_E_FAIL;
			LOG_ERROR("ERROR-VSOA app rpc server add listener url failed, url:%s.", vsoa_url.url);
			break;
		}

		vsoa_url.url = "/deviceinfo/";
		vsoa_url.url_len = strlen(vsoa_url.url);
		ret = vsoa_server_add_listener(rpc_server->vsoa_rpc_server_, &vsoa_url, 
				app_vsoa_rpc_server_on_linkdeviceinfo, 	(void*)rpc_server_param->rpc_server_arg_);
		if (!ret)
		{
			result = TLS_RESULT_E_FAIL;
			LOG_ERROR("ERROR-VSOA app rpc server add listener url failed, url:%s.", vsoa_url.url);
			break;
		}

		ret = vsoa_server_start(rpc_server->vsoa_rpc_server_, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in));
		if (!ret)
		{
			result = TLS_RESULT_E_FAIL;
			LOG_ERROR("ERROR-VSOA app rpc server start failed.");
			break;
		}

		result = app_vsoa_rpc_server_start_thread(rpc_server);
		if (TlsResultFail(result))
		{
			result = TLS_RESULT_E_FAIL;
			LOG_ERROR("ERROR-VSOA app rpc server start work thread fail.");
			break;
		}
		LOG_DEBUG("app vsoa rpc server start ok!.\n");
	}while (0);
	if (TlsResultFail(result))
	{
		app_vsoa_rpc_server_stop(rpc_server);
	}
	return result;
}
void app_vsoa_rpc_server_stop(app_vsoa_rpc_server_t* rpc_server)
{
	int					oldvalue;
	int					newvalue;


	if (NULL == rpc_server)
		return;

	oldvalue = 1;
	newvalue = 0;
	if (!__sync_val_compare_and_swap(&rpc_server->rpc_server_start_, oldvalue, newvalue))
	{
		LOG_INFO("VSOA app rpc server has already stop.\n");
		return;
	}

	app_vsoa_rpc_server_stop_thread(rpc_server);

	if (rpc_server->vsoa_rpc_server_)
	{
		vsoa_server_close(rpc_server->vsoa_rpc_server_);
		rpc_server->vsoa_rpc_server_ = NULL;
	}
}



TLS_RESULT app_vsoa_rpc_server_valid_parameter(const app_rpc_server_param_t* param)
{
	TLS_RESULT				result = TLS_RESULT_E_INVALID_PARAM;
	do
	{
		if (NULL == param)
			break;
		if (0 == strlen(param->server_name_) || 0 == strlen(param->listener_url_))
			break;
		if (0 == param->server_port_ || param->server_port_ > 65536)
			break;
		result = TLS_RESULT_S_OK;

	}while (0);
	return result;
}
TLS_RESULT app_vsoa_rpc_server_start_thread(app_vsoa_rpc_server_t* rpc_server)
{
	if (NULL == rpc_server)
		return TLS_RESULT_E_INVALID_PARAM;

	tls_event_initialize(&rpc_server->evt_loop_);
	tls_event_initialize(&rpc_server->evt_stop_);

	memset(&rpc_server->thread_rpc_server_, 0, sizeof(rpc_server->thread_rpc_server_));
	rpc_server->thread_rpc_server_.run_ = 1;
	rpc_server->thread_rpc_server_.thread_routine_ = app_vsoa_rpc_server_thread_work;
	rpc_server->thread_rpc_server_.arg_ = rpc_server;
	rpc_server->run_ = 1;

	tls_event_set(&rpc_server->evt_loop_);
	tls_thread_start(&rpc_server->thread_rpc_server_);
	
	return TLS_RESULT_S_OK;
}
void app_vsoa_rpc_server_stop_thread(app_vsoa_rpc_server_t* rpc_server)
{
	if (NULL == rpc_server)	
		return;

	rpc_server->run_ = 0;
	tls_event_set(&rpc_server->evt_loop_);
	tls_event_timewait(&rpc_server->evt_stop_, 3000);
	tls_thread_stop(&rpc_server->thread_rpc_server_);

	tls_event_uninitialize(&rpc_server->evt_loop_);
	tls_event_uninitialize(&rpc_server->evt_stop_);
}
void* app_vsoa_rpc_server_thread_work(void* arg)
{
	app_vsoa_rpc_server_t*	rpc_server;
	struct timespec			timeout = {1, 0};
	int						cnt;
	int						max_fd;
	fd_set					fds;

	if (NULL == arg)
		return NULL;

	rpc_server = (app_vsoa_rpc_server_t*)arg;

	tls_event_reset(&rpc_server->evt_stop_);
	while (rpc_server->run_)
	{
		tls_event_wait(&rpc_server->evt_loop_);
		if (!rpc_server->run_)
			break;

		FD_ZERO(&fds);
		max_fd = vsoa_server_fds(rpc_server->vsoa_rpc_server_, &fds);
		cnt = pselect(max_fd+1, &fds, NULL, NULL, &timeout, NULL);
		if (cnt > 0)
		{
			vsoa_server_input_fds(rpc_server->vsoa_rpc_server_, &fds);
		}

	}
	tls_event_set(&rpc_server->evt_stop_);

	return NULL;
}
void app_vsoa_rpc_server_param_show(const app_rpc_server_param_t* param)
{
	const app_rpc_server_param_t* rpc_server_param = param;
	LOG_DEBUG("rpc server name:%s.", rpc_server_param->server_name_);
	LOG_DEBUG("rpc server password:%s.", rpc_server_param->server_password_);
	LOG_DEBUG("rpc server ip:%s.", rpc_server_param->server_ip_);
	LOG_DEBUG("rpc server port:%d.", rpc_server_param->server_port_);
	LOG_DEBUG("rpc server listener url:%s.", rpc_server_param->listener_url_);
}
void app_vsoa_rpc_server_command(void* arg,vsoa_server_t* server,vsoa_cli_id_t cid,vsoa_header_t* vsoa_hdr,vsoa_url_t* url,vsoa_payload_t* payload)
{
	app_vsoa_rpc_server_t*			rpc_server;
	app_rpc_server_param_t*			rpc_server_param;
	vsoa_payload_t					response;
	uint32_t						seqno;

	if (NULL == arg)
		return;

	rpc_server = (app_vsoa_rpc_server_t*)arg;
	rpc_server_param = &rpc_server->rpc_server_param_;
	if (rpc_server_param->rpc_server_command_)
	{
		rpc_server_param->rpc_server_command_(rpc_server_param->rpc_server_arg_, url, payload);
	}

	seqno = vsoa_parser_get_seqno(vsoa_hdr);
	response.data = NULL;
	response.data_len = 0;
	response.param = NULL;
	response.param_len = 0;

	vsoa_server_cli_reply(server, cid, 0, seqno, 0, &response);
}



void app_vsoa_rpc_server_on_linkdeviceinfo(void* arg,vsoa_server_t* server,vsoa_cli_id_t cid, vsoa_header_t* vsoa_hdr,vsoa_url_t* url,vsoa_payload_t* payload)
{
	app_transmit_t*				app_transmit;
	vsoa_payload_t				response;
	uint32_t					seqno;


	app_linkinfo_request_t		app_linkinfo_request;
	app_linkinfo_result_t		app_linkinfo_result;

	app_transmit = (app_transmit_t*)arg;

	if (NULL == payload || NULL == payload->data || 0 == payload->data_len)
	{
		LOG_INFO("rpc server on link info, the payload data is null.");
		return;
	}
	if (payload->data_len < sizeof(app_linkinfo_request))
	{
		LOG_INFO("payload data len is not we need.");
		return;
	}
	
	memset(&app_linkinfo_request, 0, sizeof(app_linkinfo_request));
	memset(&app_linkinfo_result, 0, sizeof(app_linkinfo_result));
	memcpy(&app_linkinfo_request, payload->data, sizeof(app_linkinfo_request));
	
	tls_linkinfo(app_transmit, &app_linkinfo_request, &app_linkinfo_result);

	seqno = vsoa_parser_get_seqno(vsoa_hdr);

	response.data = &app_linkinfo_result;
	response.data_len = sizeof(app_linkinfo_result_t);
	response.param = NULL;
	response.param_len = 0;

	printf("response data len:%d\n", response.data_len);
	vsoa_server_cli_reply(server, cid, 0, seqno, 0, &response);
}
