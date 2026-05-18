#include <stdlib.h>
#include "app_transmit.h"
#include "tls_server.h"
#include "tls_vsoa_publish.h"
#include "tls_log.h"
#include "tls_request.h"
#include "tls_protocol.h"
#include "tls_linkinfo.h"

TLS_RESULT	tls_link_device_info_get(void* app_transmit, link_device_info_t* devinfo)
{
	tls_server_t*			tls_server;	
	tls_vsoa_publish_t*		tls_publish;

	if (NULL == app_transmit ||  NULL == devinfo)
		return TLS_RESULT_E_INVALID_PARAM;


	// todo...
	memset(devinfo, 0, sizeof(*devinfo));


	devinfo->device_status = 1;
	devinfo->link_status = 2;
	devinfo->link_size = 3;
	devinfo->link_type = 4;
	devinfo->link_rate = 5;
	return TLS_RESULT_S_OK;
}
TLS_RESULT	tls_link_device_info_send(void* app)
{
	int							ret;
	app_transmit_t*				app_transmit;
	tls_server_t*				tls_server;	
	tls_param_t*				server_param;
	tls_vsoa_publish_t*			tls_publish;

	tls_net_status_req_msg_t	status_msg_buf;
	tls_message_head_t*			status_msg_head;


	app_transmit = (app_transmit_t*)app;
	if (NULL == app_transmit)
		return TLS_RESULT_E_INVALID_PARAM;

	tls_server = (tls_server_t*)app_transmit->tls_server_;
	tls_publish = tls_server->tls_vsoa_publish_;
	server_param = &tls_server->svr_param_;
	
	memset(&status_msg_buf, 0, sizeof status_msg_buf);
	status_msg_head = &status_msg_buf.msg_header;

	status_msg_head->version = TLS_MSG_MAIN_VERSION;
	status_msg_head->message_type = MSG_TYPE_SEND_MESSAGE;
	status_msg_head->message_seq = htons(tls_server->sequence_++);

	strcpy(status_msg_buf.sessionid, "session");
	status_msg_buf.src_addr = htonl(server_param->tls_local_id_);
	status_msg_buf.src_entry = htonl(server_param->tls_local_sub_id_);
	status_msg_buf.dst_addr = htonl(0);
	status_msg_buf.dst_entry = htonl(0);
	status_msg_buf.path_policy = 0;

	ret = vsoa_publish_data(tls_publish, &status_msg_buf, sizeof(status_msg_buf));

	return TLS_RESULT_S_OK;
}

TLS_RESULT tls_linkinfo(void* app, const app_linkinfo_request_t* req, app_linkinfo_result_t* result)
{
	app_transmit_t*			app_transmit;
	tls_server_t*			tls_server;
	tls_request_t*			tls_request;

	if (NULL == app || NULL == req || NULL == result)
	{
		LOG_ERROR("ERROR-Invalid parameters.");
		return TLS_RESULT_E_INVALID_PARAM;
	}

	app_transmit = (app_transmit_t*)app;
	tls_server = (tls_server_t*)app_transmit->tls_server_;
	tls_request = (tls_request_t*)tls_server->tls_request_;

	return tls_request_link_info(tls_request, req, result);
}
