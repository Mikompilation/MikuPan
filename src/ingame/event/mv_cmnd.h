#include "mikupan/mikupan_region.h"
#ifndef INGAME_EVENT_MV_CMND_H
#define INGAME_EVENT_MV_CMND_H

#include "typedefs.h"

#include <cstdint>


MIKUPAN_NAMESPACE_BEGIN

typedef struct {
    short int msg_no;
    short int msg_wait;
} MOVIE_CMD_WRK;

// extern MOVIE_CMD_WRK mvcmd_wrk;

void EventMovieCommandInit();
void EventMovieCommandMain(int flame, int top_addr);
int64_t GetMovieMessageAddr(short int msg_no);


MIKUPAN_NAMESPACE_END
#endif // INGAME_EVENT_MV_CMND_H