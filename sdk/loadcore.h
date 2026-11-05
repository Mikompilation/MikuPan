#include "mikupan/mikupan_region.h"
#ifndef MIKUPAN_IOP_LOADCORE_H
#define MIKUPAN_IOP_LOADCORE_H


MIKUPAN_NAMESPACE_BEGIN

static inline int RegisterLibraryEntries(void* exports)
{
    (void)exports;
    return 0;
}

static inline int ReleaseLibraryEntries(void* exports)
{
    (void)exports;
    return 0;
}


MIKUPAN_NAMESPACE_END
#endif
