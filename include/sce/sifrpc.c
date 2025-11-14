#include "sifrpc.h"
#include <stdio.h>

#include "enums.h"
#include "os/eeiop/eeiop.h"

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
    if (rpc_number == 1 && mode == 1)
    {
        IOP_COMMAND* send_cmd = (IOP_COMMAND*)send;
        int total_cmd = rsize / sizeof(IOP_COMMAND);

        for (int i = 0; i < total_cmd; i++)
        {
            printf("IOP server received command number: %d\n", send_cmd[i].cmd_no);

            switch (send_cmd[i].cmd_no)
            {
            default: printf("Error: Command %d not yet implemented!\n", send_cmd[i].cmd_no); break;
            case IC_CDVD_INIT:  break;
            case IC_CDVD_LOAD_SECT:
                    /// Indicates file load
                    if (send_cmd[i].data5 == 0)
                    {
                        ReadFileInArchive(send_cmd[i].data2, send_cmd[i].data3, send_cmd[i].data4);
                    }
                    /// TODO : Implement SoundEffect Loading
                    /// Sound Effect load request
                    else if (send_cmd[i].data5 == 2)
                    {}

                    break;
            }
        }
    }

    return 1;
}
