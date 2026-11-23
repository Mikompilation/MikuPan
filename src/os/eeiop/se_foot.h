#include "mikupan/mikupan_region.h"
#ifndef OS_EEIOP_SE_FOOT_H
#define OS_EEIOP_SE_FOOT_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

int SFootLoadReqAndSetSub(int load_id, u_char room_from, u_char room_to);
int SFootLoadReqAndSet(int load_id, u_short door_id);
u_char SFootGetSeNo(int call_no);
u_char SFootGetSeNoFromFileNo(int file_no);


MIKUPAN_NAMESPACE_END
#endif // OS_EEIOP_SE_FOOT_H
