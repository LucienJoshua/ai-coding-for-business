CC=gcc
CFLAGS=-c -Wall -g
TARGET=./bin/tls
LIB_DIR = ./third/libvsoa/libs/x86_64
#LIB_DIR = ./third/libvsoa/libs/aarch64
LIB_DIR_ZLOG = ./third/libzlog/lib
LDFLAGS +=-L $(LIB_DIR)
LDFLAGS +=-L $(LIB_DIR_ZLOG)

INCLUDE+=-I ./
INCLUDE+=-I ./include
INCLUDE+=-I ./com
INCLUDE+=-I ./entry
INCLUDE+=-I ./server
INCLUDE+=-I ./network
INCLUDE+=-I ./config
INCLUDE+=-I ./config/mxml
INCLUDE+=-I ./third/libvsoa/include
INCLUDE+=-I ./third/libvsoa/include/libjson
INCLUDE+=-I ./third/libvsoa/include/libvsoa
INCLUDE+=-I ./app
INCLUDE+=-I ./test
INCLUDE+=-I ./statis
INCLUDE+=-I ./dialog
INCLUDE+=-I ./vsoa
INCLUDE+=-I ./filetransmit
INCLUDE+=-I ./third/libzlog/include

OBJECT+=main.o
OBJECT+=./com/tls_list.o
OBJECT+=./com/tls_thread.o
OBJECT+=./com/tls_event.o
OBJECT+=./com/tls_lock.o
OBJECT+=./com/tls_log.o
OBJECT+=./server/tls_entry.o
OBJECT+=./server/tls_server.o
OBJECT+=./server/tls_buff_queue.o
OBJECT+=./server/tls_transmit.o
OBJECT+=./server/tls_receive.o
OBJECT+=./server/tls_work.o
OBJECT+=./server/tls_linkinfo.o
OBJECT+=./server/tls_request.o
OBJECT+=./network/tls_udp_socket.o
OBJECT+=./network/tls_net_proxy.o
OBJECT+=./include/cJSON.o
OBJECT+=./app/app_transport.o
OBJECT+=./app/app_transmit.o
OBJECT+=./app/app_udp_endpoint.o
OBJECT+=./test/tls_test.o
OBJECT+=./statis/tls_statis.o
OBJECT+=./statis/tls_statis_rate.o
OBJECT+=./dialog/tcp_server.o
OBJECT+=./vsoa/tls_vsoa_subscribe.o
OBJECT+=./vsoa/tls_vsoa_publish.o
OBJECT+=./app/app_vsoa_rpc_server.o
OBJECT+=./app/app_vsoa_publish.o
OBJECT+=./app/app_vsoa_subscribe.o
OBJECT+=./filetransmit/file_transmit.o
OBJECT+=./filetransmit/file_entry.o

OBJECT+=./config/tls_config.o
OBJECT+=./config/app_config.o
OBJECT+=./config/mxml/mxml-attr.o
OBJECT+=./config/mxml/mxml-file.o
OBJECT+=./config/mxml/mxml-get.o
OBJECT+=./config/mxml/mxml-index.o
OBJECT+=./config/mxml/mxml-node.o
OBJECT+=./config/mxml/mxml-options.o
OBJECT+=./config/mxml/mxml-private.o
OBJECT+=./config/mxml/mxml-search.o

LIBS += -lpthread
LIBS += -lvsoa-client
LIBS += -lvsoa-hpserv
LIBS += -lvsoa-json
LIBS += -lvsoa-parser
LIBS += -lvsoa-position
LIBS += -lvsoa-server
LIBS += -lzlog

CLEANFLAGS+=./bin/tls
CLEANFLAGS+=./*.o
CLEANFLAGS+=./com/*.o
CLEANFLAGS+=./server/*.o
CLEANFLAGS+=./network/*.o
CLEANFLAGS+=./config/*.o
CLEANFLAGS+=./config/mxml/*.o
CLEANFLAGS+=./app/*.o
CLEANFLAGS+=./test/*.o
CLEANFLAGS+=./statis/*.o
CLEANFLAGS+=./dialog/*.o
CLEANFLAGS+=./vsoa/*.o
CLEANFLAGS+=./filetransmit/*.o

#$(TARGET):$(OBJECT)
#	$(CC) $^ -o $(TARGET) -lpthread

#%.o:%.c
#	$(CC) $(INCLUDE) $^ $(CFLAGS) -o $@

$(TARGET):$(OBJECT) 
	$(CC) $^ -o $(TARGET) $(LDFLAGS) $(LIBS)

%.o:%.c
	$(CC) $(INCLUDE) $^ $(CFLAGS) -o $@

.PHONY:clean
clean:
	$(RM) $(CLEANFLAGS)
#$(RM) *.o ./com/*.o ./server/*.o ./network/*.o ./tls
