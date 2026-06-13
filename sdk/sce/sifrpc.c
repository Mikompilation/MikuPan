#include "sifrpc.h"
#include <stdio.h>

#include "enums.h"
#include "iop/iopmain.h"
#include "os/eeiop/eeiop.h"

#include <stdlib.h>
#include <string.h>

void sceSifInitRpc(unsigned int mode)
{
}

int sceSifBindRpc(sceSifClientData* client, unsigned int rpc_number, unsigned int mode)
{
    /// Returns success otherwise program will loop forever
    if (client->serve == NULL)
    {
        client->serve = malloc(sizeof(struct _sif_serve_data));
    }

    return 1;
}

int sceSifCheckStatRpc(sceSifRpcData* cd)
{
    /// Returns 0 to avoid program looping forever
    return 0;
}

/**
 *
 * @param client
 * @param rpc_number
 * @param mode
 * @param send Pointer to the data
 * @param ssize Total size of data
 * @param receive
 * @param rsize
 * @param end_function Completion function callback
 * @param end_param Arguments for completion function callback
 * @return
 */
int sceSifCallRpc(sceSifClientData* client, unsigned int rpc_number, unsigned int mode, void* send, int ssize,
                  void* receive, int rsize, sceSifEndFunc end_function, void* end_param)
{
    (void)mode;
    (void)end_function;
    (void)end_param;

    if (client == &ei_sys.gcd)
    {
        void *ret = IopDrvFunc(rpc_number, send, ssize);
        if (receive != NULL && ret != NULL && rsize > 0)
        {
            memcpy(receive, ret, rsize);
        }
    }

    return 1;
}

int sceSifSetRpcQueue(sceSifQueueData* qd, int thread_id)
{
    if (qd != NULL)
    {
        memset(qd, 0, sizeof(*qd));
        qd->key = thread_id;
        qd->active = 1;
    }

    return 0;
}

int sceSifRegisterRpc(sceSifServeData* sd, unsigned int command, sceSifRpcFunc func, void* buff, sceSifRpcFunc cfunc, void* cbuff, sceSifQueueData* qd)
{
    if (sd != NULL)
    {
        memset(sd, 0, sizeof(*sd));
        sd->command = command;
        sd->func = func;
        sd->buff = buff;
        sd->cfunc = cfunc;
        sd->cbuff = cbuff;
        sd->base = qd;
    }

    return 0;
}

void sceSifRpcLoop(sceSifQueueData* qd)
{
    (void)qd;
}
