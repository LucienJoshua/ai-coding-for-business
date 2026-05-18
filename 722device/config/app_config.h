#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

#include "tls_com.h"
#include "tls_type.h"

#define UDP_CFG_DEFAULT_LOCAL_IP			"192.168.0.83"
#define UDP_CFG_DEFAULT_LOCAL_PORT			"9000"
#define UDP_CFG_DEFAULT_REMOTE_IP			"192.168.0.90"
#define UDP_CFG_DEFAULT_REMOTE_PORT			"9000"
#define UDP_CFG_DEFAULT_TLS_REMOTE_ID		"2"
#define UDP_CFG_DEFAULT_TLS_REMOTE_SUBID	"2"



typedef struct __app_udp_config
{
	char						local_ip[32];
	U16							local_port;
	char						remote_ip[32];
	U16							remote_port;
	
	U32							tls_remote_id;
	U32							tls_remote_sub_id;
}app_udp_config_t;

typedef struct __app_rpc_server_config
{
	char						servername[32];
	char						serverpass[32];
	char						serverip[32];
	U16							serverport;
	char						listenerurl[256];
}app_rpc_server_config_t;

typedef struct __app_publish_conifg
{
	char						servername[32];
	char						serverpass[32];
	char						serverip[32];
	U16							serverport;
	char						publishurl[256];
}app_publish_config_t;

typedef struct __app_subscribe_config
{
	char						servername[32];
	char						serverpass[32];
	char						subscribeurl[256];
}app_subscribe_config_t;


typedef struct _app_config
{
	app_udp_config_t			udp_config;
	app_rpc_server_config_t		rpc_server_config;
	app_publish_config_t		publish_config;
	app_subscribe_config_t		subscribe_config;
}app_config_t;


extern TLS_RESULT		api_udp_endpoint_config_load(const char* filename, app_udp_config_t* cfg);

extern TLS_RESULT		api_app_config_load_all(const char* filename, app_config_t* cfg);

#endif // __APP_CONFIG_H__
