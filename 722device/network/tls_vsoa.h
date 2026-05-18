#ifndef __TLS_VSOA_H__
#define  __TLS_VSOA_H__

#include "tls_type.h"


typedef struct __tls_vsoa_hdr
{
	char*				vsoa_url;
	U64					vsoa_url_size;
	char*				vsoa_param;
	U64					vsoa_param_size;
	U8*					vsoa_data;
	U64					vsoa_data_size;
	U8					tls_vsoa_data[0];
}__attribute__ ((__packed__)) tls_vsoa_hdr_t;



#endif // __TLS_VSOA_H__
