#include "mxml.h"
#include "tls_config.h" 
#define TLS_CFG_LOCAL_IP				"tls_local_ip"
#define TLS_CFG_LOCAL_PORT				"tls_local_port"
#define TLS_CFG_REMOTE_IP				"tls_remote_ip"
#define TLS_CFG_REMOTE_PORT				"tls_remote_port"
#define TLS_CFG_LOCAL_ID				"tls_local_id"
#define TLS_CFG_LOCAL_SUB_ID			"tls_local_sub_id"


#define TLS_UDP							"tls_udp"
#define TLS_SUBSCRIBE					"tls_subscribe"
#define TLS_PUBLISH						"tls_publish"
#define TLS_TRANSMIT_QUEUE				"tls_transmit_queue"


#define FILE_BASE						"file_base"
#define FILE_PUBLISH					"file_publish"
#define FILE_SUBSCRIBE					"file_subscribe"


enum TLS_UDP_ATTR
{
	LOCAL_IP = 0,
	LOCAL_PORT, 
	REMOTE_IP,
	REMOTE_PORT,
	LOCAL_ID,
	LOCAL_SUBID,
};

enum TLS_SUBSCRIBE_ATTR
{
	SUBSCRIBE_SERVERNAME = 0,
	SUBSCRIBE_SERVERPASS,
	SUBSCRIBE_URL,
};

enum TLS_PUBLISH_ATTR
{
	PUBLISH_SERVERNAME = 0,
	PUBLISH_SERVERPASS,
	PUBLISH_SERVERIP,
	PUBLISH_SERVERPORT,
	PUBLISH_URL,
};

enum TLS_QUEUE_ATTR
{
	QUEUE_PRIORITY = 0,
	QUEUE_BUFFSIZE,
	QUEUE_BUFFCOUNT,
};


enum FILE_BASE_ATTR
{
	FILE_DIR = 0, 
	FILE_BLOCK_COUNT,
	FILE_BLOCK_SIZE,
	FILE_SEND_INTERVAL,
};

enum FILE_PUBLISH_ATTR
{
	FILE_PUBLISH_INTERVAL = 0,
	FILE_PUBLISH_DSTADDR,
	FILE_PUBLISH_DSTENTRY,
	FILE_PUBLISH_URL,
};

enum FILE_SUBSCRIBE_ATTR
{
	FILE_SUBSCRIBE_SERVERNAME = 0,
	FILE_SUBSCRIBE_SERVERPASS,
	FILE_SUBSCRIBE_URL,
};


static const char*	tls_config_xml_node_text(mxml_node_t* node_root, const char* node_name);
static void			tls_config_set_default(tls_config_t* cfg);
static void			tls_config_show(const tls_config_t* cfg);	

static void			tls_load_udp(mxml_node_t* tls_udp, tls_config_t* cfg);
static void			tls_load_subscribe(mxml_node_t* tls_sub, tls_config_t* cfg);
static void			tls_load_publish(mxml_node_t* tls_pub, tls_config_t* cfg);

static void			tls_load_transmit_queue(mxml_node_t* transmit_node, tls_config_t* cfg);
static void			tls_load_transmit_queue_config(mxml_node_t* queue_node, tls_queue_config_t* queue_cfg);


static void			file_load_base(mxml_node_t* file_base_node, file_config_t* cfg);
static void			file_load_publish(mxml_node_t* file_publish, file_config_t* cfg);
static void			file_load_subscribe(mxml_node_t* file_subscribe_node, file_config_t* cfg);

TLS_RESULT tls_config_load(const char* tls_cfg_filename, tls_config_t* tls_cfg)
{
	TLS_RESULT		result = TLS_RESULT_E_FAIL;
	mxml_node_t*	root = NULL;
	mxml_node_t*	node = NULL;
	const char*		node_text;

	do
	{
		if (NULL == tls_cfg_filename || 0 == strlen(tls_cfg_filename) || NULL == tls_cfg)
		{
			result = TLS_RESULT_E_INVALID_PARAM;
			break;
		}

		memset(tls_cfg, 0, sizeof(*tls_cfg));
		root = mxmlLoadFilename(NULL, NULL, tls_cfg_filename);
		if (NULL == root)
		{
			__TLSDBG("mxml load file fail.\n");
			result = TLS_RESULT_E_FAIL;
			break;
		}
		
		node_text = tls_config_xml_node_text(root, TLS_CFG_LOCAL_IP);
		if (node_text && strlen(node_text))
		{
			strcpy(tls_cfg->tls_local_ip, node_text);
			__TLSDBG("read config tls local ip:%s.\n", tls_cfg->tls_local_ip);
		}
		else
		{
			strcpy(tls_cfg->tls_local_ip, TLS_CFG_DEFAULT_LOCAL_IP);
		}

		node_text = tls_config_xml_node_text(root, TLS_CFG_LOCAL_PORT);
		if (node_text && strlen(node_text))
		{
			tls_cfg->tls_local_port = atoi(node_text);
			__TLSDBG("read config tls local port:%d.\n", tls_cfg->tls_local_port);
		}
		else
		{
			tls_cfg->tls_local_port = atoi(TLS_CFG_DEFAULT_LOCAL_PORT);
		}

		node_text = tls_config_xml_node_text(root, TLS_CFG_REMOTE_IP);
		if (node_text && strlen(node_text))
		{
			strcpy(tls_cfg->tls_remote_ip, node_text);
			__TLSDBG("read config tls remote ip:%s.\n", tls_cfg->tls_remote_ip);
		}
		else
		{
			strcpy(tls_cfg->tls_remote_ip, TLS_CFG_DEFAULT_REMOTE_IP);
		}

		node_text = tls_config_xml_node_text(root, TLS_CFG_REMOTE_PORT);
		if (node_text && strlen(node_text))
		{
			tls_cfg->tls_remote_port = atoi(node_text);
			__TLSDBG("read config tls remote port:%d.\n", tls_cfg->tls_remote_port);
		}
		else
		{
			tls_cfg->tls_remote_port = atoi(TLS_CFG_DEFAULT_REMOTE_PORT);
		}

		node_text = tls_config_xml_node_text(root, TLS_CFG_LOCAL_ID);
		if (node_text && strlen(node_text))
		{
			tls_cfg->tls_local_id = atoi(node_text);
			__TLSDBG("read config tls local id:%d.\n", tls_cfg->tls_local_id);
		}
		else
		{
			tls_cfg->tls_local_id = atoi(TLS_CFG_DEFAULT_LOCAL_ID);
		}
		node_text = tls_config_xml_node_text(root, TLS_CFG_LOCAL_SUB_ID);
		if (node_text && strlen(node_text))
		{
			tls_cfg->tls_local_sub_id = atoi(node_text);
			__TLSDBG("read config tls local sub id:%d.\n", tls_cfg->tls_local_sub_id);
		}
		else
		{
			tls_cfg->tls_local_sub_id = atoi(TLS_CFG_DEFAULT_LOCAL_SUB_ID);
		}
		mxmlDelete(root);
		result = TLS_RESULT_S_OK;
	}while(0);

	if (TlsResultFail(result))
		tls_config_set_default(tls_cfg);
	tls_config_show(tls_cfg);

	return result;
}

const char* tls_config_xml_node_text(mxml_node_t* root, const char* node_name)
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
void tls_config_set_default(tls_config_t* cfg)
{
	if (NULL == cfg)
		return;
	memset(cfg, 0, sizeof(tls_config_t));
	strcpy(cfg->tls_local_ip, TLS_CFG_DEFAULT_LOCAL_IP);
	cfg->tls_local_port = atoi(TLS_CFG_DEFAULT_LOCAL_PORT);
	strcpy(cfg->tls_remote_ip, TLS_CFG_DEFAULT_REMOTE_IP);
	cfg->tls_local_port = atoi(TLS_CFG_DEFAULT_LOCAL_PORT);
	cfg->tls_local_id = atoi(TLS_CFG_DEFAULT_LOCAL_ID);
	cfg->tls_local_sub_id = atoi(TLS_CFG_DEFAULT_LOCAL_SUB_ID);
}
void tls_config_show(const tls_config_t* cfg)
{
	if (NULL == cfg)
		return;

	printf("------------------------ TLS config infomation ------------------------\n");
	printf("TLS local ip address:%s.\n", cfg->tls_local_ip);
	printf("TLS local port:%d.\n", cfg->tls_local_port);
	printf("TLS remote ip address:%s.\n", cfg->tls_remote_ip);
	printf("TLS remote port:%d.\n", cfg->tls_remote_port);
	printf("TLS local id:%d.\n", cfg->tls_local_id);
	printf("TLS local sub id:%d.\n", cfg->tls_local_sub_id);
	printf("----------------------------------------------------------------------\n");
}


TLS_RESULT	tls_config_load_all(const char* filename, tls_config_t* cfg)
{
	mxml_node_t*		root;
	mxml_node_t*		node;
	
	root = mxmlLoadFilename(NULL, NULL, filename);
	if (NULL == root)
	{
		__TLSDBG("Load xml fail, filename:%s.\n", filename);
		return TLS_RESULT_E_FAIL;
	}

	node = mxmlFindElement(root, root, TLS_UDP, NULL, NULL, MXML_DESCEND_ALL);
	tls_load_udp(node, cfg);
	node = mxmlFindElement(root, root, TLS_SUBSCRIBE, NULL, NULL, MXML_DESCEND_ALL);
	tls_load_subscribe(node, cfg);
	node = mxmlFindElement(root, root, TLS_PUBLISH, NULL, NULL, MXML_DESCEND_ALL);
	tls_load_publish(node, cfg);

	node = mxmlFindElement(root, root, TLS_TRANSMIT_QUEUE, NULL, NULL, MXML_DESCEND_ALL);
	tls_load_transmit_queue(node, cfg);

	mxmlDelete(root);

	// tls_config_show(cfg);
	return TLS_RESULT_S_OK;
}
TLS_RESULT file_config_load_all(const char* filename, file_config_t* cfg)
{
	mxml_node_t*		root;
	mxml_node_t*		node;
	
	root = mxmlLoadFilename(NULL, NULL, filename);
	if (NULL == root)
	{
		__TLSDBG("Load xml fail, filename:%s.\n", filename);
		return TLS_RESULT_E_FAIL;
	}

	node = mxmlFindElement(root, root, FILE_BASE, NULL, NULL, MXML_DESCEND_ALL);
	file_load_base(node, cfg);
	node = mxmlFindElement(root, root, FILE_PUBLISH, NULL, NULL, MXML_DESCEND_ALL);
	file_load_publish(node, cfg);
	node = mxmlFindElement(root, root, FILE_SUBSCRIBE, NULL, NULL, MXML_DESCEND_ALL);
	file_load_subscribe(node, cfg);

	mxmlDelete(root);
	return TLS_RESULT_S_OK;
}
void tls_load_udp(mxml_node_t* udp_node, tls_config_t* cfg)
{
	int				index = 0;
	int				count = 0;
	const char*		attr;

	if (NULL == udp_node || NULL == cfg)
		return;
	count = mxmlElementGetAttrCount(udp_node);
	for (index = 0; index < count; index++)
	{
		attr = mxmlElementGetAttrByIndex(udp_node, index, NULL);
		if (NULL == attr || 0 == strlen(attr))
			continue;

		switch (index)
		{
		case LOCAL_IP:
			if (attr && strlen(attr) > 0)
				strcpy(cfg->tls_local_ip, attr);
			break;
		case LOCAL_PORT:
			if (attr && strlen(attr) > 0)
				cfg->tls_local_port = atoi(attr);
			break;
		case REMOTE_IP:
			if (attr && strlen(attr) > 0)
				strcpy(cfg->tls_remote_ip, attr);
			break;
		case REMOTE_PORT:
			if (attr && strlen(attr) > 0)
				cfg->tls_remote_port = atoi(attr);
			break;
		case LOCAL_ID:
			if (attr && strlen(attr) > 0)
				cfg->tls_local_id = atoi(attr);
			break;
		case LOCAL_SUBID:
			if (attr && strlen(attr) > 0)
				cfg->tls_local_sub_id = atoi(attr);
			break;
		default:
			break;
		}
	}
}
void tls_load_subscribe(mxml_node_t* tls_sub, tls_config_t* cfg)
{
	int				index = 0;
	int				count = 0;
	const char*		attr;

	if (NULL == tls_sub || NULL == cfg)
		return;

	count = mxmlElementGetAttrCount(tls_sub);
	for (index = 0; index < count; index++)
	{
		attr = mxmlElementGetAttrByIndex(tls_sub, index, NULL);
		if (NULL == attr || 0 == strlen(attr))
			continue;

		switch (index)
		{
		case SUBSCRIBE_SERVERNAME:
			if (attr && strlen(attr) > 0)
				strcpy(cfg->vsoa_sub_servername, attr);
			break;
		case SUBSCRIBE_SERVERPASS:
			if (attr && strlen(attr) > 0)
				strcpy(cfg->vsoa_sub_serverpass, attr);
			break;
		case SUBSCRIBE_URL:
			if (attr && strlen(attr) > 0)
				strcpy(cfg->vsoa_sub_url, attr);
			break;
		default:
			break;
		}
	}
}
void tls_load_publish(mxml_node_t* tls_pub, tls_config_t* cfg)
{
	int				index = 0;
	int				count = 0;
	const char*		attr;

	if (NULL == tls_pub || NULL == cfg)
		return;

	count = mxmlElementGetAttrCount(tls_pub);
	for (index = 0; index < count; index++)
	{
		attr = mxmlElementGetAttrByIndex(tls_pub, index, NULL);
		if (NULL == attr || 0 == strlen(attr))
			continue;

		switch (index)
		{
		case PUBLISH_SERVERNAME:
			if (attr && strlen(attr) > 0)
				strcpy(cfg->vsoa_pub_servername, attr);
			break;
		case PUBLISH_SERVERPASS:
			if (attr && strlen(attr) > 0)
				strcpy(cfg->vsoa_pub_serverpass, attr);
			break;
		case PUBLISH_SERVERIP:
			if (attr && strlen(attr) > 0)
				strcpy(cfg->vsoa_pub_serverip, attr);
			break;
		case PUBLISH_SERVERPORT:
			if (attr && strlen(attr) > 0)
				cfg->vsoa_pub_serverport = atoi(attr);
			break;
		case PUBLISH_URL:
			if (attr && strlen(attr) > 0)
				strcpy(cfg->vsoa_pub_url, attr);
			break;
		default:
			break;
		}
	}
}
void tls_load_transmit_queue(mxml_node_t* transmit_node, tls_config_t* cfg)
{
	mxml_node_t*			queue_node = NULL;
	int						index;
	int						count;
	const char*				text;
	tls_queue_config_t*		queue_config;

	if (NULL == transmit_node || NULL == cfg)
		return;

	index = 0;
	queue_node = mxmlFindElement(transmit_node, transmit_node, "queue", NULL, NULL, MXML_DESCEND_ALL);
	while (queue_node)
	{
		queue_config = &cfg->queue_config_[index];	
		tls_load_transmit_queue_config(queue_node, queue_config);
#if 0
		__TLSDBG("read queue priority:%ld, buffsize:%ld, buffcount:%ld\n",
				queue_config->priority_, queue_config->buffsize_, queue_config->buffcount_);
#endif
		index++;
		queue_node = mxmlFindElement(queue_node, transmit_node, "queue", NULL, NULL, MXML_DESCEND_ALL);
	}
}
void tls_load_transmit_queue_config(mxml_node_t* queue_node, tls_queue_config_t* queue_cfg)
{
	int						index;
	int						count;
	const char*				attr;

	if (NULL == queue_node || NULL == queue_cfg)
		return;

	count = mxmlElementGetAttrCount(queue_node);
	for (index = 0; index < count; index++)
	{
		attr = mxmlElementGetAttrByIndex(queue_node, index, NULL);
		if (NULL == attr || 0 == strlen(attr))
			continue;

		switch (index)
		{
		case QUEUE_PRIORITY:
			if (attr && strlen(attr) > 0)
				queue_cfg->priority_ = atoi(attr);
			break;
		case QUEUE_BUFFSIZE:
			if (attr && strlen(attr) > 0)
				queue_cfg->buffsize_ = atoi(attr);
			break;
		case QUEUE_BUFFCOUNT:
			if (attr && strlen(attr) > 0)
				queue_cfg->buffcount_ = atoi(attr);
			break;
		default:
			break;
		}
	}
}

void file_load_base(mxml_node_t* file_base_node, file_config_t* cfg)
{
	int				index = 0;
	int				count = 0;
	const char*		attr;

	if (NULL == file_base_node || NULL == cfg)
		return;

	count = mxmlElementGetAttrCount(file_base_node);
	for (index = 0; index < count; index++)
	{
		attr = mxmlElementGetAttrByIndex(file_base_node, index, NULL);
		if (NULL == attr || 0 == strlen(attr))
			continue;

		switch (index)
		{
		case FILE_DIR:
			if (attr && strlen(attr) > 0)
				strcpy(cfg->file_dir, attr);
			break;
		case FILE_BLOCK_COUNT:
			if (attr && strlen(attr) > 0)
				cfg->file_block_count = atoi(attr);
			break;
		case FILE_BLOCK_SIZE:
			if (attr && strlen(attr) > 0)
				cfg->file_block_size = atoi(attr);
			break;
#if 0
		case FILE_SEND_INTERVAL:
			if (attr && strlen(attr) > 0)
				cfg->interval_ms_ = atoi(attr);
			break;
#endif
		default:
			break;
		}
	}
}
void file_load_publish(mxml_node_t* file_publish_node, file_config_t* cfg)
{
	int				index = 0;
	int				count = 0;
	const char*		attr;

	if (NULL == file_publish_node || NULL == cfg)
		return;

	count = mxmlElementGetAttrCount(file_publish_node);
	for (index = 0; index < count; index++)
	{
		attr = mxmlElementGetAttrByIndex(file_publish_node, index, NULL);
		if (NULL == attr || 0 == strlen(attr))
			continue;

		switch (index)
		{
		case FILE_PUBLISH_INTERVAL:
			if (attr && strlen(attr) > 0)
				cfg->interval_ms_ = atoi(attr);
			break;
		case FILE_PUBLISH_DSTADDR:
			if (attr && strlen(attr) > 0)
				cfg->publish_dst_addr = atoi(attr);
			break;
		case FILE_PUBLISH_DSTENTRY:
			if (attr && strlen(attr) > 0)
				cfg->publish_dst_entry = atoi(attr);
			break;
		case FILE_PUBLISH_URL:
			if (attr && strlen(attr) > 0)
				strcpy(cfg->publish_url, attr);
			break;
		default:
			break;
		}
	}
}
void file_load_subscribe(mxml_node_t* file_subscribe_node, file_config_t* cfg)
{
	int				index = 0;
	int				count = 0;
	const char*		attr;

	if (NULL == file_subscribe_node || NULL == cfg)
		return;

	count = mxmlElementGetAttrCount(file_subscribe_node);
	for (index = 0; index < count; index++)
	{
		attr = mxmlElementGetAttrByIndex(file_subscribe_node, index, NULL);
		if (NULL == attr || 0 == strlen(attr))
			continue;

		switch (index)
		{
		case FILE_SUBSCRIBE_SERVERNAME:
			if (attr && strlen(attr) > 0)
				strcpy(cfg->app_sub_servername, attr);
			__TLSDBG("subscribe name:%s\n", cfg->app_sub_servername);		
			break;
		case FILE_SUBSCRIBE_SERVERPASS:
			if (attr && strlen(attr) > 0)
				strcpy(cfg->app_sub_serverpass, attr);
			__TLSDBG("subscribe pass:%s\n", cfg->app_sub_serverpass);		
			break;
		case FILE_SUBSCRIBE_URL:
			if (attr && strlen(attr) > 0)
				strcpy(cfg->app_sub_url, attr);
			__TLSDBG("subscribe url:%s\n", cfg->app_sub_url);		
			break;
		default:
			break;
		}
	}
}
