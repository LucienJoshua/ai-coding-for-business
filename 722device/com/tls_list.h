#ifndef __TLS_LIST_H__
#define __TLS_LIST_H__

#ifdef __cplusplus
extern "C"{
#endif // __cplusplus

	typedef struct __tls_list_node
	{
		struct __tls_list_node*		pre;
		struct __tls_list_node*		next;
	}tls_list_node;

	typedef tls_list_node			tls_list_node_t;


#define __LIST_START_NODE(head)				((head)->next)
#define __LIST_IS_LAST_NODE(head, node)		((node) != (head))
#define __LIST_NEXT_NODE(node)				((node)->next)

#define __LIST_CONTAIN_OF(address, type, field) \
	((type*)((char*)(address) - (char*)(&((type*)0)->field)))

	extern void						tls_list_initialize(tls_list_node_t* head);

	extern int						tls_list_empty(tls_list_node_t* head);
	
	extern tls_list_node_t*			tls_list_insert_tail(tls_list_node_t* head, tls_list_node_t* node);

	extern tls_list_node_t*			tls_list_remove_head(tls_list_node_t* head);

	extern tls_list_node_t*			tls_list_remove_node(tls_list_node_t* head, tls_list_node_t* node);

	extern tls_list_node_t*			tls_list_insert_tail_lock(tls_list_node_t* head, tls_list_node_t* node, void* lock);

	extern tls_list_node_t*			tls_list_remove_head_lock(tls_list_node_t* head, void* lock);

	extern tls_list_node_t*			tls_list_remove_node_lock(tls_list_node_t* head, tls_list_node_t* node);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __TLS_LIST_H__
