#include "rgst_dat.h"

RGOST_AP_DAT rg_ap_dat[] = {
    {
     .room = 2,
     .ap_step0 = 236,
     .ap_step1 = {0, 0, 0, 0},
     .ap_step2 = {0, 0, 10, 0, 0, 0},
     .ap_near = 3000,
     .ap_far = 30000,
     .ap_stts = 32768,
     },
    {
     .room = 0,
     .ap_step0 = 236,
     .ap_step1 = {0, 0, 0, 0},
     .ap_step2 = {0, 0, 99, 0, 0, 0},
     .ap_near = 1700,
     .ap_far = 30000,
     .ap_stts = 32768,
     },
    {
     .room = 3,
     .ap_step0 = 236,
     .ap_step1 = {0, 0, 0, 0},
     .ap_step2 = {10, 0, 99, 0, 0, 0},
     .ap_near = 1000,
     .ap_far = 20000,
     .ap_stts = 32768,
     },
    {
     .room = 0,
     .ap_step0 = 236,
     .ap_step1 = {0, 0, 0, 0},
     .ap_step2 = {0, 0, 99, 0, 0, 0},
     .ap_near = 1700,
     .ap_far = 30000,
     .ap_stts = 32768,
     },
    {
     .room = 3,
     .ap_step0 = 236,
     .ap_step1 = {0, 0, 0, 0},
     .ap_step2 = {10, 0, 50, 0, 0, 0},
     .ap_near = 1000,
     .ap_far = 20000,
     .ap_stts = 8192,
     },
};
RGOST_DAT rg_dat[] = {
    {
     .score = 1000,
     .center_no = 1,
     .center_num = 2,
     .start = 0,
     .inter = 0,
     .end = 0,
     .dummy = 255,
     .pos = {500, -900, 500},
     },
    {
     .score = 500,
     .center_no = 1,
     .center_num = 2,
     .start = 0,
     .inter = 0,
     .end = 0,
     .dummy = 255,
     .pos = {500, -500, 500},
     },
    {
     .score = 700,
     .center_no = 1,
     .center_num = 2,
     .start = 0,
     .inter = 0,
     .end = 0,
     .dummy = 255,
     .pos = {3000, -2100, 500},
     },
    {
     .score = 500,
     .center_no = 1,
     .center_num = 2,
     .start = 0,
     .inter = 0,
     .end = 0,
     .dummy = 255,
     .pos = {3500, -1100, 3500},
     },
};
sceVu0FVECTOR rg_center00[] = {0.0f, 0.0f, 0.0f, 1.0f};
sceVu0FVECTOR rg_center01[] = {
    {0.0f, 0.0f, 0.0f, 0.699999988079071f},
    {0.0f, -100.0f, 0.0f, 0.30000001192092896f},
};
sceVu0FVECTOR *rgc_dat[] = {
    (sceVu0FVECTOR *)rg_center00,
    (sceVu0FVECTOR *)rg_center01,
};

RG_DISP_DAT rg_start_dat[] = {
    {
        .flame = 300,
        .alp_no = 1,
        .ani_no = 0,
        .efct_no = 0,
        .rate_no = 0,
    },
    {
        .flame = 10,
        .alp_no = 0,
        .ani_no = 0,
        .efct_no = 0,
        .rate_no = 0,
    },
};

RG_DISP_DAT rg_inter_dat[] = {
    {
        .flame = 400,
        .alp_no = 0,
        .ani_no = 0,
        .efct_no = 0,
        .rate_no = 0,
    },
    {
        .flame = 100,
        .alp_no = 0,
        .ani_no = 0,
        .efct_no = 0,
        .rate_no = 0,
    },
};

RG_DISP_DAT rg_end_dat[] = {
    {
        .flame = 100,
        .alp_no = 1,
        .ani_no = 0,
        .efct_no = 0,
        .rate_no = 0,
    },
};

RG_ALP_DAT rg_alp_start00[] = {
    {
        .flame = 65535,
        .alp = 100,
        .stts = 0,
    },
    {
        .flame = 0,
        .alp = 0,
        .stts = 0,
    },
};

RG_ALP_DAT rg_alp_start01[] = {
    {
        .flame = 0,
        .alp = 0,
        .stts = 0,
    },
    {
        .flame = 100,
        .alp = 0,
        .stts = 0,
    },
    {
        .flame = 300,
        .alp = 100,
        .stts = 1,
    },
    {
        .flame = 900,
        .alp = 100,
        .stts = 0,
    },
    {
        .flame = 1000,
        .alp = 0,
        .stts = 2,
    },
    {
        .flame = 65535,
        .alp = 0,
        .stts = 0,
    },
};

RG_ALP_DAT *rg_alp_start[] = {
    rg_alp_start00,
    rg_alp_start01,
};

RG_ALP_DAT rg_alp_inter00[] = {
    {
        .flame = 65535,
        .alp = 100,
        .stts = 0,
    },
};

RG_ALP_DAT rg_alp_inter01[] = {
    {
        .flame = 0,
        .alp = 0,
        .stts = 0,
    },
    {
        .flame = 50,
        .alp = 100,
        .stts = 1,
    },
    {
        .flame = 150,
        .alp = 100,
        .stts = 0,
    },
    {
        .flame = 200,
        .alp = 0,
        .stts = 2,
    },
};

RG_ALP_DAT *rg_alp_inter[] = {
    rg_alp_inter00,
    rg_alp_inter01,
};

RG_ALP_DAT rg_alp_end00[] = {
    {
        .flame = 65535,
        .alp = 100,
        .stts = 0,
    },
};

RG_ALP_DAT rg_alp_end01[] = {
    {
        .flame = 100,
        .alp = 0,
        .stts = 2,
    },
};

RG_ALP_DAT *rg_alp_end[] = {
    rg_alp_end00,
    rg_alp_end01,
};

RG_ANI_DAT rg_ani_start00[] = {
    {
        .flame = 10,
        .tim_no = 0,
    },
    {
        .flame = 20,
        .tim_no = 1,
    },
    {
        .flame = 21,
        .tim_no = 254,
    },
};

RG_ANI_DAT rg_ani_start01[] = {
    {
        .flame = 10,
        .tim_no = 0,
    },
    {
        .flame = 20,
        .tim_no = 1,
    },
    {
        .flame = 21,
        .tim_no = 255,
    },
};

RG_ANI_DAT *rg_ani_start[] = {
    rg_ani_start00,
    rg_ani_start01,
};

RG_ANI_DAT rg_ani_inter00[] = {
    {
        .flame = 10,
        .tim_no = 0,
    },
    {
        .flame = 20,
        .tim_no = 1,
    },
    {
        .flame = 21,
        .tim_no = 254,
    },
};

RG_ANI_DAT rg_ani_inter01[] = {
    {
        .flame = 10,
        .tim_no = 0,
    },
    {
        .flame = 20,
        .tim_no = 1,
    },
    {
        .flame = 21,
        .tim_no = 255,
    },
};

RG_ANI_DAT *rg_ani_inter[] = {
    rg_ani_inter00,
    rg_ani_inter01,
};

RG_ANI_DAT rg_ani_end00[] = {
    {
        .flame = 10,
        .tim_no = 0,
    },
    {
        .flame = 20,
        .tim_no = 1,
    },
    {
        .flame = 21,
        .tim_no = 254,
    },
};

RG_ANI_DAT rg_ani_end01[] = {
    {
        .flame = 10,
        .tim_no = 0,
    },
    {
        .flame = 20,
        .tim_no = 1,
    },
    {
        .flame = 21,
        .tim_no = 255,
    },
};

RG_ANI_DAT *rg_ani_end[] = {
    rg_ani_end00,
    rg_ani_end01,
};

RG_RATE_DAT rg_rate_start00[] = {
    {
        .flame = 65535,
        .rate = 100,
        .stts = 0,
    },
};

RG_RATE_DAT rg_rate_start01[] = {
    {
        .flame = 0,
        .rate = 0,
        .stts = 0,
    },
    {
        .flame = 50,
        .rate = 100,
        .stts = 1,
    },
    {
        .flame = 150,
        .rate = 100,
        .stts = 0,
    },
    {
        .flame = 200,
        .rate = 0,
        .stts = 2,
    },
};

RG_RATE_DAT *rg_rate_start[] = {
    rg_rate_start00,
    rg_rate_start01,
};

RG_RATE_DAT rg_rate_inter00[] = {
    {
        .flame = 65535,
        .rate = 100,
        .stts = 0,
    },
};

RG_RATE_DAT rg_rate_inter01[] = {
    {
        .flame = 0,
        .rate = 0,
        .stts = 0,
    },
    {
        .flame = 50,
        .rate = 100,
        .stts = 1,
    },
    {
        .flame = 150,
        .rate = 100,
        .stts = 0,
    },
    {
        .flame = 200,
        .rate = 0,
        .stts = 2,
    },
};

RG_RATE_DAT *rg_rate_inter[] = {
    rg_rate_inter00,
    rg_rate_inter01,
};

RG_RATE_DAT rg_rate_end00[] = {
    {
        .flame = 65535,
        .rate = 100,
        .stts = 0,
    },
};

RG_RATE_DAT rg_rate_end01[] = {
    {
        .flame = 0,
        .rate = 0,
        .stts = 0,
    },
    {
        .flame = 50,
        .rate = 100,
        .stts = 1,
    },
    {
        .flame = 150,
        .rate = 100,
        .stts = 0,
    },
    {
        .flame = 200,
        .rate = 0,
        .stts = 2,
    },
};

RG_RATE_DAT *rg_rate_end[] = {
    rg_rate_end00,
    rg_rate_end01,
};