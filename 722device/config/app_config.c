#include "mxml.h"
#include "app_config.h"

#define UDP_CFG_LOCAL_IP				"udp_local_ip"
#define UDP_CFG_LOCAL_PORT				"udp_local_port"
#define UDP_CFG_REMOTE_IP				"udp_remote_ip"
#define UDP_CFG_REMOTE_PORT				"udp_remote_port"
#define UDP_TLS_CFG_REMOTE_ID			"tls_remote_id"
#define UDP_TLS_CFG_REMOTE_SUBID		"tls_remote_sub_id"


#define APP_UDP							"app_udp"
#define APP_RPC_SERVER					"app_rpc_server"
#define APP_PUBLISH						"app_publish"
#define APP_SUBSCRIBR					"app_subscribe"

enum APP_UDP_ATTRIBUTE
{
	LOCALIP = 0,
	LOCALPORT,
	REMOTEIP,
	REMOTEPORT,
	REMOTEID,
	REMOTESUBID,
};

enum APP_RPCSERVER_ATTRIBUTE
{
	RPC_SERVERNAME = 0,
	RPC_SERVERPASS,
	RPC_SERVERIP, 
	RPC_SERVERPORT, 
	RPC_LISTENERURL,
};

enum APP_PUBLISH_ATTRIBUTE
{
	PUBLISH_SERVERNAME = 0,
	PUBLISH_SERVERPASS,
	PUBLISH_SERVERIP,
	PUBLISH_SERVERPORT,
	PUBLISH_URL,
};

enum APP_SUBSCRIBE_ATTRIBUTE
{
	SUBSCRIBE_SERVERNAME = 0,
	SUBSCRIBE_SERVERPASS,
	SUBSCRIBE_URL,
};




static const char*	udp_config_xml_node_text(mxml_node_t* root, const char* node_name);

static void			app_load_udp_config(mxml_node_t* udp_node, app_udp_config_t* udp_cfg);
static void			app_load_rpc_server_config(mxml_node_t* rpc_server_node, app_rpc_server_config_t* rpc_server_cfg);
static void			app_load_publish_config(mxml_node_t* publish_node, app_publish_config_t* publish_cfg);
static void			app_load_subscribe_config(mxml_node_t* subscribe_node, app_subscribe_config_t* subscribe_cfg);



TLS_RESULT api_udp_endpoint_config_load(const char* filename, app_udp_config_t* cfg)
{
	TLS_RESULT		result = TLS_RESULT_E_FAIL;
	mxml_node_t*	root = NULL;
	mxml_node_t*	node = NULL;
	const char*		node_text;

	do
	{
		if (NULL == filename || 0 == strlen(filename) || NULL == cfg)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}

		memset(cfg, 0, sizeof(*cfg));
		root = mxmlLoadFilename(NULL, NULL, filename);
		if (NULL == root)
		{
			__TLSDBG("mxml load file fail.\n");
			result = TLS_RESULT_E_FAIL;
			break;
		}
		
		node_text = udp_config_xml_node_text(root, UDP_CFG_LOCAL_IP);
		if (node_text && strlen(node_text))
		{
			strcpy(cfg->local_ip, node_text);
			__TLSDBG("read config udp local ip:%s.\n", cfg->local_ip);
		}
		else
		{
			strcpy(cfg->local_ip, UDP_CFG_DEFAULT_LOCAL_IP);
		}

		node_text = udp_config_xml_node_text(root, UDP_CFG_LOCAL_PORT);
		if (node_text && strlen(node_text))
		{
			cfg->local_port = atoi(node_text);
			__TLSDBG("read config udp local port:%d.\n", cfg->local_port);
		}
		else
		{
			cfg->local_port = atoi(UDP_CFG_DEFAULT_LOCAL_PORT);
		}

		node_text = udp_config_xml_node_text(root, UDP_CFG_REMOTE_IP);
		if (node_text && strlen(node_text))
		{
			strcpy(cfg->remote_ip, node_text);
			__TLSDBG("read config udp remote ip:%s.\n", cfg->remote_ip);
		}
		else
		{
			strcpy(cfg->remote_ip, UDP_CFG_DEFAULT_REMOTE_IP);
		}

		node_text = udp_config_xml_node_text(root, UDP_CFG_REMOTE_PORT);
		if (node_text && strlen(node_text))
		{
			cfg->remote_port = atoi(node_text);
			__TLSDBG("read config udp remote port:%d.\n", cfg->remote_port);
		}
		else
		{
			cfg->remote_port = atoi(UDP_CFG_DEFAULT_LOCAL_PORT);
		}

		node_text = udp_config_xml_node_text(root, UDP_TLS_CFG_REMOTE_ID);
		if (node_text && strlen(node_text))
		{
			cfg->tls_remote_id = atoi(node_text);
			__TLSDBG("read config udp remote tls id:%d.\n", cfg->tls_remote_id);
		}
		else
		{
			cfg->tls_remote_id = atoi(UDP_CFG_DEFAULT_TLS_REMOTE_ID);
		}
		node_text = udp_config_xml_node_text(root, UDP_TLS_CFG_REMOTE_SUBID);
		if (node_text && strlen(node_text))
		{
			cfg->tls_remote_sub_id = atoi(node_text);
			__TLSDBG("read config udp remote tls sub id:%d.\n", cfg->tls_remote_sub_id);
		}
		else
		{
			cfg->tls_remote_sub_id = atoi(UDP_CFG_DEFAULT_TLS_REMOTE_SUBID);
		}
		mxmlDelete(root);
		result = TLS_RESULT_S_OK;
	}while(0);

	return result;
}


const char* udp_config_xml_node_text(mxml_node_t* root, const char* node_name)
{
	const char*					text;
	mxml_node_t*				node;

	if (NULL == root || NULL == node_name)
		return NULL;

	node = mxmlFindElement(root, root, node_name, NULL, NULL, MXML_DESCEND_ALL);
	if (NULL == node)
		return NULL;
	text = mxmlGetText(node, NULL);
	if (NULL == text || 0 == strlen(text))
		return NULL;
	return text;
}


TLS_RESULT api_app_config_load_all(const char* filename, app_config_t* cfg)
{
	mxml_node_t*				root;
	mxml_node_t*				node;
	app_udp_config_t*			udp_cfg;
	app_rpc_server_config_t*	rpc_server_cfg;
	app_publish_config_t*		publish_cfg;
	app_subscribe_config_t*		subscribe_cfg;

	if (NULL == filename || NULL == cfg || 0 == strlen(filename))
		return TLS_RESULT_E_INVALID_PARAM;

	udp_cfg = &cfg->udp_config;
	rpc_server_cfg = &cfg->rpc_server_config;
	publish_cfg = &cfg->publish_config;
	subscribe_cfg = &cfg->subscribe_config;
	
	root = mxmlLoadFilename(NULL, NULL, filename);
	if (NULL == root)
	{
		__TLSDBG("Load xml fail, filename:%s.\n", filename);
		return TLS_RESULT_E_FAIL;
	}

	node = mxmlFindElement(root, root, APP_UDP, NULL, NULL, MXML_DESCEND_ALL);
	app_load_udp_config(node, udp_cfg);

	node = mxmlFindElement(root, root, APP_RPC_SERVER, NULL, NULL, MXML_DESCEND_ALL);
	app_load_rpc_server_config(node, rpc_server_cfg);

	node = mxmlFindElement(root, root, APP_PUBLISH, NULL, NULL, MXML_DESCEND_ALL);
	app_load_publish_config(node, publish_cfg);

	node = mxmlFindElement(root, root, APP_SUBSCRIBR, NULL, NULL, MXML_DESCEND_ALL);
	app_load_subscribe_config(node, subscribe_cfg);

	mxmlDelete(root);

	return TLS_RESULT_S_OK;
}
void app_load_udp_config(mxml_node_t* udp_node, app_udp_config_t* udp_cfg)
{
	int				index = 0;
	int				count = 0;
	const char*		attr;

	if (NULL == udp_node || NULL == udp_cfg)
		return;

	count = mxmlElementGetAttrCount(udp_node);
	for (index = 0; index < count; index++)
	{
		attr = mxmlElementGetAttrByIndex(udp_node, index, NULL);
		if (NULL == attr || 0 == strlen(attr))
			continue;

		switch (index)
		{
		case LOCALIP:
			if (attr && strlen(attr) > 0)
				strcpy(udp_cfg->local_ip, attr);
			break;
		case LOCALPORT:
			if (attr && strlen(attr) > 0)
				udp_cfg->local_port = atoi(attr);
			break;
		case REMOTEIP:
			if (attr && strlen(attr) > 0)
				strcpy(udp_cfg->remote_ip, attr);
			break;
		case REMOTEPORT:
			if (attr && strlen(attr) > 0)
				udp_cfg->remote_port = atoi(attr);
			break;
		case REMOTEID:
			if (attr && strlen(attr) > 0)
				udp_cfg->tls_remote_id = atoi(attr);
			break;
		case REMOTESUBID:
			if (attr && strlen(attr) > 0)
				udp_cfg->tls_remote_sub_id = atoi(attr);
			break;
		default:
			break;
		}
	}
}
void app_load_rpc_server_config(mxml_node_t* rpc_server_node, app_rpc_server_config_t* rpc_server_cfg)
{
	int				index = 0;
	int				count = 0;
	const char*		attr;

	if (NULL == rpc_server_node || NULL == rpc_server_cfg)
		return;

	count = mxmlElementGetAttrCount(rpc_server_node);
	for (index = 0; index < count; index++)
	{
		attr = mxmlElementGetAttrByIndex(rpc_server_node, index, NULL);
		if (NULL == attr || 0 == strlen(attr))
			continue;

		switch (index)
		{
		case RPC_SERVERNAME:
			if (attr && strlen(attr) > 0)
				strcpy(rpc_server_cfg->servername, attr);
			break;
		case RPC_SERVERPASS:
			if (attr && strlen(attr) > 0)
				strcpy(rpc_server_cfg->serverpass, attr);
			break;
		case RPC_SERVERIP:
			if (attr && strlen(attr) > 0)
				strcpy(rpc_server_cfg->serverip, attr);
			break;
		case RPC_SERVERPORT:
			if (attr && strlen(attr) > 0)
				rpc_server_cfg->serverport = atoi(attr);
			break;
		case RPC_LISTENERURL:
			if (attr && strlen(attr) > 0)
				strcpy(rpc_server_cfg->listenerurl, attr);
			break;
		default:
			break;
		}
	}
}
void app_load_publish_config(mxml_node_t* publish_node, app_publish_config_t* publish_cfg)
{
	int				index = 0;
	int				count = 0;
	const char*		attr;

	if (NULL == publish_node || NULL == publish_cfg)
		return;

	count = mxmlElementGetAttrCount(publish_node);
	for (index = 0; index < count; index++)
	{
		attr = mxmlElementGetAttrByIndex(publish_node, index, NULL);
		if (NULL == attr || 0 == strlen(attr))
			continue;

		switch (index)
		{
		case PUBLISH_SERVERNAME:
			if (attr && strlen(attr) > 0)
				strcpy(publish_cfg->servername, attr);
			break;
		case PUBLISH_SERVERPASS:
			if (attr && strlen(attr) > 0)
				strcpy(publish_cfg->serverpass, attr);
			break;
		case PUBLISH_SERVERIP:
			if (attr && strlen(attr) > 0)
				strcpy(publish_cfg->serverip, attr);
			break;
		case PUBLISH_SERVERPORT:
			if (attr && strlen(attr) > 0)
				publish_cfg->serverport = atoi(attr);
			break;
		case PUBLISH_URL:
			if (attr && strlen(attr) > 0)
				strcpy(publish_cfg->publishurl, attr);
			break;
		default:
			break;
		}
	}
}

void app_load_subscribe_config(mxml_node_t* subscribe_node, app_subscribe_config_t* subscribe_cfg)
{
	int				index = 0;
	int				count = 0;
	const char*		attr;

	if (NULL == subscribe_node || NULL == subscribe_cfg)
		return;

	count = mxmlElementGetAttrCount(subscribe_node);
	for (index = 0; index < count; index++)
	{
		attr = mxmlElementGetAttrByIndex(subscribe_node, index, NULL);
		if (NULL == attr || 0 == strlen(attr))
			continue;

		switch (index)
		{
		case SUBSCRIBE_SERVERNAME:
			if (attr && strlen(attr) > 0)
				strcpy(subscribe_cfg->servername, attr);
			break;
		case SUBSCRIBE_SERVERPASS:
			if (attr && strlen(attr) > 0)
				strcpy(subscribe_cfg->serverpass, attr);
			break;
		case SUBSCRIBE_URL:
			if (attr && strlen(attr) > 0)
				strcpy(subscribe_cfg->subscribeurl, attr);
			break;
		default:
			break;
		}
	}
}
