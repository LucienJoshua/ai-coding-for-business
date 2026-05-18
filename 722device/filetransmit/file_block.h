#ifndef __FILE_BLOCK_H__
#define __FILE_BLOCK_H__

#ifdef __cplusplus
extern "C"{
#endif

#include "tls_com.h"
#include "tls_type.h"
#include "tls_list.h"


#define FILE_NAME_SIZE			(128)
#define FILE_DATA_MTU			(4096)
#define FILE_BLOCK_FLAG			(0x55a0)


	typedef struct __file_block
	{
		tls_list_node_t			block_node_;
		PU8						block_data_;
		U32						block_size_;
		U32						size_;
	}file_block_t;
	
	typedef struct __file_block_hdr
	{
		U16						flag_;				/* 固定数 0x55, 0xa0， 标识文件升级业务数据*/
		U16						reserve_;			/* 保留，用于字节对齐 */
		U32						count_;				/* 当前文件块总个数 */	
		U32						sequence_;			/* 包序号，从0开始，范围为(0 —— count-1) */
		S8						filename_[128];		/* 文件名称，C风格字符串 */
		U32						filesize_;			/* 文件总大小 */
		U32						size_;				/* 当前文件块数据大小，最大值定位4096 */
		U8						data_[0];			/* 当前文件内容地址 */
	}__attribute__((__packed__))file_block_hdr_t;

#ifdef __cplusplus
}
#endif

#endif // __FILE_BLOCK_H__
