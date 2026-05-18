#ifndef __TLS_POOL_H__
#define __TLS_POOL_H__

#include "tls_list.h"
#include "tls_lock.h"

struct tls_buff_queue
{
	tls_list_node_t				list_buff_;
	tls_lock_t					queue_lock_;

	S32							queue_init_;

	U32							buff_count_;	
	U32							buff_size_;
	U32							mem_size_;

	U8							priority_;
};

struct tls_buff
{
	tls_list_node_t				list_node_;

	PU8							buf_;
	U32							buf_size_;
	U32							size_;
	U32							type_;

	U8							priority_;
	void*						usrdata_;
	U8							transmode_;

#if 0
	PU8							tls_hdr_;
	PU8							tls_msg_;
	PU8							tls_payload_;
	U8							payload_size_;
#endif
};

typedef struct tls_buff_queue	tls_buff_queue_t;
typedef struct tls_buff			tls_buff_t;
// typedef struct tls_message		tls_message_t;


extern TLS_RESULT		tls_buff_queue_initialize(tls_buff_queue_t* queue);

extern void				tls_buff_queue_uninitialize(tls_buff_queue_t* queue);

extern TLS_RESULT		tls_buff_queue_allocate(tls_buff_queue_t* queue, U32 size, U32 count);

extern TLS_RESULT		tls_buff_queue_allocate_t(tls_buff_queue_t* queue ,U32 size, U32 count ,U32 type);

extern TLS_RESULT		tls_buff_queue_free(tls_buff_queue_t* queue);

extern tls_buff_t*		tls_buff_queue_acquire(tls_buff_queue_t* queue);

extern void				tls_buff_queue_release(tls_buff_queue_t* queue, tls_buff_t* buf);

extern tls_buff_t*		tls_buff_create(U32 size, U32 type);

extern void				tls_buff_delete(tls_buff_t* buff);

#endif // __TLS_POOL_H__
