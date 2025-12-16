#include "common.h"
#include "ap_pgost.h"

PGOST_DAT pg_dat[] = {
    {
        .score = 500,
        .efct_no = 0,
        .room = 9,
        .pos = {3400, -600, 2800},
    },
    {
        .score = 500,
        .efct_no = 0,
        .room = 22,
        .pos = {7900, -550, 2750},
    },
    {
        .score = 500,
        .efct_no = 0,
        .room = 10,
        .pos = {400, -500, 3250},
    },
    {
        .score = 750,
        .efct_no = 0,
        .room = 17,
        .pos = {170, -975, 1650},
    },
    {
        .score = 750,
        .efct_no = 0,
        .room = 2,
        .pos = {500, -1925, 3250},
    },
    {
        .score = 750,
        .efct_no = 0,
        .room = 8,
        .pos = {4550, -375, 1690},
    },
    {
        .score = 1000,
        .efct_no = 0,
        .room = 15,
        .pos = {3125, -440, 1725},
    },
};

int test_rg_no = 0;
PGOST_WRK pg_wrk = {0};

void PuzzleGhostInit()
{
}

void PuzzleGhostLoadReq(u_char pg_no)
{
}

void PuzzleGhostReq(u_char pg_no)
{
}

int PuzzleGhostContinue(u_char ev_no)
{
}

void PuzzleGhostPhotoReq()
{
}

void PuzzleGhostMain()
{
}

void PuzzleGhostDispSet()
{
}

void PuzzleGhostDispMain()
{
}
