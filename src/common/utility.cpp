#include "mikupan/mikupan_region.h"
#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "utility.h"

#include <ctype.h>


MIKUPAN_NAMESPACE_BEGIN

char *StrToLower(char *str)
{
    char *strp;
    
    strp = str;
    
    while (*strp)
    {
        *strp = tolower(*strp);
        strp++;
    }
    
    return str;
}

MIKUPAN_NAMESPACE_END
