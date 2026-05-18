#ifndef __TLS_CONFIG_H__
#define __TLS_CONFIG_H__

#include "tls_com.h"
#include "tls_type.h"

#define TLS_CFG_DEFAULT_LOCAL_IP			"192.168.0.83"
#define TLS_CFG_DEFAULT_LOCAL_PORT			"9000"
#define TLS_CFG_DEFAULT_REMOTE_IP			"192.168.0.90"
#define TLS_CFG_DEFAULT_REMOTE_PORT			"9000"
#define TLS_CFG_DEFAULT_LOCAL_ID			"1"
#define TLS_CFG_DEFAULT_LOCAL_SUB_ID		"1"

typedef struct __tls_queue_config
{
	U32					priority_;
	U32					buffsize_;
	U32					buffcount_;
}tls_queue_config_t;


typedef struct __tls_config
{
	char				tls_local_ip[32];
	U16					tls_local_port;
	char				tls_remote_ip[32];
	U16					tls_remote_port;

	U32					tls_local_id;
	U32					tls_local_sub_id;


	char				vsoa_sub_servername[32];
	char				vsoa_sub_serverpass[32];
	char				vsoa_sub_url[128];

	char				vsoa_pub_servername[32];
	char				vsoa_pub_serverpass[32];
	char				vsoa_pub_serverip[32];
	U16					vsoa_pub_serverport;
	char				vsoa_pub_url[128];

	tls_queue_config_t	queue_config_[8];
}tls_config_t;

typedef struct __file_config
{
	char				file_dir[128];
	U32					file_block_count;
	U32					file_block_size;

	U32					interval_ms_;
	U32					publish_dst_addr;	
	U32					publish_dst_entry;
	char				publish_url[128];

	char				app_sub_servername[32];
	char				app_sub_serverpass[32];
	char				app_sub_url[128];

}file_config_t;


extern TLS_RESULT		tls_config_load(const char* tls_cfg_filename, tls_config_t* tls_cfg);

extern TLS_RESULT		tls_config_load_all(const char* filename, tls_config_t* cfg);

extern TLS_RESULT		file_config_load_all(const char* filename, file_config_t* cfg);

#endif // __TLS_CONFIG_H__
