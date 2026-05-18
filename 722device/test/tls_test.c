#include "tls_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "tls_config.h"
#include "tls_server.h"
#include "tls_statis_rate.h"


typedef struct queue_transmit_test
{
	S32					run;
	tls_thread_t		queue_thread_1;
	tls_thread_t		queue_thread_2;
	void*				usr;
}queue_transmit_test_t;

typedef struct __tls_statis_rate_test
{
	S32					test_run;
	tls_thread_t		test_thread;
	tls_event_t			test_evt_loop;
	tls_event_t			test_evt_stop;

	S32					read_run;
	tls_thread_t		read_thread;
	tls_event_t			read_evt_loop;
	tls_event_t			read_evt_stop;
}tls_statis_rate_test_t;

S32					g_run = 0;
tls_thread_t*		g_test_thread = NULL;
tls_event_t			g_wait_event;


static tls_server_t*			g_tls_server = NULL;
static tls_statis_rate_test_t	g_rate_test;	


static void test_for_tls_event(void);
static void test_for_tls_atomic(void);
static void test_for_tls_list(void);
static void test_for_buff_queue(void);
static void* test_thread_routine(void* arg);

static void test_for_statis_rate(void);
static void* test_for_statis_rate_thread(void* arg);
static void* test_for_read_statis_thread(void* arg);


static queue_transmit_test_t	g_queue_test;

static void test_for_transmit_queue(void);
static void start_transmit_queue_thread(void);
static void* transmit_queue_thread_1(void* arg);
static void* transmit_queue_thread_2(void* arg);


static void tls_sleep_us(U32 us);

void on_tls_test(void)
{
	// test_for_transmit_queue();
	// test_for_tls_list();
	test_for_statis_rate();
}


void* test_thread_routine(void* arg)
{
	__TLSDBG("start the test thread.\n");	
	while (1)
	{
		tls_event_wait(&g_wait_event);

		__TLSDBG("thread process.\n");

		tls_event_reset(&g_wait_event);
	}
	return NULL;
}
void test_for_buff_queue(void)
{
	TLS_RESULT		result;
	tls_buff_queue_t  test_queue;

	tls_buff_t*			buff;
	tls_list_node_t*	list_head;
	tls_list_node_t*	list_node;
	int					index;

	result = tls_buff_queue_initialize(&test_queue);
	__TLSDBG("tls_buff_queue_initialize reuslt:%d.\n", result);

	tls_buff_queue_allocate(&test_queue, 2048, 1024);

	// tls_buff_queue_uninitialize(&test_queue);

	buff = tls_buff_queue_acquire(&test_queue);

	__TLSDBG("acquire a buff, size:%d.\n", buff->buf_size_);

	tls_buff_queue_release(&test_queue, buff);

	index = 0;
	list_head = &test_queue.list_buff_;
	list_node = __LIST_START_NODE(list_head);		
	while (__LIST_IS_LAST_NODE(list_head, list_node))
	{
		buff = __LIST_CONTAIN_OF(list_node, tls_buff_t, list_node_);
		if (buff)
		{
			__TLSDBG("index:%d, buff size:%d.\n", index++, buff->buf_size_);
		}
		list_node = __LIST_NEXT_NODE(list_node);
	}

}


void test_for_tls_atomic(void)
{
	int value = 0;
	int oldvalue = 0;
	int newvalue = 1;

	int ret = 0;

	ret = __sync_val_compare_and_swap(&value, oldvalue, newvalue);

	__TLSDBG("value:%d, ret:%d.\n", value, ret);

	ret = __sync_val_compare_and_swap(&value, oldvalue, newvalue);

	__TLSDBG("value:%d, ret:%d.\n", value, ret);

	oldvalue = 1;
	newvalue = 0;

	ret = __sync_val_compare_and_swap(&value, oldvalue, newvalue);
	__TLSDBG("value:%d, ret:%d.\n", value, ret);

	ret = __sync_val_compare_and_swap(&value, oldvalue, newvalue);
	__TLSDBG("value:%d, ret:%d.\n", value, ret);
}
void test_for_tls_event(void)
{
	memset(&g_wait_event, 0, sizeof g_wait_event);
	tls_event_initialize(&g_wait_event);

	g_test_thread = tls_thread_allocate();
	assert(g_test_thread);

	g_test_thread->thread_routine_ = test_thread_routine;
	g_run = 1;
	tls_thread_start(g_test_thread);

	int count = 4;
	while (count--)
	{
		sleep(1);
		tls_event_set(&g_wait_event);
	}
}
typedef struct test_node
{
	tls_list_node_t	list_node;
	int				index;
}test_node;

void test_for_tls_list()
{
	tls_list_node_t			list;

	tls_list_node_t*		list_head;
	tls_list_node_t*		list_node;
	tls_list_node_t*		next_node;

	test_node*				node;

	tls_list_initialize(&list);

	for (int i = 0; i < 10; i++)
	{
		node = (test_node*)malloc(sizeof(test_node));
		memset(node, 0, sizeof(*node));
		node->index = i;
		tls_list_initialize(&node->list_node);

		tls_list_insert_tail(&list, &node->list_node);
	}

	list_head = &list;
	list_node = __LIST_START_NODE(list_head);		
	while (__LIST_IS_LAST_NODE(list_head, list_node))
	{
		node = __LIST_CONTAIN_OF(list_node, test_node, list_node);
		if (node)
		{
			next_node = __LIST_NEXT_NODE(list_node);
			tls_list_remove_node(list_head, &node->list_node);
			list_node = next_node;
			__TLSDBG("remove node index:%d\n", node->index);
		}
		else
		{
			list_node = __LIST_NEXT_NODE(list_node);
		}
	}

	__TLSDBG("==========================\n");
	list_head = &list;
	printf("empty:%d\n", tls_list_empty(list_head));
	list_node = __LIST_START_NODE(list_head);		
	while (__LIST_IS_LAST_NODE(list_head, list_node))
	{
		node = __LIST_CONTAIN_OF(list_node, test_node, list_node);
		if (node)
		{
			__TLSDBG("node index:%d.\n", node->index);
		}
		list_node = __LIST_NEXT_NODE(list_node);
	}

	__TLSDBG("\n");







	list_head = &list;
	while (!tls_list_empty(list_head))
	{
		list_node = tls_list_remove_head(list_head);
		node = __LIST_CONTAIN_OF(list_node, test_node, list_node);
		if (node)
		{
			__TLSDBG("delete node index:%d.\n", node->index);
			free(node);
		}
	}
}


void test_for_transmit_queue(void)
{
	TLS_RESULT				result = TLS_RESULT_E_FAIL;
	tls_config_t			config;
	tls_param_t				param;
	tls_server_t*			server = NULL;

	__TLSDBG("------------------ tls ------------------.\n");
	do
	{
		
		tls_config_load("./tls_config.xml", &config);
	
		memset(&param, 0, sizeof(param));
		strcpy(param.local_ip_, config.tls_local_ip);
		param.local_port_ = config.tls_local_port;
		strcpy(param.remote_ip_, config.tls_remote_ip);
		param.remote_port_ = config.tls_remote_port;
		param.tls_local_id_ = config.tls_local_id;
		param.tls_local_sub_id_ = config.tls_local_sub_id;
		param.priv_data_ = NULL;
	
		param.on_tls_register_callback = NULL;
		param.on_tls_receive_callback = NULL;	
	
		server = tls_server_allocate();
		assert(server);
		memset(server, 0, sizeof(*server));
	
		result = tls_server_initialize(server, &param);
		if (TlsResultFail(result))
		{
			__TLSDBG("server init fail.\n");
			break;
		}
	
		__TLSDBG("Server initialize OK\n");
	
		result = tls_server_start(server);
		if (TlsResultFail(result))
		{
			__TLSDBG("server start fail.\n");
			break;
		}		

		tls_server_register(server, param.tls_local_id_, param.tls_local_sub_id_);
	}while(0);

	if (TlsResultOk(result))
	{
		g_tls_server = server;
		__TLSDBG("Server start OK\n");

		start_transmit_queue_thread();
	}
	else
	{
		tls_server_uninitialize(server);
		g_tls_server = NULL;
	}

}
void start_transmit_queue_thread(void)
{
	memset(&g_queue_test, 0, sizeof g_queue_test);
	g_queue_test.usr = g_tls_server;

	g_queue_test.run = 1;
	g_queue_test.queue_thread_1.thread_routine_ = transmit_queue_thread_1;
	g_queue_test.queue_thread_1.arg_ = (void*)&g_queue_test;

	g_queue_test.queue_thread_2.thread_routine_ = transmit_queue_thread_2;
	g_queue_test.queue_thread_2.arg_ = (void*)&g_queue_test;

	tls_thread_start(&g_queue_test.queue_thread_1);
	tls_thread_start(&g_queue_test.queue_thread_2);
}
void* transmit_queue_thread_1(void* arg)
{
	U8							data[2048];
	U32							size;
	U8							priority = 0;
	queue_transmit_test_t*		queue_test;
	tls_server_t*				s;
	tls_session_t				session;

	queue_test = (queue_transmit_test_t*)arg;
	s = (tls_server_t*)queue_test->usr;

	memset(data, 0x12, sizeof data);
	size = 640;

	while (1)
	{
		srand((unsigned)time(NULL));
		priority = rand() % 4;
		memset(&session, 0, sizeof session);
		session.tls_priority_ = priority;
		tls_server_transmit(s, &session, data, size);

		usleep(1000 * 1000);
	}
	return NULL;
}
void* transmit_queue_thread_2(void* arg)
{
	U8							data[2048];
	U32							size;
	U8							priority = 0;
	queue_transmit_test_t*		queue_test;
	tls_server_t*				s;
	tls_session_t				session;

	queue_test = (queue_transmit_test_t*)arg;
	s = (tls_server_t*)queue_test->usr;

	memset(data, 0x12, sizeof data);
	size = 120;

	while (1)
	{
		srand((unsigned)time(NULL));
		priority = rand() % 7;
		memset(&session, 0, sizeof session);
		session.tls_priority_ = priority;
		tls_server_transmit(s, &session, data, size);

		usleep(100 * 1000);
	}
	return NULL;
}

static void test_for_statis_rate(void)
{
	memset(&g_rate_test, 0, sizeof(g_rate_test));

	tls_event_initialize(&g_rate_test.test_evt_loop);
	tls_event_initialize(&g_rate_test.test_evt_stop);


	g_rate_test.read_run = 1;
	g_rate_test.read_thread.arg_ = (void*)&g_rate_test;
	g_rate_test.read_thread.thread_routine_ = test_for_read_statis_thread;
	tls_event_set(&g_rate_test.read_evt_loop);
	tls_thread_start(&g_rate_test.read_thread);

	g_rate_test.test_run = 1;
	g_rate_test.test_thread.arg_ = (void*)&g_rate_test;
	g_rate_test.test_thread.thread_routine_ = test_for_statis_rate_thread;
	tls_event_set(&g_rate_test.test_evt_loop);
	tls_thread_start(&g_rate_test.test_thread);
}
void* test_for_statis_rate_thread(void* arg)
{
	tls_statis_rate_test_t* rate_test = (tls_statis_rate_test_t*)arg;
	U64				input_tx_byte = 1472;
	U64				output_tx_byte = 0;
	U64				rx_byte = 1472;

	while (rate_test->test_run)
	{
		// input_tx_byte = 1024;
		// tls_statis_rate_tx_input(input_tx_byte);
		tls_statis_rate_rx(rx_byte);
		tls_sleep_us(10000);
		// usleep(10000);
	}
}
void* test_for_read_statis_thread(void* arg)
{
	tls_statis_rate_test_t* rate_test = (tls_statis_rate_test_t*)arg;
	U64				rate = 0;
	char			rate_text[256];

	while (rate_test->read_run)
	{
		memset(rate_text, 0, sizeof(rate_text));
		// rate = tls_statis_rate_get_tx_input();
		rate = tls_statis_rate_get_rx();
		tls_statis_rate_get_text(rate_text, rate);
		printf("result rate:%s\n", rate_text);
		sleep(1);
	}
}

void tls_sleep_us(U32 us)
{
	struct timeval		t1;
	struct timeval		t2;

	gettimeofday(&t1, NULL);

	do
	{
		gettimeofday(&t2, NULL);
	}while(((t2.tv_sec * 1e6 + t2.tv_usec) - (t1.tv_sec * 1e6 + t1.tv_usec)) < us);
}
