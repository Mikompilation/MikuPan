#ifndef SCE_SIFRPC_H
#define SCE_SIFRPC_H

struct _sif_client_data;
struct _sif_serve_data;

struct _sif_queue_data { // 0x18
	/* 0x00 */ int key;
	/* 0x04 */ int active;
	/* 0x08 */ struct _sif_serve_data *link;
	/* 0x0c */ struct _sif_serve_data *start;
	/* 0x10 */ struct _sif_serve_data *end;
	/* 0x14 */ struct _sif_queue_data *next;
};

typedef struct _sif_client_data sceSifClientData;
typedef struct _sif_queue_data sceSifQueueData;
typedef struct _sif_serve_data sceSifServeData;
typedef void* (*sceSifRpcFunc)(unsigned int command, void* data, int size);

struct _sif_serve_data { // 0x44
	/* 0x00 */ unsigned int command;
	/* 0x04 */ sceSifRpcFunc func;
	/* 0x08 */ void *buff;
	/* 0x0c */ int size;
	/* 0x10 */ sceSifRpcFunc cfunc;
	/* 0x14 */ void *cbuff;
	/* 0x18 */ int csize;
	/* 0x1c */ sceSifClientData *client;
	/* 0x20 */ void *paddr;
	/* 0x24 */ unsigned int fno;
	/* 0x28 */ void *receive;
	/* 0x2c */ int rsize;
	/* 0x30 */ int rmode;
	/* 0x34 */ unsigned int rid;
	/* 0x38 */ struct _sif_serve_data *link;
	/* 0x3c */ struct _sif_serve_data *next;
	/* 0x40 */ struct _sif_queue_data *base;
};

typedef struct _sif_rpc_data {
	void *paddr;
	unsigned int pid;
	int tid;
	unsigned int mode;
} sceSifRpcData;

typedef void (* sceSifEndFunc)(void *);

typedef struct _sif_client_data {
	struct _sif_rpc_data rpcd;
	unsigned int command;
	void *buff;
	void *gp;
	sceSifEndFunc func;
	void *para;
	struct _sif_serve_data *serve;
} sceSifClientData;

void sceSifInitRpc(unsigned int mode);
int sceSifBindRpc(sceSifClientData *client, unsigned int rpc_number, unsigned int mode);
int sceSifCheckStatRpc(sceSifRpcData *cd);
int sceSifCallRpc(sceSifClientData *client, unsigned int rpc_number, unsigned int mode, void * send, int ssize, void * receive, int rsize, sceSifEndFunc end_function, void *end_param);
int sceSifSetRpcQueue(sceSifQueueData *qd, int thread_id);
int sceSifRegisterRpc(sceSifServeData *sd, unsigned int command, sceSifRpcFunc func, void *buff, sceSifRpcFunc cfunc, void *cbuff, sceSifQueueData *qd);
void sceSifRpcLoop(sceSifQueueData *qd);


#endif // SCE_SIFRPC_H
