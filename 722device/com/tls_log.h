#ifndef __TLS_LOG_H__
#define __TLS_LOG_H__


#ifdef __cplusplus
extern "C"{
#endif

#include "tls_com.h"
#include "zlog.h"


#define TLSLOG_FATAL(log, format, args...) \
	zlog_fatal(((tls_log_t*)(log))->zlog_, format, ##args)

#define TLSLOG_ERROR(log, format, args...) \
	zlog_error(((tls_log_t*)(log))->zlog_, format, ##args)

#define TLSLOG_WARN(log, format, args...) \
	zlog_warn(((tls_log_t*)(log))->zlog_, format, ##args)

#define TLSLOG_NOTICE(log, format, args...) \
	zlog_notice(((tls_log_t*)(log))->zlog_, format, ##args)

#define TLSLOG_INFO(log, format, args...) \
	zlog_info(((tls_log_t*)(log))->zlog_, format, ##args)

#define TLSLOG_DEBUG(log, format, args...) \
	zlog_debug(((tls_log_t*)(log))->zlog_, format, ##args)


/*
#define LOG_FATAL(zlog, format, args...) \
	zlog_fatal(zlog, format, ##args)

#define LOG_ERROR(zlog, format, args...) \
	zlog_error(zlog_, format, ##args)

#define LOG_WARN(zlog, format, args...) \
	zlog_warn(zlog_, format, ##args)

#define LOG_NOTICE(zlog, format, args...) \
	zlog_notice(zlog_, format, ##args)

#define LOG_INFO(zlog, format, args...) \
	zlog_info(zlog_, format, ##args)

#define LOG_DEBUG(zlog, format, args...) \
	zlog_debug(zlog_, format, ##args)
*/


#define LOG_FATAL(format, args...) \
	zlog_fatal(((tls_log_t*)(g_tls_log))->zlog_, format, ##args)

#define LOG_ERROR(format, args...) \
	zlog_error(((tls_log_t*)(g_tls_log))->zlog_, format, ##args)

#define LOG_WARN(format, args...) \
	zlog_warn(((tls_log_t*)(g_tls_log))->zlog_, format, ##args)

#define LOG_NOTICE(format, args...) \
	zlog_notice(((tls_log_t*)(g_tls_log))->zlog_, format, ##args)

#define LOG_INFO(format, args...) \
	zlog_info(((tls_log_t*)(g_tls_log))->zlog_, format, ##args)

#define LOG_DEBUG(format, args...) \
	zlog_info(((tls_log_t*)(g_tls_log))->zlog_, format, ##args)


	typedef struct __tls_log
	{
		int					init_;
		void*				zlog_;
	}tls_log_t;

	extern tls_log_t*			tls_log_allocate(void);
	extern void					tls_log_free(tls_log_t* log);

	extern TLS_RESULT			tls_log_initialize(tls_log_t* log);
	extern void					tls_log_uninitialize(tls_log_t* log);



	extern tls_log_t*			g_tls_log;


#ifdef __cplusplus
}
#endif

#endif // __TLS_LOG_H__
