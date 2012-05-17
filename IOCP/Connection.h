#ifndef _CONNECTION_H
#define _CONNECTION_H
#include "KendyNet.h"
#include "wpacket.h"
#include "rpacket.h"
struct OVERLAPCONTEXT
{
	struct OverLapContext m_super;
	char   isUsed;
};

typedef void (*process_packet)(struct connection*,rpacket_t);
typedef void (*on_connection_destroy)(struct connection*);

#define MAX_WBAF 64

struct connection
{
	struct Socket socket;
	WSABUF wsendbuf[MAX_WBAF];
	WSABUF wrecvbuf[2];
	struct OVERLAPCONTEXT send_overlap;
	struct OVERLAPCONTEXT recv_overlap;

	unsigned long unpack_size; //��δ��������ݴ�С

	unsigned long unpack_pos;
	unsigned long next_recv_pos;

	buffer_t next_recv_buf;
	buffer_t unpack_buf; 
	
	struct link_list *send_list;//�����͵İ�
	process_packet _process_packet;
	on_connection_destroy _on_destroy;
};

struct connection *connection_create(SOCKET s,process_packet,on_connection_destroy);
void connection_destroy(struct connection**);

//�����Ѱ����뷢�Ͷ���
void connection_push_packet(struct connection*,wpacket_t);

//����ֵ:0,���ӶϿ�;��������
int connection_send(struct connection*,wpacket_t,int);

int connection_recv(struct connection*);

#endif