#include "sifrpc.h"
#include <stdio.h>

#include "enums.h"
#include "os/eeiop/eeiop.h"
#include "iop/iopsys.h"

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

void ReadFileInArchive(int sector, int size, int64_t address);
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
    if (client == &ei_sys.gcd) {
        void *ret = IopDrvFunc(rpc_number, send, ssize);
        memcpy(receive, ret, rsize);
    }

    return 1;
}
