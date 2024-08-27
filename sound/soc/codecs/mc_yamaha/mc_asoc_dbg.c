/*
 * MC ASoC codec driver
 *
 * Copyright (c) 2011 Yamaha Corporation
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "mc_asoc_priv.h"

/* QNET Audio:2012-01-17 Log start */
//#ifdef CONFIG_SND_SOC_MC_YAMAHA_DEBUG

static void mc_asoc_dump_init_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_INIT_INFO *info = (MCDRV_INIT_INFO *)pvPrm;

	pr_info("bCkSel      = 0x%02x\n", info->bCkSel);
	pr_info("bDivR0      = 0x%02x\n", info->bDivR0);
	pr_info("bDivF0      = 0x%02x\n", info->bDivF0);
	pr_info("bDivR1      = 0x%02x\n", info->bDivR1);
	pr_info("bDivF1      = 0x%02x\n", info->bDivF1);
	pr_info("bRange0     = 0x%02x\n", info->bRange0);
	pr_info("bRange1     = 0x%02x\n", info->bRange1);
	pr_info("bBypass     = 0x%02x\n", info->bBypass);
	pr_info("bDioSdo0Hiz = 0x%02x\n", info->bDioSdo0Hiz);
	pr_info("bDioSdo1Hiz = 0x%02x\n", info->bDioSdo1Hiz);
	pr_info("bDioSdo2Hiz = 0x%02x\n", info->bDioSdo2Hiz);
	pr_info("bDioClk0Hiz = 0x%02x\n", info->bDioClk0Hiz);
	pr_info("bDioClk1Hiz = 0x%02x\n", info->bDioClk1Hiz);
	pr_info("bDioClk2Hiz = 0x%02x\n", info->bDioClk2Hiz);

	pr_info("bPcmHiz     = 0x%02x\n", info->bPcmHiz);
	pr_info("bLineIn1Dif = 0x%02x\n", info->bLineIn1Dif);
	pr_info("bLineIn2Dif = 0x%02x\n", info->bLineIn2Dif);
	pr_info("bLineOut1Dif= 0x%02x\n", info->bLineOut1Dif);
	pr_info("bLineOut2Dif= 0x%02x\n", info->bLineOut2Dif);
	pr_info("bSpmn       = 0x%02x\n", info->bSpmn);
	pr_info("bMic1Sng    = 0x%02x\n", info->bMic1Sng);
	pr_info("bMic2Sng    = 0x%02x\n", info->bMic2Sng);
	pr_info("bMic3Sng    = 0x%02x\n", info->bMic3Sng);
	pr_info("bPowerMode  = 0x%02x\n", info->bPowerMode);
	pr_info("bSpHiz      = 0x%02x\n", info->bSpHiz);
	pr_info("bLdo        = 0x%02x\n", info->bLdo);
	pr_info("bPad0Func   = 0x%02x\n", info->bPad0Func);
	pr_info("bPad1Func   = 0x%02x\n", info->bPad1Func);
	pr_info("bPad2Func   = 0x%02x\n", info->bPad2Func);
	pr_info("bAvddLev    = 0x%02x\n", info->bAvddLev);
	pr_info("bVrefLev    = 0x%02x\n", info->bVrefLev);
	pr_info("bDclGain    = 0x%02x\n", info->bDclGain);
	pr_info("bDclLimit   = 0x%02x\n", info->bDclLimit);
	pr_info("bCpMod      = 0x%02x\n", info->bCpMod);
	pr_info("bSdDs       = 0x%02x\n", info->bSdDs);

	pr_info("sWaitTime.dAdHpf   = 0x%08x\n", (unsigned int)info->sWaitTime.dAdHpf);
	pr_info("sWaitTime.dMic1Cin = 0x%08x\n", (unsigned int)info->sWaitTime.dMic1Cin);
	pr_info("sWaitTime.dMic2Cin = 0x%08x\n", (unsigned int)info->sWaitTime.dMic2Cin);
	pr_info("sWaitTime.dMic3Cin = 0x%08x\n", (unsigned int)info->sWaitTime.dMic3Cin);
	pr_info("sWaitTime.dLine1Cin= 0x%08x\n", (unsigned int)info->sWaitTime.dLine1Cin);
	pr_info("sWaitTime.dLine2Cin= 0x%08x\n", (unsigned int)info->sWaitTime.dLine2Cin);
	pr_info("sWaitTime.dVrefRdy1= 0x%08x\n", (unsigned int)info->sWaitTime.dVrefRdy1);
	pr_info("sWaitTime.dVrefRdy2= 0x%08x\n", (unsigned int)info->sWaitTime.dVrefRdy2);
	pr_info("sWaitTime.dHpRdy   = 0x%08x\n", (unsigned int)info->sWaitTime.dHpRdy);
	pr_info("sWaitTime.dSpRdy   = 0x%08x\n", (unsigned int)info->sWaitTime.dSpRdy);
	pr_info("sWaitTime.dPdm     = 0x%08x\n", (unsigned int)info->sWaitTime.dPdm);
	pr_info("sWaitTime.dAnaRdyInterval = 0x%08x\n", (unsigned int)info->sWaitTime.dAnaRdyInterval);
	pr_info("sWaitTime.dSvolInterval   = 0x%08x\n", (unsigned int)info->sWaitTime.dSvolInterval);
	pr_info("sWaitTime.dAnaRdyTimeOut  = 0x%08x\n", (unsigned int)info->sWaitTime.dAnaRdyTimeOut);
	pr_info("sWaitTime.dSvolTimeOut    = 0x%08x\n", (unsigned int)info->sWaitTime.dSvolTimeOut);
}

static void mc_asoc_dump_reg_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_REG_INFO *info = (MCDRV_REG_INFO *)pvPrm;

	pr_info("bRegType = 0x%02x\n", info->bRegType);
	pr_info("bAddress = 0x%02x\n", info->bAddress);
	pr_info("bData    = 0x%02x\n", info->bData);
}

static void mc_asoc_dump_array(const char *name,
			     const unsigned char *data, size_t len)
{
	char str[128], *p;
#if 1
	int n = len;
#else
	int n = (len <= 282) ? len : 282;
#endif
	int i;

	p = str;
	pr_info("%s[] = %d byte\n", name, n);
	for (i = 0; i < n; i++) {
		p += sprintf(p, "0x%02x ", data[i]);
#if 1
		if ((i % 16 == 15) || (i == n - 1)) {
#else
		if ((i % 8 == 7) || (i == n - 1)) {
#endif
			pr_info("%s\n", str);
			p = str;
		}
	}
}

/* ebabling symbolic IO device name LOGGING */
#if 0
#define DEF_PATH(p) {offsetof(MCDRV_PATH_INFO, p), #p}

static void mc_asoc_dump_path_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_PATH_INFO *info = (MCDRV_PATH_INFO *)pvPrm;
	int i;
	UINT8	mask	= (dPrm==0)?0xFF:dPrm;

	struct path_table {
		size_t offset;
		char *name;
	};

	struct path_table table[] = {
		DEF_PATH(asHpOut[0]), DEF_PATH(asHpOut[1]),
		DEF_PATH(asSpOut[0]), DEF_PATH(asSpOut[1]),
		DEF_PATH(asRcOut[0]),
		DEF_PATH(asLout1[0]), DEF_PATH(asLout1[1]),
		DEF_PATH(asLout2[0]), DEF_PATH(asLout2[1]),
//		DEF_PATH(asPeak[0]),
		DEF_PATH(asDit0[0]),
		DEF_PATH(asDit1[0]),
		DEF_PATH(asDit2[0]),
		DEF_PATH(asDac[0]), DEF_PATH(asDac[1]),
		DEF_PATH(asAe[0]),
		DEF_PATH(asCdsp[0]), DEF_PATH(asCdsp[1]),
		DEF_PATH(asCdsp[2]), DEF_PATH(asCdsp[3]),
		DEF_PATH(asAdc0[0]), DEF_PATH(asAdc0[1]),
		DEF_PATH(asAdc1[0]),
		DEF_PATH(asMix[0]),
		DEF_PATH(asBias[0]),
//		DEF_PATH(asMix2[0]),
	};

#define N_PATH_TABLE (sizeof(table) / sizeof(struct path_table))

	for (i = 0; i < N_PATH_TABLE; i++) {
		MCDRV_CHANNEL *ch = (MCDRV_CHANNEL *)((void *)info + table[i].offset);
#if 1
		pr_info("%s.abSrcOnOff\t= 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
			table[i].name,
			ch->abSrcOnOff[0] & mask,
			ch->abSrcOnOff[1] & mask,
			ch->abSrcOnOff[2] & mask,
			ch->abSrcOnOff[3] & mask,
			ch->abSrcOnOff[4] & mask,
			ch->abSrcOnOff[5] & mask,
			ch->abSrcOnOff[6] & mask
			);
#else
		int j;
		for (j = 0; j < SOURCE_BLOCK_NUM; j++) {
			if (ch->abSrcOnOff[j] != 0) {
				pr_info("%s.abSrcOnOff[%d] = 0x%02x\n",
					 table[i].name, j, ch->abSrcOnOff[j]);
			}
		}
#endif
	}
}
#else
static const struct {
  const char* name;
  char src[28];
  int print;
} outinfo[] =
{//               MIC       LINEIN1   LINEIN2   DIR       D P ADC   DAC       M A CDSP
 //               1-2-3     L-R-M     L-R-M     0-1-2-2D  T D 0-1   L-R-M     X E N-D
  {"HPOUT_L",    {1,1,1,0,  1,0,1,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  1,0,1,0,  0,0,0,0}, 1},
  {"HPOUT_R",    {1,1,1,0,  0,1,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,1,0,0,  0,0,0,0}, 1},
  {"SPOUT_L",    {0,0,0,0,  1,0,1,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  1,0,1,0,  0,0,0,0}, 1},
  {"SPOUT_R",    {0,0,0,0,  0,1,1,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,1,1,0,  0,0,0,0}, 1},
  {"RCOUT  ",    {1,1,1,0,  0,0,1,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  1,1,0,0,  0,0,0,0}, 1},
  {"LOUT1_L",    {1,1,1,0,  1,0,1,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  1,0,1,0,  0,0,0,0}, 1},
  {"LOUT1_R",    {1,1,1,0,  0,1,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,1,0,0,  0,0,0,0}, 1},
  {"LOUT2_L",    {1,1,1,0,  1,0,1,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  1,0,1,0,  0,0,0,0}, 0},
  {"LOUT2_R",    {1,1,1,0,  0,1,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,1,0,0,  0,0,0,0}, 0},
  {"PEAK   ",    {0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0}, 0},
  {"DIT0   ",    {0,0,0,0,  0,0,0,0,  0,0,0,0,  1,1,1,0,  0,1,1,0,  0,0,0,0,  1,1,0,0}, 1},
  {"DIT1   ",    {0,0,0,0,  0,0,0,0,  0,0,0,0,  1,1,1,0,  0,1,1,0,  0,0,0,0,  1,1,0,0}, 1},
  {"DIT2   ",    {0,0,0,0,  0,0,0,0,  0,0,0,0,  1,1,1,0,  0,1,1,0,  0,0,0,0,  1,1,0,0}, 1},
  {"DAC_M  ",    {0,0,0,0,  0,0,0,0,  0,0,0,0,  1,1,1,0,  0,1,1,0,  0,0,0,0,  1,1,0,0}, 1},
  {"DAC_V  ",    {0,0,0,0,  0,0,0,0,  0,0,0,0,  1,1,1,0,  0,1,1,0,  0,0,0,0,  1,1,0,0}, 0},
  {"AE     ",    {0,0,0,0,  0,0,0,0,  0,0,0,0,  1,1,1,0,  0,1,1,0,  0,0,0,0,  1,0,0,0}, 1},
  {"CDSP_0 ",    {0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0}, 0},
  {"CDSP_1 ",    {0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0}, 0},
  {"CDSP_2 ",    {0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0}, 0},
  {"CDSP_3 ",    {0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0}, 0},
  {"ADC0_L ",    {1,1,1,0,  1,0,1,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0}, 1},
  {"ADC0_R ",    {1,1,1,0,  0,1,1,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0}, 1},
  {"ADC1   ",    {0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0}, 0},
  {"MIX    ",    {0,0,0,0,  0,0,0,0,  0,0,0,0,  1,1,1,0,  0,1,1,0,  0,0,0,0,  0,1,0,0}, 1},
  {"BIAS   ",    {1,1,1,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0}, 1},
  {"MIX2   ",    {0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0}, 0},
}; // max 7 inputs per line
static const char inname[][8] =
{
  "(MIC1) ",
  "(MIC2) ",
  "(MIC3) ",
  "(unk0) ",
  "(LI1_L)",
  "(LI1_R)",
  "(LI1_M)",
  "(unk1) ",
  "(LI2_L)",
  "(LI2_R)",
  "(LI2_M)",
  "(unk2) ",
  "(DIR0) ",
  "(DIR1) ",
  "(DIR2) ",
  "(DIR2D)",
  "(DTMF) ",
  "(PDM)  ",
  "(ADC0) ",
  "(ADC1) ",
  "(DAC_L)",
  "(DAC_R)",
  "(DAC_M)",
  "(unk5) ",
  "(MIX)  ",
  "(AE)   ",
  "(CDSP) ",
  "(CDSPD)",
};
static void mc_asoc_dump_path_info(const void *pvPrm, UINT32 dPrm)
{
  const MCDRV_PATH_INFO* pPathInfo = pvPrm;
  unsigned int o,i,s,n;
  const unsigned char* p;
  static char str[80];
  for (o = 0;o < ARRAY_SIZE(outinfo);o++)
  {
    p = pPathInfo->asHpOut[o].abSrcOnOff;
    for (i = 0;i < ARRAY_SIZE(inname);i++)
    {
      if (!outinfo[o].src[i] && p[i >> 2] >> ((i & 3) << 1) & 1)
      {
        pr_info("WARNING: ineffectual path: %s o%s\n",outinfo[o].name,inname[i]);
      }
    }
  }
  for (o = 0;o < ARRAY_SIZE(outinfo);o++)
  {
    if (!outinfo[o].print) continue;
    p = pPathInfo->asHpOut[o].abSrcOnOff;
    for (i = n = 0;i < ARRAY_SIZE(inname);i++)
    {
      if (outinfo[o].src[i])
      {
        s = p[i >> 2] >> ((i & 3) << 1) & 3;
        if (s)
        {
          n += snprintf(str + n,sizeof str - n," %c%s",s == 2 ? 'x' : 'o',inname[i]);
        }
      }
    }
    if (n) pr_info("path: %s%s\n",outinfo[o].name,str);
  }
}
#endif

#define DEF_VOL(v) {offsetof(MCDRV_VOL_INFO, v), #v}

/* ebabling dB value LOGGING */
#if 0
#else
static char* get_db(int v)
{
  static char str[32];
  unsigned long l,i,n,x;

  /* convert v int section[d15-d8] to string int section, make l where str[l] = '.' */
  v &= ~1;
  if (v < 0)
  {
    v = -v;
    l = snprintf(str,sizeof str,"-%d.",v >> 8 & 0xff) - 1;
  }
  else
  {
    l = snprintf(str,sizeof str,"%d.",v >> 8 & 0xff) - 1;
  }

  /* convert v flac section[d7-d1] to int n, range 0...9921875(=10000000 * 127 / 128) */
  n = (v >> 1 & 0x7f) * (10000000 / 128);

  /* convert n to string flac section */
  for (i = 7;0 < i;i--)
  {
    x = n / 10;
    str[l + i] = '0' + n - x * 10;
    n = x;
  }

  /* delete trailing '0', delete '.' where no flac value */
  for (i = 7;0 < i;i--)
  {
    if (str[l + i] != '0')
    {
      i++;
      break;
    }
  }
  str[l + i] = 0;
  return str;
}
#endif

static void mc_asoc_dump_vol_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_VOL_INFO *info = (MCDRV_VOL_INFO *)pvPrm;
	int i;

	struct vol_table {
		size_t offset;
		char *name;
	};

	struct vol_table table[] = {
		DEF_VOL(aswD_Ad0[0]), DEF_VOL(aswD_Ad0[1]),
		DEF_VOL(aswD_Reserved1[0]),
		DEF_VOL(aswD_Aeng6[0]), DEF_VOL(aswD_Aeng6[1]),
		DEF_VOL(aswD_Pdm[0]), DEF_VOL(aswD_Pdm[1]),
		DEF_VOL(aswD_Dtmfb[0]), DEF_VOL(aswD_Dtmfb[1]),
		DEF_VOL(aswD_Dir0[0]), DEF_VOL(aswD_Dir0[1]),
		DEF_VOL(aswD_Dir1[0]), DEF_VOL(aswD_Dir1[1]),
		DEF_VOL(aswD_Dir2[0]), DEF_VOL(aswD_Dir2[1]),
		DEF_VOL(aswD_Ad0Att[0]), DEF_VOL(aswD_Ad0Att[1]),
		DEF_VOL(aswD_Reserved2[0]),
		DEF_VOL(aswD_Dir0Att[0]), DEF_VOL(aswD_Dir0Att[1]),
		DEF_VOL(aswD_Dir1Att[0]), DEF_VOL(aswD_Dir1Att[1]),
		DEF_VOL(aswD_Dir2Att[0]), DEF_VOL(aswD_Dir2Att[1]),
		DEF_VOL(aswD_SideTone[0]), DEF_VOL(aswD_SideTone[1]),
		DEF_VOL(aswD_DtmfAtt[0]), DEF_VOL(aswD_DtmfAtt[1]),
		DEF_VOL(aswD_DacMaster[0]), DEF_VOL(aswD_DacMaster[1]),
		DEF_VOL(aswD_DacVoice[0]), DEF_VOL(aswD_DacVoice[1]),
		DEF_VOL(aswD_DacAtt[0]), DEF_VOL(aswD_DacAtt[1]),
		DEF_VOL(aswD_Dit0[0]), DEF_VOL(aswD_Dit0[1]),
		DEF_VOL(aswD_Dit1[0]), DEF_VOL(aswD_Dit1[1]),
		DEF_VOL(aswD_Dit2[0]), DEF_VOL(aswD_Dit2[1]),
		DEF_VOL(aswA_Ad0[0]), DEF_VOL(aswA_Ad0[1]),
		DEF_VOL(aswA_Ad1[0]),
		DEF_VOL(aswA_Lin1[0]), DEF_VOL(aswA_Lin1[1]),
		DEF_VOL(aswA_Lin2[0]), DEF_VOL(aswA_Lin2[1]),
		DEF_VOL(aswA_Mic1[0]),
		DEF_VOL(aswA_Mic2[0]),
		DEF_VOL(aswA_Mic3[0]),
		DEF_VOL(aswA_Hp[0]), DEF_VOL(aswA_Hp[1]),
		DEF_VOL(aswA_Sp[0]), DEF_VOL(aswA_Sp[1]),
		DEF_VOL(aswA_Rc[0]),
		DEF_VOL(aswA_Lout1[0]), DEF_VOL(aswA_Lout1[1]),
		DEF_VOL(aswA_Lout2[0]), DEF_VOL(aswA_Lout2[1]),
		DEF_VOL(aswA_Mic1Gain[0]),
		DEF_VOL(aswA_Mic2Gain[0]),
		DEF_VOL(aswA_Mic3Gain[0]),
		DEF_VOL(aswA_HpGain[0]),
		DEF_VOL(aswD_Adc1[0]), DEF_VOL(aswD_Adc1[1]),
		DEF_VOL(aswD_Adc1Att[0]), DEF_VOL(aswD_Adc1Att[1]),
		DEF_VOL(aswD_Dit21[0]), DEF_VOL(aswD_Dit21[1]),
//		DEF_VOL(aswD_Adc0AeMix[0]), DEF_VOL(aswD_Adc0AeMix[1]),
//		DEF_VOL(aswD_Adc1AeMix[0]), DEF_VOL(aswD_Adc1AeMix[1]),
//		DEF_VOL(aswD_Dir0AeMix[0]), DEF_VOL(aswD_Dir0AeMix[1]),
//		DEF_VOL(aswD_Dir1AeMix[0]), DEF_VOL(aswD_Dir1AeMix[1]),
//		DEF_VOL(aswD_Dir2AeMix[0]), DEF_VOL(aswD_Dir2AeMix[1]),
//		DEF_VOL(aswD_PdmAeMix[0]), DEF_VOL(aswD_PdmAeMix[1]),
//		DEF_VOL(aswD_MixAeMix[0]), DEF_VOL(aswD_MixAeMix[1]),
	};

#define N_VOL_TABLE (sizeof(table) / sizeof(struct vol_table))

	for (i = 0; i < N_VOL_TABLE; i++) {
		SINT16 vol = *(SINT16 *)((void *)info + table[i].offset);
		if (vol & 0x0001) {
#if 0
			pr_info("%s = 0x%04x\n", table[i].name, (vol & 0xfffe));
#else
			pr_info("vol: %s = %s[dB](0x%04X)\n",table[i].name,get_db(vol),vol & 0xfffe);
#endif
		}
	}
}

static void mc_asoc_dump_dio_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_DIO_INFO *info = (MCDRV_DIO_INFO *)pvPrm;
	MCDRV_DIO_PORT *port;
	UINT32 update;
	int i;

	for (i = 0; i < IOPORT_NUM; i++) {
		pr_info("asPortInfo[%d]:\n", i);
		port = &info->asPortInfo[i];
		update = dPrm >> (i*3);
		if (update & MCDRV_DIO0_COM_UPDATE_FLAG) {
			pr_info("sDioCommon.bMasterSlave = 0x%02x\n",
				 port->sDioCommon.bMasterSlave);
			pr_info("           bAutoFs = 0x%02x\n",
				 port->sDioCommon.bAutoFs);
			pr_info("           bFs = 0x%02x\n",
				 port->sDioCommon.bFs);
			pr_info("           bBckFs = 0x%02x\n",
				 port->sDioCommon.bBckFs);
			pr_info("           bInterface = 0x%02x\n",
				 port->sDioCommon.bInterface);
			pr_info("           bBckInvert = 0x%02x\n",
				 port->sDioCommon.bBckInvert);
			pr_info("           bPcmHizTim = 0x%02x\n",
				 port->sDioCommon.bPcmHizTim);
			pr_info("           bPcmClkDown = 0x%02x\n",
				 port->sDioCommon.bPcmClkDown);
			pr_info("           bPcmFrame = 0x%02x\n",
				 port->sDioCommon.bPcmFrame);
			pr_info("           bPcmHighPeriod = 0x%02x\n",
				 port->sDioCommon.bPcmHighPeriod);
		}
		if (update & MCDRV_DIO0_DIR_UPDATE_FLAG) {
			pr_info("sDir.wSrcRate = 0x%04x\n",
				 port->sDir.wSrcRate);
			pr_info("     sDaFormat.bBitSel = 0x%02x\n",
				 port->sDir.sDaFormat.bBitSel);
			pr_info("               bMode = 0x%02x\n",
				 port->sDir.sDaFormat.bMode);
			pr_info("     sPcmFormat.bMono = 0x%02x\n",
				 port->sDir.sPcmFormat.bMono);
			pr_info("                bOrder = 0x%02x\n",
				 port->sDir.sPcmFormat.bOrder);
			pr_info("                bLaw = 0x%02x\n",
				 port->sDir.sPcmFormat.bLaw);
			pr_info("                bBitSel = 0x%02x\n",
				 port->sDir.sPcmFormat.bBitSel);
			pr_info("     abSlot[] = {0x%02x, 0x%02x}\n",
				 port->sDir.abSlot[0], port->sDir.abSlot[1]);
		}
		if (update & MCDRV_DIO0_DIT_UPDATE_FLAG) {
			pr_info("sDit.wSrcRate = 0x%04x\n",
				 port->sDit.wSrcRate);
			pr_info("     sDaFormat.bBitSel = 0x%02x\n",
				 port->sDit.sDaFormat.bBitSel);
			pr_info("               bMode = 0x%02x\n",
				 port->sDit.sDaFormat.bMode);
			pr_info("     sPcmFormat.bMono = 0x%02x\n",
				 port->sDit.sPcmFormat.bMono);
			pr_info("                bOrder = 0x%02x\n",
				 port->sDit.sPcmFormat.bOrder);
			pr_info("                bLaw = 0x%02x\n",
				 port->sDit.sPcmFormat.bLaw);
			pr_info("                bBitSel = 0x%02x\n",
				 port->sDit.sPcmFormat.bBitSel);
			pr_info("     abSlot[] = {0x%02x, 0x%02x}\n",
				 port->sDit.abSlot[0], port->sDit.abSlot[1]);
		}
	}
}

static void mc_asoc_dump_dac_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_DAC_INFO *info = (MCDRV_DAC_INFO *)pvPrm;

	if (dPrm & MCDRV_DAC_MSWP_UPDATE_FLAG) {
		pr_info("bMasterSwap = 0x%02x\n", info->bMasterSwap);
	}
	if (dPrm & MCDRV_DAC_VSWP_UPDATE_FLAG) {
		pr_info("bVoiceSwap = 0x%02x\n", info->bVoiceSwap);
	}
	if (dPrm & MCDRV_DAC_HPF_UPDATE_FLAG) {
		pr_info("bDcCut = 0x%02x\n", info->bDcCut);
	}
	if (dPrm & MCDRV_DAC_DACSWP_UPDATE_FLAG) {
		pr_info("bDacSwap = 0x%02x\n", info->bDacSwap);
	}
}

static void mc_asoc_dump_adc_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_ADC_INFO *info = (MCDRV_ADC_INFO *)pvPrm;

	if (dPrm & MCDRV_ADCADJ_UPDATE_FLAG) {
		pr_info("bAgcAdjust = 0x%02x\n", info->bAgcAdjust);
	}
	if (dPrm & MCDRV_ADCAGC_UPDATE_FLAG) {
		pr_info("bAgcOn = 0x%02x\n", info->bAgcOn);
	}
	if (dPrm & MCDRV_ADCMONO_UPDATE_FLAG) {
		pr_info("bMono = 0x%02x\n", info->bMono);
	}
}

static void mc_asoc_dump_sp_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_SP_INFO *info = (MCDRV_SP_INFO *)pvPrm;

	pr_info("bSwap = 0x%02x\n", info->bSwap);
}

static void mc_asoc_dump_dng_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_DNG_INFO *info = (MCDRV_DNG_INFO *)pvPrm;

	if (dPrm & MCDRV_DNGSW_HP_UPDATE_FLAG) {
		pr_info("HP:abOnOff = 0x%02x\n", info->abOnOff[0]);
	}
	if (dPrm & MCDRV_DNGTHRES_HP_UPDATE_FLAG) {
		pr_info("HP:abThreshold = 0x%02x\n", info->abThreshold[0]);
	}
	if (dPrm & MCDRV_DNGHOLD_HP_UPDATE_FLAG) {
		pr_info("HP:abHold = 0x%02x\n", info->abHold[0]);
	}
	if (dPrm & MCDRV_DNGATK_HP_UPDATE_FLAG) {
		pr_info("HP:abAttack = 0x%02x\n", info->abAttack[0]);
	}
	if (dPrm & MCDRV_DNGREL_HP_UPDATE_FLAG) {
		pr_info("HP:abRelease = 0x%02x\n", info->abRelease[0]);
	}
	if (dPrm & MCDRV_DNGTARGET_HP_UPDATE_FLAG) {
		pr_info("HP:abTarget = 0x%02x\n", info->abTarget[0]);
	}
	
	if (dPrm & MCDRV_DNGSW_SP_UPDATE_FLAG) {
		pr_info("SP:abOnOff = 0x%02x\n", info->abOnOff[1]);
	}
	if (dPrm & MCDRV_DNGTHRES_SP_UPDATE_FLAG) {
		pr_info("SP:abThreshold = 0x%02x\n", info->abThreshold[1]);
	}
	if (dPrm & MCDRV_DNGHOLD_SP_UPDATE_FLAG) {
		pr_info("SP:abHold = 0x%02x\n", info->abHold[1]);
	}
	if (dPrm & MCDRV_DNGATK_SP_UPDATE_FLAG) {
		pr_info("SP:abAttack = 0x%02x\n", info->abAttack[1]);
	}
	if (dPrm & MCDRV_DNGREL_SP_UPDATE_FLAG) {
		pr_info("SP:abRelease = 0x%02x\n", info->abRelease[1]);
	}
	if (dPrm & MCDRV_DNGTARGET_SP_UPDATE_FLAG) {
		pr_info("SP:abTarget = 0x%02x\n", info->abTarget[1]);
	}

	if (dPrm & MCDRV_DNGSW_RC_UPDATE_FLAG) {
		pr_info("RC:abOnOff = 0x%02x\n", info->abOnOff[2]);
	}
	if (dPrm & MCDRV_DNGTHRES_RC_UPDATE_FLAG) {
		pr_info("RC:abThreshold = 0x%02x\n", info->abThreshold[2]);
	}
	if (dPrm & MCDRV_DNGHOLD_RC_UPDATE_FLAG) {
		pr_info("RC:abHold = 0x%02x\n", info->abHold[2]);
	}
	if (dPrm & MCDRV_DNGATK_RC_UPDATE_FLAG) {
		pr_info("RC:abAttack = 0x%02x\n", info->abAttack[2]);
	}
	if (dPrm & MCDRV_DNGREL_RC_UPDATE_FLAG) {
		pr_info("RC:abRelease = 0x%02x\n", info->abRelease[2]);
	}
	if (dPrm & MCDRV_DNGTARGET_RC_UPDATE_FLAG) {
		pr_info("RC:abTarget = 0x%02x\n", info->abTarget[2]);
	}

	if (dPrm & MCDRV_DNGFW_FLAG) {
		pr_info("   bFw = 0x%02x\n", info->bFw);
	}
}

static void mc_asoc_dump_ae_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_AE_INFO *info = (MCDRV_AE_INFO *)pvPrm;

	pr_info("bOnOff = 0x%02x\n", info->bOnOff);
	if (dPrm & MCDRV_AEUPDATE_FLAG_BEX) {
		mc_asoc_dump_array("abBex", info->abBex, BEX_PARAM_SIZE);
	}
	if (dPrm & MCDRV_AEUPDATE_FLAG_WIDE) {
		mc_asoc_dump_array("abWide", info->abWide, WIDE_PARAM_SIZE);
	}
	if (dPrm & MCDRV_AEUPDATE_FLAG_DRC) {
		mc_asoc_dump_array("abDrc", info->abDrc, DRC_PARAM_SIZE);
		mc_asoc_dump_array("abDrc2", info->abDrc2, DRC2_PARAM_SIZE);
	}
	if (dPrm & MCDRV_AEUPDATE_FLAG_EQ5) {
		mc_asoc_dump_array("abEq5", info->abEq5, EQ5_PARAM_SIZE);
	}
	if (dPrm & MCDRV_AEUPDATE_FLAG_EQ3) {
		mc_asoc_dump_array("abEq3", info->abEq3, EQ3_PARAM_SIZE);
	}
}

static void mc_asoc_dump_ae_ex(const void *pvPrm, UINT32 dPrm)
{
	UINT8 *info = (UINT8 *)pvPrm;
	char str[128], *p;
	int i;

	p = str;
	pr_info("AE_EX PROGRAM = %ld byte\n", dPrm);
	for (i = 0; i < dPrm && i < 16; i++) {
		p += sprintf(p, "0x%02x ", info[i]);
		if ((i % 16 == 15) || (i == dPrm - 1)) {
			pr_info("%s\n", str);
			p = str;
		}
	}
	if(dPrm > 16) {
		pr_info(":\n:\n");
	}
}

static void mc_asoc_dump_cdsp(const void *pvPrm, UINT32 dPrm)
{
	UINT8 *info = (UINT8 *)pvPrm;
	char str[128], *p;
	int i;

	p = str;
	pr_info("CDSP PROGRAM = %ld byte\n", dPrm);
	for (i = 0; i < dPrm && i < 16; i++) {
		p += sprintf(p, "0x%02x ", info[i]);
		if ((i % 16 == 15) || (i == dPrm - 1)) {
			pr_info("%s\n", str);
			p = str;
		}
	}
	if(dPrm > 16) {
		pr_info(":\n:\n");
	}
}

static void mc_asoc_dump_cdspparam(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_CDSPPARAM *info = (MCDRV_CDSPPARAM *)pvPrm;

	pr_info("bCommand = 0x%02x\n", info->bCommand);

#if 1
	pr_info("abParam = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		info->abParam[0],
		info->abParam[1],
		info->abParam[2],
		info->abParam[3],
		info->abParam[4],
		info->abParam[5],
		info->abParam[6],
		info->abParam[7],
		info->abParam[8],
		info->abParam[9],
		info->abParam[10],
		info->abParam[11],
		info->abParam[12],
		info->abParam[13],
		info->abParam[14],
		info->abParam[15]
		);
#else
	int i;
	for (i = 0; i < 16; i++){
		pr_info("abParam[%d] = 0x%02x\n", i, info->abParam[i]);
	}
#endif
}

static void mc_asoc_dump_pdm_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_PDM_INFO *info = (MCDRV_PDM_INFO *)pvPrm;

	if (dPrm & MCDRV_PDMCLK_UPDATE_FLAG) {
		pr_info("bClk = 0x%02x\n", info->bClk);
	}
	if (dPrm & MCDRV_PDMADJ_UPDATE_FLAG) {
		pr_info("bAgcAdjust = 0x%02x\n", info->bAgcAdjust);
	}
	if (dPrm & MCDRV_PDMAGC_UPDATE_FLAG) {
		pr_info("bAgcOn = 0x%02x\n", info->bAgcOn);
	}
	if (dPrm & MCDRV_PDMEDGE_UPDATE_FLAG) {
		pr_info("bPdmEdge = 0x%02x\n", info->bPdmEdge);
	}
	if (dPrm & MCDRV_PDMWAIT_UPDATE_FLAG) {
		pr_info("bPdmWait = 0x%02x\n", info->bPdmWait);
	}
	if (dPrm & MCDRV_PDMSEL_UPDATE_FLAG) {
		pr_info("bPdmSel = 0x%02x\n", info->bPdmSel);
	}
	if (dPrm & MCDRV_PDMMONO_UPDATE_FLAG) {
		pr_info("bMono = 0x%02x\n", info->bMono);
	}
}

static void mc_asoc_dump_dtmf_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_DTMF_INFO *info = (MCDRV_DTMF_INFO *)pvPrm;
	
	if (dPrm & MCDRV_DTMFHOST_UPDATE_FLAG) {
		pr_info("bSinGenHost = 0x%02x\n", info->bSinGenHost);
	}
	if (dPrm & MCDRV_DTMFONOFF_UPDATE_FLAG) {
		pr_info("bOnOff = 0x%02x\n", info->bOnOff);
	}
	if (dPrm & MCDRV_DTMFFREQ0_UPDATE_FLAG) {
		pr_info("sParam.wSinGen0Freq = 0x%02x\n", info->sParam.wSinGen0Freq);
	}
	if (dPrm & MCDRV_DTMFFREQ1_UPDATE_FLAG) {
		pr_info("sParam.wSinGen1Freq = 0x%02x\n", info->sParam.wSinGen1Freq);
	}
	if (dPrm & MCDRV_DTMFVOL0_UPDATE_FLAG) {
		pr_info("sParam.swSinGen0Vol = 0x%02x\n", info->sParam.swSinGen0Vol);
	}
	if (dPrm & MCDRV_DTMFVOL1_UPDATE_FLAG) {
		pr_info("sParam.swSinGen1Vol = 0x%02x\n", info->sParam.swSinGen1Vol);
	}
	if (dPrm & MCDRV_DTMFGATE_UPDATE_FLAG) {
		pr_info("sParam.bSinGenGate = 0x%02x\n", info->sParam.bSinGenGate);
	}
	if (dPrm & MCDRV_DTMFLOOP_UPDATE_FLAG) {
		pr_info("sParam.bSinGenLoop = 0x%02x\n", info->sParam.bSinGenLoop);
	}
}

static void mc_asoc_dump_clksw_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_CLKSW_INFO *info = (MCDRV_CLKSW_INFO *)pvPrm;

	pr_info("bClkSrc = 0x%02x\n", info->bClkSrc);
}

static void mc_asoc_dump_syseq_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_SYSEQ_INFO *info = (MCDRV_SYSEQ_INFO *)pvPrm;
	int i;

	if (dPrm & MCDRV_SYSEQ_ONOFF_UPDATE_FLAG) {
		pr_info("bOnOff = 0x%02x\n", info->bOnOff);
	}
	if (dPrm & MCDRV_SYSEQ_PARAM_UPDATE_FLAG) {
		for (i = 0; i < 15; i++){
			pr_info("abParam[%d] = 0x%02x\n", i, info->abParam[i]);
		}
	}
}

static void mc_asoc_dump_exparam(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_EXPARAM *info = (MCDRV_EXPARAM *)pvPrm;

	pr_info("bCommand = 0x%02x\n", info->bCommand);

#if 1
	pr_info("abParam = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n",
		info->abParam[0],
		info->abParam[1],
		info->abParam[2],
		info->abParam[3],
		info->abParam[4],
		info->abParam[5],
		info->abParam[6],
		info->abParam[7],
		info->abParam[8],
		info->abParam[9],
		info->abParam[10],
		info->abParam[11],
		info->abParam[12],
		info->abParam[13],
		info->abParam[14],
		info->abParam[15]
		);
#else
	int i;
	for (i = 0; i < 16; i++){
		pr_info("abParam[%d] = 0x%02x\n", i, info->abParam[i]);
	}
#endif
}

static void mc_asoc_dump_ditswap_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_DITSWAP_INFO *info = (MCDRV_DITSWAP_INFO *)pvPrm;
	
	if (dPrm & MCDRV_DITSWAP_L_UPDATE_FLAG) {
		pr_info("bSwapL = 0x%02x\n", info->bSwapL);
	}
	if (dPrm & MCDRV_DITSWAP_R_UPDATE_FLAG) {
		pr_info("bSwapR = 0x%02x\n", info->bSwapR);
	}
}

static void mc_asoc_dump_agc_ex_info(const void *pvPrm, UINT32 dPrm)
{
	MCDRV_ADC_EX_INFO *info = (MCDRV_ADC_EX_INFO *)pvPrm;

	if (dPrm & MCDRV_AGCENV_UPDATE_FLAG) {
		pr_info("bAgcEnv = 0x%02x\n", info->bAgcEnv);
	}
	if (dPrm & MCDRV_AGCLVL_UPDATE_FLAG) {
		pr_info("bAgcLvl = 0x%02x\n", info->bAgcLvl);
	}
	if (dPrm & MCDRV_AGCUPTIM_UPDATE_FLAG) {
		pr_info("bAgcUpTim = 0x%02x\n", info->bAgcUpTim);
	}
	if (dPrm & MCDRV_AGCDWTIM_UPDATE_FLAG) {
		pr_info("bAgcDwTim = 0x%02x\n", info->bAgcDwTim);
	}
	if (dPrm & MCDRV_AGCCUTLVL_UPDATE_FLAG) {
		pr_info("bAgcCutLvl = 0x%02x\n", info->bAgcCutLvl);
	}
	if (dPrm & MCDRV_AGCCUTTIM_UPDATE_FLAG) {
		pr_info("bAgcCutTim = 0x%02x\n", info->bAgcCutTim);
	}
}



struct mc_asoc_dump_func {
	char *name;
	void (*func)(const void *, UINT32);
};

struct mc_asoc_dump_func mc_asoc_dump_func_map[] = {
	{"MCDRV_INIT", mc_asoc_dump_init_info},
	{"MCDRV_TERM", NULL},
	{"MCDRV_READ_REG", mc_asoc_dump_reg_info},
	{"MCDRV_WRITE_REG", NULL},
	{"MCDRV_GET_PATH", NULL},
	{"MCDRV_SET_PATH", mc_asoc_dump_path_info},
	{"MCDRV_GET_VOLUME", NULL},
	{"MCDRV_SET_VOLUME", mc_asoc_dump_vol_info},
	{"MCDRV_GET_DIGITALIO", NULL},
	{"MCDRV_SET_DIGITALIO", mc_asoc_dump_dio_info},
	{"MCDRV_GET_DAC", NULL},
	{"MCDRV_SET_DAC", mc_asoc_dump_dac_info},
	{"MCDRV_GET_ADC", NULL},
	{"MCDRV_SET_ADC", mc_asoc_dump_adc_info},
	{"MCDRV_GET_SP", NULL},
	{"MCDRV_SET_SP", mc_asoc_dump_sp_info},
	{"MCDRV_GET_DNG", NULL},
	{"MCDRV_SET_DNG", mc_asoc_dump_dng_info},
	{"MCDRV_SET_AUDIOENGINE", mc_asoc_dump_ae_info},
	{"MCDRV_SET_AUDIOENGINE_EX", mc_asoc_dump_ae_ex},
	{"MCDRV_SET_CDSP", mc_asoc_dump_cdsp},
	{"MCDRV_GET_CDSP_PARAM", NULL},
	{"MCDRV_SET_CDSP_PARAM", mc_asoc_dump_cdspparam},
	{"MCDRV_REGISTER_CDSP_CB", NULL},
	{"MCDRV_GET_PDM", NULL},
	{"MCDRV_SET_PDM", mc_asoc_dump_pdm_info},
	{"MCDRV_SET_DTMF", mc_asoc_dump_dtmf_info},
	{"MCDRV_CONFIG_GP", NULL},
	{"MCDRV_MASK_GP", NULL},
	{"MCDRV_GETSET_GP", NULL},
	{"MCDRV_GET_PEAK", NULL},
	{"MCDRV_IRQ", NULL},
	{"MCDRV_UPDATE_CLOCK", NULL},
	{"MCDRV_SWITCH_CLOCK", mc_asoc_dump_clksw_info},
	{"MCDRV_GET_SYSEQ", NULL},
	{"MCDRV_SET_SYSEQ", mc_asoc_dump_syseq_info},
	{"MCDRV_GET_DTMF", NULL},
	{"MCDRV_SET_EX_PARAM", mc_asoc_dump_exparam},
	{"MCDRV_GET_DITSWAP", NULL},
	{"MCDRV_SET_DITSWAP", mc_asoc_dump_ditswap_info},
	{"MCDRV_GET_ADC_EX", NULL},
	{"MCDRV_SET_ADC_EX", mc_asoc_dump_agc_ex_info},
	{"MCDRV_GET_PDM_EX", NULL},
	{"MCDRV_SET_PDM_EX", mc_asoc_dump_agc_ex_info},
};

SINT32 McDrv_Ctrl_dbg(UINT32 dCmd, void *pvPrm, UINT32 dPrm)
{
	SINT32 err;

	pr_info("calling %s:\n", mc_asoc_dump_func_map[dCmd].name);

	if (mc_asoc_dump_func_map[dCmd].func) {
		mc_asoc_dump_func_map[dCmd].func(pvPrm, dPrm);
	}

	err = McDrv_Ctrl(dCmd, pvPrm, dPrm);
	pr_info("err = %d\n", (int)err);

	if(dCmd == MCDRV_SET_VOLUME) {
		McDrv_Ctrl(MCDRV_GET_VOLUME, pvPrm, 0);
		mc_asoc_dump_vol_info(pvPrm, 0xFF);
	}
	if(dCmd == MCDRV_GET_PATH) {
		mc_asoc_dump_path_info(pvPrm, 0xFF);
	}
	if(dCmd == MCDRV_SET_PATH) {
		McDrv_Ctrl(MCDRV_GET_PATH, pvPrm, 0);
		mc_asoc_dump_path_info(pvPrm, 0x55);
	}

	return err;
}

//#endif /* CONFIG_SND_SOC_MC_YAMAHA_DEBUG */
/* QNET Audio:2012-01-17 Log end */
