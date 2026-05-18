#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "tls_config.h"
#include "tls_server.h"
#include "vsoa_server.h"
#include "app_config.h"
#include "app_udp_endpoint.h"
#include "app_vsoa_rpc_server.h"
#include "app_vsoa_publish.h"
#include "tls_vsoa.h"
#include "app_transport.h"

static tls_server_t*				g_tls_server = NULL;
static app_udp_endpoint_t*			g_udp_endpoint = NULL;
static app_vsoa_rpc_server_t*		s_app_rpc_server = NULL;
static app_vsoa_publish_t*			s_app_publish = NULL;



static TLS_RESULT	api_tls_server_start(void);
static void			api_tls_server_stop(void);

static TLS_RESULT	api_udp_endpoint_start(void);
static void			api_udp_endpoint_stop(void);

static void			callback_on_tls_register(void* priv_dta, U32 status);
static TLS_RESULT	callback_on_tls_receive(void* priv_data, PU8 data, U32 size);
static void			callback_on_udp_receive(void* userdata, PU8 data, U32 size);

static void			callback_on_tls_subscribe_message(void* arg, vsoa_url_t* url, vsoa_payload_t* payload);
static void			callback_on_tls_subscribe_receive(void* arg, PU8 data, U32 size);
static void			callback_on_app_rpc_server_command(void* arg, vsoa_url_t* url, vsoa_payload_t* payload);




static void			api_tls_server_publish_test(tls_server_t* tls_server);

static TLS_RESULT	api_rpc_server_start(void);
static void			api_rpc_server_stop(void);


static TLS_RESULT	api_publish_server_start(void);
static void			api_publish_server_stop(void);
static void			api_publish_server_test(void);


TLS_RESULT api_transport_initialize(void)
{
	TLS_RESULT			result = TLS_RESULT_E_FAIL;
	do
	{
		result = api_tls_server_start();
		if (TlsResultFail(result))
		{
			__TLSDBG("TLS  server start fail.\n");
			break;
		}
		result = api_udp_endpoint_start();
		if (TlsResultFail(result))
		{
			__TLSDBG("UDP Endpoint start fail.\n");
			break;
		}
		result = api_publish_server_start();
		if (TlsResultFail(result))
		{
			__TLSDBG("App VSOA publish server start fail.\n");
			break;
		}
		result = api_rpc_server_start();
		if (TlsResultFail(result))
		{
			__TLSDBG("App VSOA rpc server start fail.\n");
			break;
		}
	}while(0);
	if (TlsResultFail(result))
	{
		api_transport_uninitialize();
	}
	return result;
}

void api_transport_uninitialize(void)
{
	api_rpc_server_stop();
	api_publish_server_stop();
	api_tls_server_stop();
	api_udp_endpoint_stop();
}




TLS_RESULT	api_tls_server_start(void)
{
	TLS_RESULT				result = TLS_RESULT_E_FAIL;
	tls_config_t			config;
	tls_param_t				param;
	struct tls_vsoa_subscribe_param* vsoa_subscribe_param = NULL;
	struct tls_vsoa_publish_param*	 vsoa_publish_param = NULL;
	tls_server_t*			server = NULL;

	__TLSDBG("------------------ tls ------------------.\n");
	do
	{
		if (g_tls_server)
			api_tls_server_stop();
		
		tls_config_load_all("./tls_config.xml", &config);
	
		memset(&param, 0, sizeof(param));
		strcpy(param.local_ip_, config.tls_local_ip);
		param.local_port_ = config.tls_local_port;
		strcpy(param.remote_ip_, config.tls_remote_ip);
		param.remote_port_ = config.tls_remote_port;
		param.tls_local_id_ = config.tls_local_id;
		param.tls_local_sub_id_ = config.tls_local_sub_id;
		param.priv_data_ = NULL;
	
		param.on_tls_register_callback = callback_on_tls_register;
		param.on_tls_receive_callback = callback_on_tls_receive;	


		vsoa_subscribe_param = &param.subscribe_param_;
		memset(vsoa_subscribe_param, 0, sizeof(*vsoa_subscribe_param));
		strcpy(vsoa_subscribe_param->server_name_, config.vsoa_sub_servername);
		strcpy(vsoa_subscribe_param->server_password_, config.vsoa_sub_serverpass);
		strcpy(vsoa_subscribe_param->subscribe_url_, config.vsoa_sub_url);
		vsoa_subscribe_param->arg = NULL;
		vsoa_subscribe_param->on_subscribe_receive_data = callback_on_tls_subscribe_receive;

		vsoa_publish_param = &param.publish_param_;
		memset(vsoa_publish_param, 0, sizeof(*vsoa_publish_param));
		strcpy(vsoa_publish_param->server_name_, config.vsoa_pub_servername);
		strcpy(vsoa_publish_param->server_password_, config.vsoa_pub_serverpass);
		strcpy(vsoa_publish_param->server_ip_, config.vsoa_pub_serverip);
		vsoa_publish_param->server_port_ = config.vsoa_pub_serverport;
		// memset(vsoa_publish_param->url_, 0, sizeof(vsoa_publish_param->url_));
		strcpy(vsoa_publish_param->url_, config.vsoa_pub_url);
	
		server = tls_server_allocate();
		assert(server);
		memset(server, 0, sizeof(*server));
	
		result = tls_server_initialize(server, &param);
		if (TlsResultFail(result))
		{
			__TLSDBG("server init fail.\n");
			break;
		}
	
		__TLSDBG("Server initialize OK\n");

		result = tls_server_start(server);
		if (TlsResultFail(result))
		{
			__TLSDBG("server start fail.\n");
			break;
		}		

		tls_server_register(server, param.tls_local_id_, param.tls_local_sub_id_);

	}while(0);

	if (TlsResultOk(result))
	{
		g_tls_server = server;
		__TLSDBG("Server start OK\n");
	}
	else
	{
		tls_server_uninitialize(server);
		g_tls_server = NULL;
	}
	return result;
}
void api_tls_server_stop(void)
{
	tls_server_t*		server;

	if (NULL == g_tls_server)
		return;

	server = g_tls_server;
	tls_server_stop(server);
	tls_server_uninitialize(server);
	tls_server_free(server);
	g_tls_server = NULL;
}
TLS_RESULT	api_udp_endpoint_start(void)
{
	TLS_RESULT				result = TLS_RESULT_E_FAIL;
	app_udp_endpoint_t*		udp_endpoint = NULL;
	app_udp_config_t		udp_config;
	app_udp_param_t			udp_param;

	if (g_udp_endpoint)
		api_udp_endpoint_stop();
	memset(&udp_param, 0, sizeof(udp_param));
	memset(&udp_config, 0, sizeof(udp_config));

	api_udp_endpoint_config_load("./udp_config.xml", &udp_config);
	strcpy(udp_param.local_ip_, udp_config.local_ip);
	udp_param.local_port_ = udp_config.local_port;
	strcpy(udp_param.remote_ip_, udp_config.remote_ip);
	udp_param.remote_port_ = udp_config.remote_port;
	udp_param.udp_endpoint_receive = callback_on_udp_receive;
	udp_param.userdata = NULL;

	udp_endpoint = udp_endpoint_create(&udp_param);
	if (NULL == udp_endpoint)
		return TLS_RESULT_E_FAIL;
	g_udp_endpoint = udp_endpoint;

	return TLS_RESULT_S_OK;
}
void api_udp_endpoint_stop(void)
{
	if (NULL == g_udp_endpoint)
		return;
	udp_endpoint_delete(g_udp_endpoint);
	g_udp_endpoint = NULL;
}




void callback_on_tls_register(void* priv_dta, U32 status)
{
	if (TLS_REGISTER_OK == status)
	{
		__TLSDBG("On register ok...\n");
	}
	else
	{
		__TLSDBG("On register fail...\n");
	}
}
TLS_RESULT	callback_on_tls_receive(void* priv_data, PU8 data, U32 size)
{
	tls_server_t*		server;

	__TLSDBG("TLS receive data size:%d.\n", size);

	assert(g_tls_server);
	server = g_tls_server;

	udp_endpoint_send(g_udp_endpoint, data, size);

	return TLS_RESULT_S_OK;
}
void callback_on_udp_receive(void* userdata, PU8 data, U32 size) 
{
	tls_session_t		session;
	app_udp_param_t*	udp_param;

	__TLSDBG("UDP receive data size:%d.\n", size);
	if (NULL == g_udp_endpoint)
		return;
	udp_param = &g_udp_endpoint->udp_param_;
	memset(&session, 0, sizeof(session));
	session.tls_remote_id_ = udp_param->tls_remote_id_;
	session.tls_remote_sub_id_ = udp_param->tls_remote_sub_id_;

	tls_server_transmit(g_tls_server, &session, data, size);
}
void callback_on_tls_subscribe_message(void* arg, vsoa_url_t* url, vsoa_payload_t* payload)
{
	tls_vsoa_hdr_t*			tls_vsoa_hdr = NULL;
	
	vsoa_url_t				vsoa_url;
	vsoa_payload_t			vsoa_payload;

	if (NULL == url || NULL == payload)
		return;

	tls_vsoa_hdr = payload->data;

	__TLSDBG("url size:%d, param size:%d, data size:%d.\n", 
			(int)tls_vsoa_hdr->vsoa_url_size, (int)tls_vsoa_hdr->vsoa_param_size, (int)tls_vsoa_hdr->vsoa_data_size);

	vsoa_url.url			= tls_vsoa_hdr->vsoa_url;
	vsoa_url.url_len		= tls_vsoa_hdr->vsoa_url_size;
	vsoa_payload.data		= tls_vsoa_hdr->vsoa_data;
	vsoa_payload.data_len	= tls_vsoa_hdr->vsoa_data_size;
	vsoa_payload.param		= tls_vsoa_hdr->vsoa_param;
	vsoa_payload.param_len	= tls_vsoa_hdr->vsoa_param_size;

	app_vsoa_publish_data(s_app_publish, &vsoa_url, &vsoa_payload);
}
void callback_on_tls_subscribe_receive(void* arg, PU8 data, U32 size)
{
	tls_vsoa_hdr_t*			tls_vsoa_hdr = NULL;
	
	vsoa_url_t				vsoa_url;
	vsoa_payload_t			vsoa_payload;

	if (NULL == data || NULL == size)
		return;

	tls_vsoa_hdr = (tls_vsoa_hdr_t*)data;

	vsoa_url.url			= tls_vsoa_hdr->vsoa_url;
	vsoa_url.url_len		= tls_vsoa_hdr->vsoa_url_size;
	vsoa_payload.data		= tls_vsoa_hdr->vsoa_data;
	vsoa_payload.data_len	= tls_vsoa_hdr->vsoa_data_size;
	vsoa_payload.param		= tls_vsoa_hdr->vsoa_param;
	vsoa_payload.param_len  = tls_vsoa_hdr->vsoa_param_size;

	app_vsoa_publish_data(s_app_publish, &vsoa_url, &vsoa_payload);
}
void api_tls_server_publish_test(tls_server_t* tls_server)
{
	vsoa_url_t			url;
	vsoa_payload_t		payload;
	char				param[100];
	int					roll = 1, pitch = 1, yaw = 1;

	if (NULL == tls_server)
		return;

	url.url="/csfw54_sub";
	url.url_len = strlen(url.url);
	payload.data = NULL;
	payload.data_len = 0;
	payload.param = param;

	payload.param_len = snprintf(param, 100, 
				"{\"roll\":%d, \"pitch\":%d, \"yaw\":%d}", 
				roll++, pitch++, yaw++);

	tls_server_vsoa_publish(tls_server, &url, &payload, 0);
}
void api_transport_tls_publish_test(void)
{
	api_tls_server_publish_test(g_tls_server);
}
void api_transport_app_publish_test(void)
{
	api_publish_server_test();
}


TLS_RESULT api_rpc_server_start(void)
{
	TLS_RESULT					result = TLS_RESULT_E_FAIL;
	app_config_t				app_config;
	app_rpc_server_config_t*	rpc_server_config;
	app_rpc_server_param_t		rpc_server_param;
	app_vsoa_rpc_server_t*		rpc_server_entry;

	if (s_app_rpc_server)
		api_rpc_server_stop();

	memset(&app_config, 0, sizeof app_config);
	api_app_config_load_all("./app_config.xml", &app_config);
	rpc_server_config = &app_config.rpc_server_config;

	memset(&rpc_server_param, 0, sizeof rpc_server_param);
	strcpy(rpc_server_param.server_name_, rpc_server_config->servername);	
	strcpy(rpc_server_param.server_password_, rpc_server_config->serverpass);	
	strcpy(rpc_server_param.server_ip_, rpc_server_config->serverip);	
	rpc_server_param.server_port_ = rpc_server_config->serverport;
	strcpy(rpc_server_param.listener_url_, rpc_server_config->listenerurl);	
	rpc_server_param.rpc_server_command_ = callback_on_app_rpc_server_command;

	rpc_server_entry = app_vsoa_rpc_server_allocate();
	if (NULL == rpc_server_entry)
	{
		__TLSDBG("allocate rpc server entry fail.\n");
		return TLS_RESULT_E_ALLOCATE;
	}

	memset(rpc_server_entry, 0, sizeof(*rpc_server_entry));
	result = app_vsoa_rpc_server_start(&rpc_server_param, rpc_server_entry);
	if (TlsResultOk(result))
		s_app_rpc_server = rpc_server_entry;
	return result;

}
void api_rpc_server_stop(void)
{
	if (NULL == s_app_rpc_server)
		return;

	app_vsoa_rpc_server_stop(s_app_rpc_server);
	app_vsoa_rpc_server_free(s_app_rpc_server);
	s_app_rpc_server = NULL;
}
void callback_on_app_rpc_server_command(void* arg, vsoa_url_t* url, vsoa_payload_t* payload)
{
#if 0
	__TLSDBG("urllen:%d, payload paramsize:%d, payload:datasize:%d.\n", url->url_len,  payload->param_len, payload->data_len);
#endif

	// tls_server_vsoa_publish(g_tls_server, url, payload, 0);
	tls_server_vsoa_publish_overtls(g_tls_server, url, payload, 0);
}

TLS_RESULT api_publish_server_start(void)
{
	TLS_RESULT					result = TLS_RESULT_E_FAIL;
	app_config_t				app_config;
	app_publish_config_t*		app_publish_config;
	app_vsoa_publish_param_t	app_publish_param;
	app_vsoa_publish_t*			app_publish_entry;


	if (s_app_publish)
		api_publish_server_stop();

	memset(&app_config, 0, sizeof app_config);
	api_app_config_load_all("./app_config.xml", &app_config);
	app_publish_config = &app_config.publish_config;

	memset(&app_publish_param, 0, sizeof app_publish_param);
	strcpy(app_publish_param.server_name_, app_publish_config->servername);	
	strcpy(app_publish_param.server_password_, app_publish_config->serverpass);	
	strcpy(app_publish_param.server_ip_, app_publish_config->serverip);	
	app_publish_param.server_port_ = app_publish_config->serverport;

	app_publish_entry = app_vsoa_publish_allocate();
	if (NULL == app_publish_entry)
	{
		__TLSDBG("allocate publish server entry fail.\n");
		return TLS_RESULT_E_ALLOCATE;
	}
	memset(app_publish_entry, 0, sizeof(*app_publish_entry));
	result = app_vsoa_publish_start(&app_publish_param, app_publish_entry);
	if (TlsResultOk(result))
		s_app_publish = app_publish_entry;
	return result;
}
void api_publish_server_stop(void)
{
	if (NULL == s_app_publish)
		return;
	app_vsoa_publish_stop(s_app_publish);
	app_vsoa_publish_free(s_app_publish);
	s_app_publish = NULL;
}
void api_publish_server_test(void)
{
	vsoa_url_t			url;
	vsoa_payload_t		payload;
	char				param[1024] = {0};
	char				data[1024] = {0x12};
	static int			msg_seq = 1;

	if (NULL == s_app_publish)
		return;

	url.url="/app_client";
	url.url_len = strlen(url.url);
	payload.data = data;
	payload.data_len = 512;
	// payload.param = param;
	// sprintf(param, "This is a test message on publish-subscribe, msg_seq:%d.\n", msg_seq++);
	// payload.param_len = strlen(param);
	payload.param = NULL;
	payload.param_len = 0;

	__TLSDBG("url:%.*s, urlsize:%d, datasize:%d.\n",url.url_len, url.url,  url.url_len, payload.data_len);

	app_vsoa_publish_data(s_app_publish, &url, &payload);
}
