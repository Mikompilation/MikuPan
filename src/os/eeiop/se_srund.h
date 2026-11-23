#include "mikupan/mikupan_region.h"
#ifndef OS_EEIOP_SE_SRUND_H
#define OS_EEIOP_SE_SRUND_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

int SSrundLoadReqAndSetSub(int load_id, u_char room_from, u_char room_to);
int SSrundLoadReqAndSet(int load_id, u_short door_id);
u_char SSrundGetSeNo(int file_no);


MIKUPAN_NAMESPACE_END
#endif // OS_EEIOP_SE_SRUND_H
