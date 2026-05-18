#ifndef __APP_LINKINFO_H__
#define __APP_LINKINFO_H__


#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>


#define	APP_MAX_LINK_NUM					(4)
#define APP_MAX_PATH_NUM					(4)


/* 选路策略类型 */
#define APP_PATH_POLICY_MAXBD				(0x01)		/* 最大带宽策略 */
#define APP_PATH_POLICY_MINDELAY			(0x02)		/* 最低时延策略 */
#define APP_PATH_POLICY_MINHOP				(0x03)		/* 最小跳数策略 */


/* 链路类型定义 */
#define APP_LINK_TYPE_CDB					(1)			/* 超短波 */
#define APP_LINK_TYPE_WX					(2)			/* 卫星 */
#define APP_LINK_TYPE_DB					(3)			/* 短波 */
#define APP_LINK_TYPE_WB					(4)			/* 微波 */
#define APP_LINK_TYPE_RD					(5)			/* 热点 */
#define APP_LINK_TYPE_GPD					(6)			/* 高速台 */
#define APP_LINK_TYPE_WIRE					(7)			/* 有线 */
#define APP_LINK_TYPE_WIRELESS				(8)			/* 无线 */
#define APP_LINK_TYPE_JG					(9)			/* 激光 */
#define APP_LINK_TYPE_BKC					(10)		/* 编宽C */
#define APP_LINK_TYPE_BKU					(11)		/* 编宽U */
#define APP_LINK_TYPE_SS					(12)		/* 散射 */


/* link info 请求数据结构 */
typedef struct __app_linkinfo_request
{
	uint32_t			dst_app_id;			/* 目的应用系统标识 */
	uint16_t			dst_entity_id;		/* 目的应用实体标识 */
	uint16_t			path_policy;		/* 选路策略 */
	uint32_t			reserve;			/* 保留字段 */
}__attribute__((__packed__))app_linkinfo_request_t;







typedef struct __app_linkinfo
{
	uint16_t			link_type;				/* 链路类型 */
	uint32_t			max_bd;					/* 最大带宽, 单位bps*/
	uint32_t			available_bd;			/* 可用带宽, 单位bps*/
	uint32_t			reserve_bd;				/* 预留带宽, 单位bps */
	uint32_t			time_delay;				/* 时延,链路平均时延*/
	uint32_t			loss_rate;				/* 链路平均丢包率, 单位 千分之一 */
	uint32_t			reserve;				/* 预留字段保留服务器IP地址 */
}__attribute__((__packed__))app_linkinfo_t;


typedef struct __app_pathinfo
{
	uint64_t			pathid;						/* 路径唯一标识 */
	uint8_t				hopnum;						/* 链路条数,1-3*/
	app_linkinfo_t		linkset[APP_MAX_LINK_NUM];	/* 链路信息数组 */
}__attribute__((__packed__))app_pathinfo_t;


/* link info 请求后的回复  */
typedef struct __app_linkinfo_result
{
	uint32_t			src_app_id;					/* 源系统应用标识 */
	uint16_t			src_entity_id;				/* 源应用实体标识 */
	uint32_t			dst_app_id;					/* 目的应用系统标识 */
	uint16_t			dst_entity_id;				/* 目的应用实体标识 */
	uint32_t			reserve;					/* 预留字段 */				
	uint8_t				result;						/* 结果 0:成功, 1:失败 */
	uint8_t				pathnum;					/* 路径条数, 最大不超过3条，0-3 */
	app_pathinfo_t		pathinfo[APP_MAX_PATH_NUM]; /* 路径信息 */
}__attribute__((__packed__))app_linkinfo_result_t;



#ifdef __cplusplus
}
#endif


#endif // __APP_LINKINFO_H__
