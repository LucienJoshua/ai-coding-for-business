#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "tls_config.h"
#include "tls_server.h"
#include "vsoa_server.h"
#include "app_transmit.h"
#include "tls_test.h"
#include "file_block.h"
#include "tls_vsoa.h"
#include "tls_protocol.h"
#include "app_linkinfo.h"
#include "app_vsoa_subscribe.h"

static app_transmit_t		s_app_tranmit;

int main(int argc, char** argv)
{
	TLS_RESULT			result = TLS_RESULT_E_FAIL;

	printf("sizeof tls_path_msg_t:%d\n", sizeof(tls_path_msg_t));
	printf("sizeof app_pathinfo_t:%d\n", sizeof(app_pathinfo_t));
	printf("sizeof tls_path_info_t:%d\n", sizeof(tls_path_info_t));

	memset(&s_app_tranmit, 0, sizeof(s_app_tranmit));
	result = api_transmit_initialize(&s_app_tranmit);
	if (TlsResultFail(result))
	{
		__TLSDBG("api transmit initialize fail.");
		goto __FINISH;
	}


	// on_tls_test();

	while (api_transmit_running_status(&s_app_tranmit))
	{
		api_transmit_poll(&s_app_tranmit);
	}

__FINISH:
	api_transmit_uninitialize(&s_app_tranmit);
	__TLSDBG("All finish.");
	exit(0);

	return 0;
}


app_transmit_t* api_transmit_instance(void)
{
	return &s_app_tranmit;
}
