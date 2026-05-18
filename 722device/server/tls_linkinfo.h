#ifndef __TLS_LINKINFO_H__
#define __TLS_LINKINFO_H__

#ifdef __cplusplus
extern "C"{
#endif 


#include "tls_com.h"
#include "tls_type.h"
#include "app_linkinfo.h"

	typedef struct __link_device_info 
	{
		U32				dst_addr;
		U32				dst_entry;
		S32				device_status;		/* 设备状态，枚举 */
		S32				link_status;		/* 工作状态，枚举 */
		S32				link_size;			/* 传输帧长，单位字节 */
		S32				link_type;			/* 通信方式，枚举类型 */
		S32				link_rate;			/* 通信速率，单位bps */
		S32				result;				/* 错误码 */
	}link_device_info_t;


	extern TLS_RESULT	tls_link_device_info_get(void* app_transmit, link_device_info_t* devinfo);

	extern TLS_RESULT	tls_link_device_info_send(void* app_transmit);

	extern TLS_RESULT	tls_linkinfo(void* app, const app_linkinfo_request_t* req, app_linkinfo_result_t* result);



#ifdef __cplusplus
}
#endif 


#endif // __TLS_LINKINFO_H__
