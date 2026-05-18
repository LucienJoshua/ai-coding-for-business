#ifndef __TCP_PROTOCOL_H__
#define __TCP_PROTOCOL_H__

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#include "tls_com.h"
#include "tls_type.h"


	typedef struct __dialog_tcp_head
	{
		U16						flag;
		U16						padding;
	
		U64						usrdata;
		U32						type;
		U32						datasize;

		U8						data[0];
	}__attribute__ ((__packed__)) dialog_tcp_head_t;


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __TCP_PROTOCOL_H__
