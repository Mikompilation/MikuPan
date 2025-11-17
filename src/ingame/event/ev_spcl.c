#include "common.h"
#include "ev_spcl.h"
#include "typedefs.h"


void SpecialEventInit(u_char spev_no)
{
}

void SpecialEventMain()
{
}

int GetSpecialEventMessageAddr(short int msg_no)
{
}

void SimpleDispSprt(SPRT_SDAT* ssd, u_int addr, int sp_no, SPRT_SROT* srot, SPRT_SSCL* sscl, u_char alp_rate)
{
    /* 0x0(sp) */ DISP_SPRT ds;
	/* 0x90(sp) */ SPRT_DAT sd;

    sd.tex0 = GetTex0Reg(addr, sp_no, 0);
    
    sd.u = ssd->u;
    sd.v = ssd->v;
    
    sd.w = ssd->w;
    sd.h = ssd->h;
    
    sd.x = ssd->x;
    sd.y = ssd->y;
    
    sd.pri = ssd->pri << 12;
    
    if (alp_rate == 0xFF) 
    {
        ds.alphar = 72;
    }
    else 
    {
        sd.alpha = (ssd->alp * alp_rate) / 100;
    }
    
    CopySprDToSpr(&ds, &sd);
    
    if (srot != NULL)
    {
        if (srot->cx != 0x7FFF)
        {
            ds.rot = srot->rot;
            
            ds.crx = srot->cx;
            ds.cry = srot->cy;
        }
        else 
        {
            ds.att |= srot->cy;
        }
    }
    
    if (sscl != NULL) 
    {
        ds.scw = sscl->sw / 100.0f;
        ds.sch = sscl->sh / 100.0f;
        
        ds.csx = sscl->cx;
        ds.csy = sscl->cy;
    }
    
    DispSprD(&ds);
}

void SimpleDispAlphaSprt(SPRT_SDAT *ssd, int64_t addr, int sp_no, u_char alp_rate, u_char alp_type)
{
    /* 0x0(sp) */ DISP_SPRT ds;
	/* 0x90(sp) */ SPRT_DAT sd;

    sd.tex0 = GetTex0Reg(addr, sp_no, 0);
    
    sd.u = ssd->u;
    sd.v = ssd->v;
    
    sd.w = ssd->w;
    sd.h = ssd->h;
    
    sd.x = ssd->x;
    sd.y = ssd->y;
    
    sd.pri = ssd->pri << 12;
    
    if (alp_type != 0) 
    {
        ds.alphar = 72;
    }
    else 
    {
        ds.alphar = 68;
    }
    
    sd.alpha = (ssd->alp * alp_rate) / 100;
    
    CopySprDToSpr(&ds, &sd);
    
    DispSprD(&ds);
}

void SimpleDispSprtRGB(u_int addr, int sp_no, u_char alp_rate, u_char rr, u_char gg, int bb)
{
}

void SimpleDispSprtLNR(u_int addr, int sp_no, u_char alp_rate, int lnr)
{
}

void SimpleDispSprtDatCopy(SPRT_SDAT* org, SPRT_SDAT* cpy)
{
}

void TestPk2Data(long int sendtexaddr)
{
}

int ButtonMarkNext(int x_off, int y_off, int se_flg)
{
}

int ButtonMarkWait()
{
}

void ButtonMarkTimeClear()
{
}

int CsrInclease(u_char* csr_idx, u_char alpha_max, u_char inclease)
{
}

int CsrDeclease(u_char* csr_idx, u_char alpha_min, int inclease)
{
}

int CsrBlink(u_char* csr_idx, u_char alpha_max, u_char alpha_min, u_char inclease, u_char* blink)
{
}

void CsrClear(u_char* csr_idx)
{
}

void CsrClearAll()
{
}

void SpevStrInit()
{
}

void SpevWrkInit()
{
}

void SpevSelectYesNoCsr(float pos_x, float pos_y, int pri, float alp)
{
}

int DeadlySeStopWait()
{
}

void DummyProg()
{
}

void StarPuzzleInit(int pzl_no)
{
}

void StarPuzzleDataSet(int pzl_no)
{
}

int StarPuzzleMain(int pzl_no)
{
}

int StarPuzzleClearJudge()
{
}

void StarPuzzleDisp()
{
}

int StarPuzzleMSGMain(int pzl_no)
{
}

void StarPuzzleMSGDisp()
{
}

void DialKeyDoorInit(int door_no)
{
}

void DialKeyDoorDataSet(int door_no)
{
}

int DialKeyDoorMain()
{
}

void DialKeyDoorDisp()
{
}

void DialKeyMSGDoorInit()
{
}

int DialKeyMSGDoorMain(int msg_no)
{
}

void DialKeyMSGDoorDisp(int msg_no)
{
}

void GhostDoorInit(int door_no)
{
}

void GhostDoorSet(int door_no)
{
}

void GhostDoorMain(int door_no)
{
}

void GhostDoorDisp(int door_no)
{
}

void DollPzlInit()
{
}

void DollPzlMain()
{
}

int CursorManagerEvent003()
{
}

void SpecialEventDisp003(int no)
{
}

void ButsuzoPzlInit()
{
}

void ButsuzoPzlMain()
{
}

u_char BldAlpRetern(short int time_cnt, short int bld_appear, short int bld_end, short int bld_alp)
{
}

void SpecialEventDisp004()
{
}

void ButsuzoMSGInit()
{
}

void ButsuzoMSGMain()
{
}

void ButsuzoMSGDisp()
{
}

void LightsOutInit()
{
}

void LightsOutMain()
{
}

void SpecialEventDisp014(int no)
{
}

void FaceDoorInit(int face_no)
{
}

void FaceDoorMain(int face_no)
{
}

void FaceDoorOkSet(int face_no)
{
}

void FaceDoorAimSet(int face_no)
{
}

int NisUseCheck(int face_no)
{
}

void NisUseSet(int face_no)
{
}

void NisUseUnSet(int face_no)
{
}

void FaceDoorDisp(int face_no)
{
}

void SurpriseDoorInit()
{
}

void SurpriseDoorMain()
{
}

void SurpriseDoorDisp(int face_no)
{
}

void SimenPillarInit(int event_no)
{
}

void SimenPillarMain(int event_no)
{
}

int SimenCheck()
{
}

void SimenPillarDisp()
{
}

void IkariMenInit()
{
}

void IkariMenComeOn()
{
}

void HanyouKaitenInit(int event_no)
{
}

void HanyouKaitenMain(int event_no)
{
}

void ZushiBonjiInit(int bonji_no)
{
}

void ZushiBonjiMain(int bonji_no)
{
}

void ZushiBonjiDisp(int bonji_no)
{
}

void ZushiBonjiMSGInit()
{
}

void ZushiBonjiMSGMain()
{
}

void ZushiBonjiMSGDisp()
{
}

void ZushiBonjiAfterInit(int bonji_no)
{
}

void ZushiBonjiAfterMain(int bonji_no)
{
}

void ZushiBonjiAfterDisp(int bonji_no)
{
}

void KakejikuDoorInit()
{
}

void KakejikuDoorMain()
{
}

void IdoFirstIntoInit()
{
}

void IdoFirstIntoMain()
{
}

void IdoIntoInit()
{
}

void IdoIntoMain()
{
}

void IdoFirstOutInit()
{
}

void IdoFirstOutMain()
{
}

void IdoOutInit()
{
}

void IdoOutMain()
{
}

void IdoInOutDisp(int inout)
{
}

void ItemEventInit(int event_no)
{
}

void ItemEventMain(int event_no)
{
}

void ItemEventDisp()
{
}

void NawakakeFalseMain(int event_no)
{
}

void NawakakeFalseDisp()
{
}

void SpecialEventInit000()
{
}

void SpecialEventMain000()
{
}

void SpecialEventInit001()
{
}

void SpecialEventMain001()
{
}

void SpecialEventInit002()
{
}

void SpecialEventMain002()
{
}

void SpecialEventInit003()
{
}

void SpecialEventMain003()
{
}

void SpecialEventInit004()
{
}

void SpecialEventMain004()
{
}

void SpecialEventInit005()
{
}

void SpecialEventMain005()
{
}

void SpecialEventInit006()
{
}

void SpecialEventMain006()
{
}

void SpecialEventInit007()
{
}

void SpecialEventMain007()
{
}

void SpecialEventInit008()
{
}

void SpecialEventMain008()
{
}

void SpecialEventInit009()
{
}

void SpecialEventMain009()
{
}

void SpecialEventInit010()
{
}

void SpecialEventMain010()
{
}

void SpecialEventInit011()
{
}

void SpecialEventMain011()
{
}

void SpecialEventInit012()
{
}

void SpecialEventMain012()
{
}

void SpecialEventInit013()
{
}

void SpecialEventMain013()
{
}

void SpecialEventInit014()
{
}

void SpecialEventMain014()
{
}

void SpecialEventInit015()
{
}

void SpecialEventMain015()
{
}

void SpecialEventInit016()
{
}

void SpecialEventMain016()
{
}

void SpecialEventInit017()
{
}

void SpecialEventMain017()
{
}

void SpecialEventInit018()
{
}

void SpecialEventMain018()
{
}

void SpecialEventInit019()
{
}

void SpecialEventMain019()
{
}

void SpecialEventInit020()
{
}

void SpecialEventMain020()
{
}

void SpecialEventInit021()
{
}

void SpecialEventMain021()
{
}

void SpecialEventInit022()
{
}

void SpecialEventMain022()
{
}

void SpecialEventInit023()
{
}

void SpecialEventMain023()
{
}

void SpecialEventInit024()
{
}

void SpecialEventMain024()
{
}

void SpecialEventInit025()
{
}

void SpecialEventMain025()
{
}

void SpecialEventInit026()
{
}

void SpecialEventMain026()
{
}

void SpecialEventInit027()
{
}

void SpecialEventMain027()
{
}

void SpecialEventInit028()
{
}

void SpecialEventMain028()
{
}

void SpecialEventInit029()
{
}

void SpecialEventMain029()
{
}

void SpecialEventInit030()
{
}

void SpecialEventMain030()
{
}

void SpecialEventInit031()
{
}

void SpecialEventMain031()
{
}

void SpecialEventInit032()
{
}

void SpecialEventMain032()
{
}

void SpecialEventInit033()
{
}

void SpecialEventMain033()
{
}

void SpecialEventInit034()
{
}

void SpecialEventMain034()
{
}

void SpecialEventInit035()
{
}

void SpecialEventMain035()
{
}

void SpecialEventInit036()
{
}

void SpecialEventMain036()
{
}

void SpecialEventInit037()
{
}

void SpecialEventMain037()
{
}

void SpecialEventInit038()
{
}

void SpecialEventMain038()
{
}

void SpecialEventInit039()
{
}

void SpecialEventMain039()
{
}

void SpecialEventInit040()
{
}

void SpecialEventMain040()
{
}

void SpecialEventInit041()
{
}

void SpecialEventMain041()
{
}

void SpecialEventInit042()
{
}

void SpecialEventMain042()
{
}

void SpecialEventInit043()
{
}

void SpecialEventMain043()
{
}

void SpecialEventInit044()
{
}

void SpecialEventMain044()
{
}

void SpecialEventInit045()
{
}

void SpecialEventMain045()
{
}

void SpecialEventInit046()
{
}

void SpecialEventMain046()
{
}

void SpecialEventInit047()
{
}

void SpecialEventMain047()
{
}

void SpecialEventInit048()
{
}

void SpecialEventMain048()
{
}

void SpecialEventInit049()
{
}

void SpecialEventMain049()
{
}

void SpecialEventInit050()
{
}

void SpecialEventMain050()
{
}

void SpecialEventInit051()
{
}

void SpecialEventMain051()
{
}

void SpecialEventInit052()
{
}

void SpecialEventMain052()
{
}

void SpecialEventInit053()
{
}

void SpecialEventMain053()
{
}

void SpecialEventInit054()
{
}

void SpecialEventMain054()
{
}

void SpecialEventInit055()
{
}

void SpecialEventMain055()
{
}

void SpecialEventInit056()
{
}

void SpecialEventMain056()
{
}

void SpecialEventInit057()
{
}

void SpecialEventMain057()
{
}

void SpecialEventInit058()
{
}

void SpecialEventMain058()
{
}

void SpecialEventInit059()
{
}

void SpecialEventMain059()
{
}

void SpecialEventInit060()
{
}

void SpecialEventMain060()
{
}

void SpecialEventInit061()
{
}

void SpecialEventMain061()
{
}

void SpecialEventInit062()
{
}

void SpecialEventMain062()
{
}

void SpecialEventInit063()
{
}

void SpecialEventMain063()
{
}

void SpecialEventInit064()
{
}

void SpecialEventMain064()
{
}

void SpecialEventInit065()
{
}

void SpecialEventMain065()
{
}

void SpecialEventInit066()
{
}

void SpecialEventMain066()
{
}

void SpecialEventInit067()
{
}

void SpecialEventMain067()
{
}

void SpecialEventInit068()
{
}

void SpecialEventMain068()
{
}

void SpecialEventInit069()
{
}

void SpecialEventMain069()
{
}

void SpecialEventInit070()
{
}

void SpecialEventMain070()
{
}

void SpecialEventInit071()
{
}

void SpecialEventMain071()
{
}

void SpecialEventInit072()
{
}

void SpecialEventMain072()
{
}

void SpecialEventInit073()
{
}

void SpecialEventMain073()
{
}

void SpecialEventInit074()
{
}

void SpecialEventMain074()
{
}

void SpecialEventInit075()
{
}

void SpecialEventMain075()
{
}

void SpecialEventInit076()
{
}

void SpecialEventMain076()
{
}

void SpecialEventInit077()
{
}

void SpecialEventMain077()
{
}

void SpecialEventInit078()
{
}

void SpecialEventMain078()
{
}

void SpecialEventInit079()
{
}

void SpecialEventMain079()
{
}

void SpecialEventInit080()
{
}

void SpecialEventMain080()
{
}

void SpecialEventInit081()
{
}

void SpecialEventMain081()
{
}

void SpecialEventInit082()
{
}

void SpecialEventMain082()
{
}

void SpecialEventInit083()
{
}

void SpecialEventMain083()
{
}

void SpecialEventInit084()
{
}

void SpecialEventMain084()
{
}

void SpecialEventInit085()
{
}

void SpecialEventMain085()
{
}

void SpecialEventInit086()
{
}

void SpecialEventMain086()
{
}

void SpecialEventInit087()
{
}

void SpecialEventMain087()
{
}

void SpecialEventInit088()
{
}

void SpecialEventMain088()
{
}

void SpecialEventInit089()
{
}

void SpecialEventMain089()
{
}

void SpecialEventInit090()
{
}

void SpecialEventMain090()
{
}
