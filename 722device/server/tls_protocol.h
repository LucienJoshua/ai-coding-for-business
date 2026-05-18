#ifndef __TLS_PROTOCOL_H__
#define __TLS_PROTOCOL_H__



#define TLS_MSG_MAIN_VERSION			(0x00)

#define MSG_TYPE_REGISTER_REQUEST		(0x11)	/* 注册请求 */
#define MSG_TYPE_REGISTER_RESPONSE		(0x12)	/* 注册回复 */
#define MSG_TYPE_REGISTER_CANCEL		(0x13)  /* 注销 */

#define MSG_REGISTER_OK					(0)		/* 注册反馈成功的结果 */

#define MSG_TYPE_SEND_MESSAGE			(0x41)	/* 发送报文 */
#define MSG_TYPE_RECV_MESSAGE			(0x42)	/* 接收报文 */
#define MSG_TYPE_MESSAGE_ACK			(0x43)	/* 报文确认 */

#define MSG_TYPE_LINK_STATUS_REQUEST	(0x51)	/* 链路状态查询报文 */
#define MSG_TYPE_LINK_STATUS_RESPONSE	(0x52)	/* 链路状态回复报文 */


struct tls_message_head
{
	U8			version;				/* 主版本号 */
	U8			message_type;			/* 消息类型 */
	U32			reserve;				/* 保留字段 */
	U16			message_seq;			/* 消息序列号 */
}__attribute__ ((__packed__));

struct tls_register_request
{
	U32			src_addr;				/* 标识发送方系统地址信息 */
	U16			src_entry;				/* 应用系统实体号,应用系统内软件模块号 */
	U32			src_ip;					/* 应用系统的IP地址 */
	U32			reserve;				/* 保留字段 */
}__attribute__ ((__packed__));

struct tls_register_response
{
	U32			src_addr;				/* 标识发送方系统地址信息 */
	U16			src_entry;				/* 应用系统内软件模块号 */
	U8			result;					/* 反馈结果，0代表成功，非零代表失败 */	
}__attribute__  ((__packed__)) ;


#define MSG_TRANSFER_TYPE_P2P			(1)		/* 点对点传输 */

#define MSG_NO_RELIABLE					(0)		/* 无可靠性要求 */
#define MSG_RELIABLE					(1)		/* 有可靠性要求 */

#define MSG_PRIORITY_LOW				(0)		/* 低优先级 */
#define MSG_PRIORITY_HIGH				(1)		/* 高优先级 */

struct tls_message_content
{
	S8			id[16];					/* 报文唯一标识 */	
	U8			trans_type;				/* 报文传输类型 */
	U32			src_addr;				/* 发送方应用系统标识 */
	U16			src_entry;				/* 发送方应用实体标识 */
	U16			dst_count;				/* 接收方个数 */
	U32			dst_addr;				/* 接收方应用系统标识，接收方系统地址信息 */
	U16			dst_entry;				/* 接收方应用系统标识 */
	U32			msg_type;				/* 业务类型 */
	U8			reliable;				/* 可靠性 0-无可靠性要求，1-可靠性传输 */
	U8			priority;				/* 优先级，0-低优先级，1-高优先级 */
	U32			reserve;				/* 预留字段 */
	U16			timeout;				/* 超时时间 */
	U16			size;					/* 数据大小，最大为1024 */
	U8			payload[0];				/* 数据类容 */
}__attribute__ ((__packed__)) ;


#define MSG_ACK_STATUS_FAIL						(0x0000)	/* 失败 */
#define MSG_ACK_STATUS_LOCAL_RECEIVED			(0x0001)	/* 本端通信服务已接收 */
#define MSG_ACK_STATUS_REMOTE_RECEIVED			(0x0002)	/* 对端通信服务已接收 */
#define MSG_ACK_STATUS_REMOTE_PROCESSED			(0x0004)	/* 对端指控已处理 */

struct tls_message_ack
{
	S8			id[16];					/* 报文唯一标识 */	
	U32			src_addr;				/* 发送方应用系统标识 */
	U16			src_entry;				/* 发送方应用实体标识 */
	U32			dst_addr;				/* 接收方应用系统标识，接收方系统地址信息 */
	U16			dst_entry;				/* 接收方应用系统标识 */
	U32			msg_type;				/* 业务类型 */
	U32			reserve;				/* 预留字段 */
	U16			status;					/* 状态位 */
}__attribute__ ((__packed__));


typedef struct tls_message_head			tls_message_head_t;

typedef struct tls_register_request		tls_register_request_t;
typedef struct tls_register_response	tls_register_response_t;

typedef struct tls_message_content		tls_message_content_t;
typedef struct tls_message_ack			tls_message_ack_t;






#define	MAX_LINK_NUM					(4)
#define MAX_PATH_NUM					(4)


/* 选路策略类型 */
#define PATH_POLICY_MAXBD				(0x01)		/* 最大带宽策略 */
#define PATH_POLICY_MINDELAY			(0x02)		/* 最低时延策略 */
#define PATH_POLICY_MINHOP				(0x03)		/* 最小跳数策略 */


/* 链路类型定义 */
#define LINK_TYPE_CDB					(1)			/* 超短波 */
#define LINK_TYPE_WX					(2)			/* 卫星 */
#define LINK_TYPE_DB					(3)			/* 短波 */
#define LINK_TYPE_WB					(4)			/* 微波 */
#define LINK_TYPE_RD					(5)			/* 热点 */
#define LINK_TYPE_GPD					(6)			/* 高速台 */
#define LINK_TYPE_WIRE					(7)			/* 有线 */
#define LINK_TYPE_WIRELESS				(8)			/* 无线 */
#define LINK_TYPE_JG					(9)			/* 激光 */
#define LINK_TYPE_BKC					(10)		/* 编宽C */
#define LINK_TYPE_BKU					(11)		/* 编宽U */
#define LINK_TYPE_SS					(12)		/* 散射 */


typedef struct __tls_link_info
{
	U16					link_type;				/* 链路类型 */
	U32					max_bd;					/* 带宽, 单位bps*/
	U32					available_bd;			/* 可用带宽, 单位bps*/
	U32					reserve_bd;				/* 预留带宽, 单位bps */
	U32					time_delay;				/* 时延,链路平均时延*/
	U32					loss_rate;				/* 链路平均丢包率, 单位 千分之一 */
	U32					reserve;				/* 预留字段保留服务器IP地址 */
}__attribute__((__packed__))tls_link_info_t;

/* 路径消息 */
typedef struct __tls_path_info
{
	U64					pathid;					/* 路径唯一标识 */
	U8					hopnum;					/* 链路条数, 1-3 */
	tls_link_info_t		linkset[MAX_LINK_NUM];
}__attribute__((__packed__))tls_path_info_t;

/* 查询网络状态请求 */
typedef struct __tls_net_status_req_msg
{
	tls_message_head_t	msg_header;
	U8					sessionid[16];			/* 请求唯一标识 */
	U32					src_addr;				/* 源系统应用标识 */
	U16					src_entry;				/* 源应用实体标识 */
	U32					dst_addr;				/* 目的应用系统标识 */
	U16					dst_entry;				/* 目的应用实体标识 */
	U8					path_policy;			/* 选路策略 */
	U32					reserve;				/* 保留字段,1:自动路由, 2:指定路径 */
}__attribute__((__packed__))tls_net_status_req_msg_t;

/* 网络状态查询结果 */
typedef struct __tls_path_msg
{
	tls_message_head_t	msg_header;
	U8					sessionid[16];			/* 请求唯一标识 */
	U32					src_addr;				/* 源系统应用标识 */
	U16					src_entry;				/* 源应用实体标识 */
	U32					dst_addr;				/* 目的应用系统标识 */
	U16					dst_entry;				/* 目的应用实体标识 */
	U32					reserve;				/* 预留字段 */				
	U8					result;					/* 结果 0:成功, 1:失败 */
	U8					pathnum;				/* 路径条数, 最大不超过3条，0-3 */
	tls_path_info_t		pathinfo[MAX_PATH_NUM]; /* 路径信息 */
}__attribute__((__packed__))tls_path_msg_t;

#endif // __TLS_PROTOCOL_H__
