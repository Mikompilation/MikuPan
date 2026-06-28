#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "utility.h"

#include <ctype.h>

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
