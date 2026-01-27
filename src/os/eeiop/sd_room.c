#include "sd_room.h"
#include "common.h"
#include "enums.h"
#include "typedefs.h"

#include "eese.h"
#include "main/glob.h"

#define MAX_VOL 0x2fff
#define RM_DOOR_SIZE 42

typedef struct {
  u_int fno[3];
} ROOM_DOOR_SE;

typedef struct {
  u_int fno[5];
} ROOM_FOOT_SE;

ADPCM_ROOM_PLAY adpcm_param_tbl[] = {
    {
        .file_no = 1532,
        .vol = 16383,
    },
    {
        .file_no = 1533,
        .vol = 16383,
    },
    {
        .file_no = 1534,
        .vol = 16383,
    },
    {
        .file_no = 1535,
        .vol = 16383,
    },
    {
        .file_no = 1536,
        .vol = 16383,
    },
    {
        .file_no = 1537,
        .vol = 16383,
    },
    {
        .file_no = 1538,
        .vol = 16383,
    },
    {
        .file_no = 1539,
        .vol = 16383,
    },
    {
        .file_no = 1540,
        .vol = 16383,
    },
    {
        .file_no = 1541,
        .vol = 16383,
    },
    {
        .file_no = 1542,
        .vol = 16383,
    },
    {
        .file_no = 1543,
        .vol = 16383,
    },
    {
        .file_no = 1544,
        .vol = 16383,
    },
    {
        .file_no = 1551,
        .vol = 16383,
    },
    {
        .file_no = 1552,
        .vol = 16383,
    },
    {
        .file_no = 1556,
        .vol = 16383,
    },
    {
        .file_no = 1557,
        .vol = 16383,
    },
    {
        .file_no = 1558,
        .vol = 16383,
    },
    {
        .file_no = 1559,
        .vol = 16383,
    },
    {
        .file_no = 1560,
        .vol = 16383,
    },
    {
        .file_no = 1561,
        .vol = 16383,
    },
    {
        .file_no = 1545,
        .vol = 16383,
    },
    {
        .file_no = 1563,
        .vol = 16383,
    },
    {
        .file_no = -1,
        .vol = 16383,
    },
};

int foot_se_index[] = { 1452, 1444, 1445, 1446, 1447, 1448, 1450, 1451, 1453, 1454, 1456, 1457, 1458, 1459, 1460, 1461, 1462, 1463, 1464, 1465, 1466, 1467, 1468, 1469, 1470, 1471, 1455, 1449 };

static ROOM_SOUND_INFO rm_snd_info[] = {
    {
        .rvol = 12287,
        .door_no = 0,
        .foot_no = 0,
        .srund_no = 0,
        .event_no = 0,
        .adpcm = {3, 4, 11, 2, 7},
    },
    {
        .rvol = 12287,
        .door_no = 1,
        .foot_no = 1,
        .srund_no = 1,
        .event_no = 1,
        .adpcm = {3, 3, 3, 3, 7},
    },
    {
        .rvol = 16383,
        .door_no = 2,
        .foot_no = 2,
        .srund_no = 2,
        .event_no = 2,
        .adpcm = {3, 4, 4, 2, 7},
    },
    {
        .rvol = 24575,
        .door_no = 3,
        .foot_no = 3,
        .srund_no = 3,
        .event_no = 3,
        .adpcm = {3, 1, 11, 2, 7},
    },
    {
        .rvol = 8191,
        .door_no = 4,
        .foot_no = 4,
        .srund_no = 4,
        .event_no = 4,
        .adpcm = {8, 8, 8, 8, 8},
    },
    {
        .rvol = 20479,
        .door_no = 5,
        .foot_no = 5,
        .srund_no = 5,
        .event_no = 5,
        .adpcm = {1, 1, 9, 2, 7},
    },
    {
        .rvol = 12287,
        .door_no = 6,
        .foot_no = 6,
        .srund_no = 6,
        .event_no = 6,
        .adpcm = {23, 23, 23, 22, 7},
    },
    {
        .rvol = 12287,
        .door_no = 7,
        .foot_no = 7,
        .srund_no = 7,
        .event_no = 7,
        .adpcm = {10, 10, 11, 2, 7},
    },
    {
        .rvol = 16383,
        .door_no = 8,
        .foot_no = 8,
        .srund_no = 8,
        .event_no = 8,
        .adpcm = {2, 1, 1, 2, 7},
    },
    {
        .rvol = 12287,
        .door_no = 9,
        .foot_no = 9,
        .srund_no = 9,
        .event_no = 9,
        .adpcm = {8, 5, 23, 23, 8},
    },
    {
        .rvol = 8191,
        .door_no = 10,
        .foot_no = 10,
        .srund_no = 10,
        .event_no = 10,
        .adpcm = {11, 11, 11, 2, 7},
    },
    {
        .rvol = 8191,
        .door_no = 11,
        .foot_no = 11,
        .srund_no = 11,
        .event_no = 11,
        .adpcm = {4, 4, 8, 2, 7},
    },
    {
        .rvol = 8191,
        .door_no = 12,
        .foot_no = 12,
        .srund_no = 12,
        .event_no = 12,
        .adpcm = {5, 5, 8, 2, 7},
    },
    {
        .rvol = 12287,
        .door_no = 13,
        .foot_no = 13,
        .srund_no = 13,
        .event_no = 13,
        .adpcm = {5, 5, 4, 2, 7},
    },
    {
        .rvol = 24575,
        .door_no = 14,
        .foot_no = 14,
        .srund_no = 14,
        .event_no = 14,
        .adpcm = {10, 10, 10, 10, 7},
    },
    {
        .rvol = 8191,
        .door_no = 15,
        .foot_no = 15,
        .srund_no = 15,
        .event_no = 15,
        .adpcm = {23, 23, 23, 8, 7},
    },
    {
        .rvol = 8191,
        .door_no = 16,
        .foot_no = 16,
        .srund_no = 16,
        .event_no = 16,
        .adpcm = {17, 17, 11, 22, 7},
    },
    {
        .rvol = 16383,
        .door_no = 17,
        .foot_no = 17,
        .srund_no = 17,
        .event_no = 17,
        .adpcm = {5, 3, 11, 2, 7},
    },
    {
        .rvol = 8191,
        .door_no = 18,
        .foot_no = 18,
        .srund_no = 18,
        .event_no = 18,
        .adpcm = {23, 23, 23, 23, 23},
    },
    {
        .rvol = 12287,
        .door_no = 19,
        .foot_no = 19,
        .srund_no = 19,
        .event_no = 19,
        .adpcm = {9, 9, 9, 9, 7},
    },
    {
        .rvol = 8191,
        .door_no = 20,
        .foot_no = 20,
        .srund_no = 20,
        .event_no = 20,
        .adpcm = {8, 8, 8, 8, 8},
    },
    {
        .rvol = 8191,
        .door_no = 21,
        .foot_no = 21,
        .srund_no = 21,
        .event_no = 21,
        .adpcm = {20, 20, 11, 22, 7},
    },
    {
        .rvol = 8191,
        .door_no = 22,
        .foot_no = 22,
        .srund_no = 22,
        .event_no = 22,
        .adpcm = {18, 18, 18, 22, 7},
    },
    {
        .rvol = 28671,
        .door_no = 23,
        .foot_no = 23,
        .srund_no = 23,
        .event_no = 23,
        .adpcm = {15, 15, 11, 15, 15},
    },
    {
        .rvol = 8191,
        .door_no = 24,
        .foot_no = 24,
        .srund_no = 24,
        .event_no = 24,
        .adpcm = {16, 16, 16, 22, 16},
    },
    {
        .rvol = 8191,
        .door_no = 25,
        .foot_no = 25,
        .srund_no = 25,
        .event_no = 25,
        .adpcm = {19, 19, 19, 22, 19},
    },
    {
        .rvol = 16383,
        .door_no = 26,
        .foot_no = 26,
        .srund_no = 26,
        .event_no = 26,
        .adpcm = {4, 7, 23, 23, 10},
    },
    {
        .rvol = 20479,
        .door_no = 27,
        .foot_no = 27,
        .srund_no = 27,
        .event_no = 27,
        .adpcm = {6, 6, 6, 2, 7},
    },
    {
        .rvol = 28671,
        .door_no = 28,
        .foot_no = 28,
        .srund_no = 28,
        .event_no = 28,
        .adpcm = {6, 6, 6, 6, 7},
    },
    {
        .rvol = 24575,
        .door_no = 29,
        .foot_no = 29,
        .srund_no = 29,
        .event_no = 29,
        .adpcm = {7, 7, 7, 7, 7},
    },
    {
        .rvol = 24575,
        .door_no = 30,
        .foot_no = 30,
        .srund_no = 30,
        .event_no = 30,
        .adpcm = {7, 7, 7, 7, 6},
    },
    {
        .rvol = 28671,
        .door_no = 31,
        .foot_no = 31,
        .srund_no = 31,
        .event_no = 31,
        .adpcm = {23, 23, 23, 7, 8},
    },
    {
        .rvol = 12287,
        .door_no = 32,
        .foot_no = 32,
        .srund_no = 32,
        .event_no = 32,
        .adpcm = {1, 1, 10, 10, 10},
    },
    {
        .rvol = 20479,
        .door_no = 33,
        .foot_no = 33,
        .srund_no = 33,
        .event_no = 33,
        .adpcm = {1, 1, 1, 1, 6},
    },
    {
        .rvol = 28671,
        .door_no = 34,
        .foot_no = 34,
        .srund_no = 34,
        .event_no = 34,
        .adpcm = {21, 21, 21, 21, 21},
    },
    {
        .rvol = 8191,
        .door_no = 35,
        .foot_no = 35,
        .srund_no = 35,
        .event_no = 35,
        .adpcm = {9, 9, 9, 2, 9},
    },
    {
        .rvol = 12287,
        .door_no = 36,
        .foot_no = 36,
        .srund_no = 36,
        .event_no = 36,
        .adpcm = {1, 1, 10, 10, 7},
    },
    {
        .rvol = 8191,
        .door_no = 0,
        .foot_no = 0,
        .srund_no = 0,
        .event_no = 0,
        .adpcm = {23, 23, 23, 23, 23},
    },
    {
        .rvol = 8191,
        .door_no = 38,
        .foot_no = 38,
        .srund_no = 38,
        .event_no = 38,
        .adpcm = {5, 3, 1, 1, 7},
    },
    {
        .rvol = 8191,
        .door_no = 0,
        .foot_no = 0,
        .srund_no = 0,
        .event_no = 0,
        .adpcm = {23, 23, 23, 23, 23},
    },
    {
        .rvol = 8191,
        .door_no = 40,
        .foot_no = 40,
        .srund_no = 40,
        .event_no = 40,
        .adpcm = {11, 11, 11, 23, 7},
    },
    {
        .rvol = 8191,
        .door_no = 41,
        .foot_no = 41,
        .srund_no = 41,
        .event_no = 41,
        .adpcm = {23, 23, 23, 23, 23},
    },
    {
        .rvol = 8191,
        .door_no = 0,
        .foot_no = 0,
        .srund_no = 0,
        .event_no = 0,
        .adpcm = {23, 5, 5, 5, 5},
    },
    {
        .rvol = 8191,
        .door_no = 0,
        .foot_no = 0,
        .srund_no = 0,
        .event_no = 0,
        .adpcm = {23, 23, 23, 23, 23},
    },
    {
        .rvol = 8191,
        .door_no = 0,
        .foot_no = 0,
        .srund_no = 0,
        .event_no = 0,
        .adpcm = {23, 23, 23, 23, 23},
    },
};

static ROOM_DOOR_SE rm_door[] = {
    {
        .fno = {1472, 1474, 4294967295},
    },
    {
        .fno = {1478, 1472, 4294967295},
    },
    {
        .fno = {1478, 1472, 4294967295},
    },
    {
        .fno = {1478, 4294967295, 4294967295},
    },
    {
        .fno = {1474, 4294967295, 4294967295},
    },
    {
        .fno = {1474, 4294967295, 4294967295},
    },
    {
        .fno = {1474, 1478, 1477},
    },
    {
        .fno = {1478, 4294967295, 4294967295},
    },
    {
        .fno = {1478, 1472, 1477},
    },
    {
        .fno = {1478, 4294967295, 4294967295},
    },
    {
        .fno = {1478, 1474, 4294967295},
    },
    {
        .fno = {1474, 1472, 4294967295},
    },
    {
        .fno = {1478, 1472, 4294967295},
    },
    {
        .fno = {1478, 1472, 4294967295},
    },
    {
        .fno = {1478, 1477, 4294967295},
    },
    {
        .fno = {1478, 1477, 4294967295},
    },
    {
        .fno = {1478, 1477, 1476},
    },
    {
        .fno = {1478, 1476, 4294967295},
    },
    {
        .fno = {1478, 1476, 4294967295},
    },
    {
        .fno = {1477, 4294967295, 4294967295},
    },
    {
        .fno = {1476, 4294967295, 4294967295},
    },
    {
        .fno = {1478, 1475, 4294967295},
    },
    {
        .fno = {1476, 4294967295, 4294967295},
    },
    {
        .fno = {1478, 4294967295, 4294967295},
    },
    {
        .fno = {1472, 4294967295, 4294967295},
    },
    {
        .fno = {1477, 1475, 4294967295},
    },
    {
        .fno = {1477, 4294967295, 4294967295},
    },
    {
        .fno = {1477, 4294967295, 4294967295},
    },
    {
        .fno = {1473, 4294967295, 4294967295},
    },
    {
        .fno = {1477, 1473, 4294967295},
    },
    {
        .fno = {1473, 4294967295, 4294967295},
    },
    {
        .fno = {1473, 4294967295, 4294967295},
    },
    {
        .fno = {1473, 4294967295, 4294967295},
    },
    {
        .fno = {1473, 4294967295, 4294967295},
    },
    {
        .fno = {1473, 4294967295, 4294967295},
    },
    {
        .fno = {1472, 1476, 4294967295},
    },
    {
        .fno = {1474, 4294967295, 4294967295},
    },
    {
        .fno = {4294967295, 4294967295, 4294967295},
    },
    {
        .fno = {1478, 1472, 4294967295},
    },
    {
        .fno = {4294967295, 4294967295, 4294967295},
    },
    {
        .fno = {1478, 1474, 4294967295},
    },
    {
        .fno = {1478, 1472, 1476},
    },
    {
        .fno = {1478, 1474, 4294967295},
    },
};

static ROOM_FOOT_SE rm_foot[] = {
    {
        .fno = {1452, 1455, 1449, 1450, 1444},
    },
    {
        .fno = {1452, 1455, 1461, 1454, 4294967295},
    },
    {
        .fno = {1452, 1455, 4294967295, 4294967295, 4294967295},
    },
    {
        .fno = {1452, 1455, 1461, 1450, 1454},
    },
    {
        .fno = {1452, 1455, 4294967295, 4294967295, 4294967295},
    },
    {
        .fno = {1452, 1455, 1450, 1448, 4294967295},
    },
    {
        .fno = {1452, 1455, 1447, 1448, 1464},
    },
    {
        .fno = {1452, 1455, 1461, 1454, 4294967295},
    },
    {
        .fno = {1452, 1455, 1450, 4294967295, 4294967295},
    },
    {
        .fno = {1452, 1455, 1446, 1468, 4294967295},
    },
    {
        .fno = {1452, 1455, 1461, 1454, 1449},
    },
    {
        .fno = {1452, 1455, 1461, 1454, 4294967295},
    },
    {
        .fno = {1452, 1455, 1461, 1454, 4294967295},
    },
    {
        .fno = {1452, 1455, 4294967295, 4294967295, 4294967295},
    },
    {
        .fno = {1452, 1455, 1462, 4294967295, 4294967295},
    },
    {
        .fno = {1448, 4294967295, 4294967295, 4294967295, 4294967295},
    },
    {
        .fno = {1452, 1455, 1448, 1465, 1450},
    },
    {
        .fno = {1452, 1455, 1449, 1450, 4294967295},
    },
    {
        .fno = {1452, 1455, 1461, 1454, 4294967295},
    },
    {
        .fno = {1452, 1455, 1450, 1449, 4294967295},
    },
    {
        .fno = {1452, 1455, 1450, 1470, 1471},
    },
    {
        .fno = {1466, 1467, 1448, 1447, 4294967295},
    },
    {
        .fno = {1467, 1451, 1465, 4294967295, 4294967295},
    },
    {
        .fno = {1448, 1469, 4294967295, 4294967295, 4294967295},
    },
    {
        .fno = {1452, 1455, 1463, 4294967295, 4294967295},
    },
    {
        .fno = {1450, 1460, 1464, 1452, 4294967295},
    },
    {
        .fno = {1452, 1455, 1449, 4294967295, 4294967295},
    },
    {
        .fno = {1452, 1455, 1449, 4294967295, 4294967295},
    },
    {
        .fno = {1447, 1448, 1464, 4294967295, 4294967295},
    },
    {
        .fno = {1448, 1459, 1452, 1455, 4294967295},
    },
    {
        .fno = {1447, 1448, 1464, 4294967295, 4294967295},
    },
    {
        .fno = {1448, 1458, 4294967295, 4294967295, 4294967295},
    },
    {
        .fno = {1448, 1451, 1445, 4294967295, 4294967295},
    },
    {
        .fno = {1448, 1464, 4294967295, 4294967295, 4294967295},
    },
    {
        .fno = {1448, 1464, 4294967295, 4294967295, 4294967295},
    },
    {
        .fno = {1452, 1455, 1461, 1454, 1453},
    },
    {
        .fno = {1452, 1455, 1450, 4294967295, 4294967295},
    },
    {
        .fno = {4294967295, 4294967295, 4294967295, 4294967295, 4294967295},
    },
    {
        .fno = {1452, 1455, 1461, 1454, 4294967295},
    },
    {
        .fno = {4294967295, 4294967295, 4294967295, 4294967295, 4294967295},
    },
    {
        .fno = {1452, 1455, 1461, 1454, 4294967295},
    },
    {
        .fno = {1452, 1455, 1461, 1454, 4294967295},
    },
};

static u_int rm_srund[] = { 1494, 4294967295, 1495, 1496, 1497, 4294967295, 4294967295, 1498, 4294967295, 1499, 1500, 1501, 1502, 4294967295, 1503, 1504, 1505, 4294967295, 4294967295, 4294967295, 1506, 1507, 1508, 4294967295, 4294967295, 4294967295, 4294967295, 4294967295, 1509, 1509, 1511, 4294967295, 1509, 4294967295, 1509, 4294967295, 4294967295, 4294967295, 4294967295, 4294967295, 1500, 1512 };

static u_int rm_event[] = { 1399, 1400, 1401, 1402, 4294967295, 1403, 1404, 1405, 1406, 1407, 1408, 1409, 1410, 4294967295, 1411, 1412, 1421, 1414, 1415, 4294967295, 4294967295, 1415, 1416, 1417, 1418, 1419, 1420, 1421, 4294967295, 4294967295, 4294967295, 4294967295, 4294967295, 4294967295, 4294967295, 1422, 4294967295, 4294967295, 1423, 4294967295, 1408, 4294967295 };
ROOM_SOUND_INFO* GetSdrDatP(u_char room_id)
{
  return &rm_snd_info[room_id];
}

int GetSdrAdpcmFno(u_char room_id)
{
    return adpcm_param_tbl[rm_snd_info[room_id].adpcm[ingame_wrk.msn_no]].file_no;
}

u_short GetSdrAdpcmVol(u_char room_id)
{
    return adpcm_param_tbl[rm_snd_info[room_id].adpcm[ingame_wrk.msn_no]].vol;
}

u_short GetSdrReverbVol(u_char room_id)
{
    return rm_snd_info[room_id].rvol;
}

void SetRoomReverbVol(u_char room_id)
{
    u_short vol;
  
    if (room_id >= RM_DOOR_SIZE) 
    {
        vol = MAX_VOL;
        
    }
    else 
    {
        vol = GetSdrReverbVol(room_id);
    }
    
    SetReverbVolume(vol);
}

u_int * GetSdrDoorSeTblP(u_char room_id)
{
    u_char door_no;
  
    door_no = rm_snd_info[room_id].door_no;

    if (ingame_wrk.msn_no != 1 && room_id == 13)
    {
        door_no = RM_DOOR_SIZE;
    }
    
    return rm_door[door_no].fno;
}

u_int * GetSdrFootSeTblP(u_char room_id)
{
    return rm_foot[rm_snd_info[room_id].foot_no].fno;
}

u_int GetSdrSrundSe(u_char room_id)
{
    return rm_srund[rm_snd_info[room_id].srund_no];
}

u_int GetSdrEventSe(u_char room_id)
{
    return rm_event[rm_snd_info[room_id].event_no];
}
