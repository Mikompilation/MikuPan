#include "common.h"
#include "ig_rank.h"

SAVE_RANK save_rank;
static MENU_RANK menu_rank;

void NewgameMenuRankInit()
{    
    memset(&menu_rank, 0, sizeof(menu_rank));
    memset(&save_rank, 0, sizeof(save_rank));
}

void LoadgameMenuRankInit()
{
}

void StartRankModeInit()
{
}

void IngameMenuRank()
{
}

void IngameMenuRankDisp()
{
}

void RankingChkMem(PICTURE_WRK new_pic)
{
}

int CheckSamePic(PICTURE_WRK *newp)
{
    return 0;
}
