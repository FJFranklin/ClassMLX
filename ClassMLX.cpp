/*
 * Based on MLX90640 code by Melexis N.V.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ClassMLX.hh"

static const char *mlx_Mode_description[] = {
  "Chess",
  "Interleaved"
};
const char *MLX::mode_description(mlx_Mode mode) const {
  return mlx_Mode_description[mode];
}

static const char *mlx_RefreshRate_description[] = {
  "0.5 Hz",
  "1 Hz",
  "2 Hz",
  "4 Hz",
  "8 Hz",
  "16 Hz",
  "32 Hz",
  "64 Hz"
};
const char *MLX::refresh_rate_description(mlx_RefreshRate rate) const {
  return mlx_RefreshRate_description[rate];
}

static const char *mlx_Resolution_description[] = {
  "16 bit",
  "17 bit",
  "18 bit",
  "19 bit"
};
const char *MLX::resolution_description(mlx_Resolution resolution) const {
  return mlx_Resolution_description[resolution];
}

int MLX::read_eeprom() {
  const uint16_t regaddr = 0x2400;
  uint16_t eeData[832];
  for (uint16_t offset = 0; offset < 832; offset += 32) {
    i2c_read_sync(regaddr + offset, eeData + offset, 32);
  }

  struct MLX_Parameters *mlx90640 = &m_params;

  // ExtractVDDParameters

  int16_t kVdd;
  int16_t vdd25;

  kVdd = eeData[51];

  kVdd = (eeData[51] & 0xFF00) >> 8;
  if (kVdd > 127) {
    kVdd = kVdd - 256;
  }
  kVdd = 32 * kVdd;
  vdd25 = eeData[51] & 0x00FF;
  vdd25 = ((vdd25 - 256) << 5) - 8192;

  mlx90640->kVdd = kVdd;
  mlx90640->vdd25 = vdd25;

  // ExtractPTATParameters

  float KvPTAT = (eeData[50] & 0xFC00) >> 10;

  if (KvPTAT > 31) {
    KvPTAT = KvPTAT - 64;
  }
  KvPTAT = KvPTAT / 4096;

  float KtPTAT = eeData[50] & 0x03FF;

  if (KtPTAT > 511) {
    KtPTAT = KtPTAT - 1024;
  }
  KtPTAT = KtPTAT / 8;

  int16_t vPTAT25 = eeData[49];

  float alphaPTAT = (eeData[16] & 0xF000) / pow(2, (double) 14) + 8.0f;

  mlx90640->KvPTAT = KvPTAT;
  mlx90640->KtPTAT = KtPTAT;
  mlx90640->vPTAT25 = vPTAT25;
  mlx90640->alphaPTAT = alphaPTAT;

  // ExtractGainParameters

  int16_t gainEE = eeData[48];

  if (gainEE > 32767) {
    gainEE = gainEE -65536;
  }
  mlx90640->gainEE = gainEE;

  // ExtractTgcParameters

  float tgc = eeData[60] & 0x00FF;

  if (tgc > 127) {
    tgc = tgc - 256;
  }
  tgc = tgc / 32.0f;

  mlx90640->tgc = tgc;

  // ExtractResolutionParameters

  uint8_t resolutionEE = (eeData[56] & 0x3000) >> 12;

  mlx90640->resolutionEE = resolutionEE;

  // ExtractKsTaParameters

  float KsTa = (eeData[60] & 0xFF00) >> 8;

  if (KsTa > 127) {
    KsTa = KsTa -256;
  }
  KsTa = KsTa / 8192.0f;

  mlx90640->KsTa = KsTa;

  // ExtractKsToParameters

  int8_t step = ((eeData[63] & 0x3000) >> 12) * 10;

  mlx90640->ct[0] = -40;
  mlx90640->ct[1] = 0;
  mlx90640->ct[2] = (eeData[63] & 0x00F0) >> 4;
  mlx90640->ct[3] = (eeData[63] & 0x0F00) >> 8;

  mlx90640->ct[2] = mlx90640->ct[2] * step;
  mlx90640->ct[3] = mlx90640->ct[2] + mlx90640->ct[3] * step;
  mlx90640->ct[4] = 400;

  int KsToScale = (eeData[63] & 0x000F) + 8;
  KsToScale = 1 << KsToScale;

  mlx90640->ksTo[0] = eeData[61] & 0x00FF;
  mlx90640->ksTo[1] = (eeData[61] & 0xFF00) >> 8;
  mlx90640->ksTo[2] = eeData[62] & 0x00FF;
  mlx90640->ksTo[3] = (eeData[62] & 0xFF00) >> 8;

  for (int i = 0; i < 4; i++) {
    if (mlx90640->ksTo[i] > 127) {
      mlx90640->ksTo[i] = mlx90640->ksTo[i] - 256;
    }
    mlx90640->ksTo[i] = mlx90640->ksTo[i] / KsToScale;
  }
  mlx90640->ksTo[4] = -0.0002;

  // ExtractCPParameters

  uint8_t alphaScale = ((eeData[32] & 0xF000) >> 12) + 27;

  int16_t offsetSP[2];

  offsetSP[0] = (eeData[58] & 0x03FF);
  if (offsetSP[0] > 511) {
    offsetSP[0] = offsetSP[0] - 1024;
  }
  offsetSP[1] = (eeData[58] & 0xFC00) >> 10;
  if (offsetSP[1] > 31) {
    offsetSP[1] = offsetSP[1] - 64;
  }
  offsetSP[1] = offsetSP[1] + offsetSP[0];

  float alphaSP[2];

  alphaSP[0] = (eeData[57] & 0x03FF);
  if (alphaSP[0] > 511) {
    alphaSP[0] = alphaSP[0] - 1024;
  }
  alphaSP[0] = alphaSP[0] / pow(2, (double) alphaScale);

  alphaSP[1] = (eeData[57] & 0xFC00) >> 10;
  if (alphaSP[1] > 31) {
    alphaSP[1] = alphaSP[1] - 64;
  }
  alphaSP[1] = (1 + alphaSP[1]/128) * alphaSP[0];

  mlx90640->cpAlpha[0] = alphaSP[0];
  mlx90640->cpAlpha[1] = alphaSP[1];

  float cpKta = (eeData[59] & 0x00FF);
  if (cpKta > 127) {
    cpKta = cpKta - 256;
  }

  uint8_t ktaScale1 = ((eeData[56] & 0x00F0) >> 4) + 8;
  mlx90640->cpKta = cpKta / pow(2, (double) ktaScale1);

  float cpKv = (eeData[59] & 0xFF00) >> 8;
  if (cpKv > 127) {
    cpKv = cpKv - 256;
  }

  uint8_t kvScale = (eeData[56] & 0x0F00) >> 8;
  mlx90640->cpKv = cpKv / pow(2, (double) kvScale);

  mlx90640->cpOffset[0] = offsetSP[0];
  mlx90640->cpOffset[1] = offsetSP[1];

  // ExtractAlphaParameters

  int accRow[24];
  int accColumn[32];

  uint8_t accRemScale    =   eeData[32] & 0x000F;
  uint8_t accColumnScale =  (eeData[32] & 0x00F0) >> 4;
  uint8_t accRowScale    =  (eeData[32] & 0x0F00) >> 8;

  alphaScale = ((eeData[32] & 0xF000) >> 12) + 30; // Note: was +27 earlier

  int alphaRef = eeData[33];

  for (int i = 0; i < 6; i++) {
    int p = i * 4;
    accRow[p + 0] = (eeData[34 + i] & 0x000F);
    accRow[p + 1] = (eeData[34 + i] & 0x00F0) >> 4;
    accRow[p + 2] = (eeData[34 + i] & 0x0F00) >> 8;
    accRow[p + 3] = (eeData[34 + i] & 0xF000) >> 12;
  }

  for (int i = 0; i < 24; i++) {
    if (accRow[i] > 7) {
      accRow[i] = accRow[i] - 16;
    }
  }

  for (int i = 0; i < 8; i++) {
    int p = i * 4;
    accColumn[p + 0] = (eeData[40 + i] & 0x000F);
    accColumn[p + 1] = (eeData[40 + i] & 0x00F0) >> 4;
    accColumn[p + 2] = (eeData[40 + i] & 0x0F00) >> 8;
    accColumn[p + 3] = (eeData[40 + i] & 0xF000) >> 12;
  }

  for (int i = 0; i < 32; i ++) {
    if (accColumn[i] > 7) {
      accColumn[i] = accColumn[i] - 16;
    }
  }

  float *scratchData = m_cam; // use the frame buffer as a temp space

  for (int i = 0; i < 24; i++) {
    for(int j = 0; j < 32; j ++) {
      int p = 32 * i + j;
      scratchData[p] = (eeData[64 + p] & 0x03F0) >> 4;
      if (scratchData[p] > 31) {
	scratchData[p] = scratchData[p] - 64;
      }
      scratchData[p] *= (1 << accRemScale);
      scratchData[p] += alphaRef + (accRow[i] << accRowScale) + (accColumn[j] << accColumnScale);
      scratchData[p] /= pow(2, (double) alphaScale);
      scratchData[p] -= mlx90640->tgc * (mlx90640->cpAlpha[0] + mlx90640->cpAlpha[1]) / 2;
      scratchData[p]  = mlx_SCALEALPHA / scratchData[p];
    }
  }

  float temp = scratchData[0];
  for (int i = 1; i < 768; i++) {
    if (scratchData[i] > temp) {
      temp = scratchData[i];
    }
  }

  alphaScale = 0;
  while (temp < 32768) {
    temp *= 2;
    alphaScale += 1;
  }

  for (int i = 0; i < 768; i++) {
    temp = scratchData[i] * pow(2, (double) alphaScale);
    mlx90640->alpha[i] = (temp + 0.5);
  }
  mlx90640->alphaScale = alphaScale;

  // ExtractOffsetParameters

  int *occRow    = accRow; // reuse space
  int *occColumn = accColumn;

  uint8_t occRemScale    = (eeData[16] & 0x000F);
  uint8_t occColumnScale = (eeData[16] & 0x00F0) >> 4;
  uint8_t occRowScale    = (eeData[16] & 0x0F00) >> 8;

  int16_t offsetRef = eeData[17];
  if (offsetRef > 32767) {
    offsetRef = offsetRef - 65536;
  }

  for (int i = 0; i < 6; i++) {
    int p = i * 4;
    occRow[p + 0] = (eeData[18 + i] & 0x000F);
    occRow[p + 1] = (eeData[18 + i] & 0x00F0) >> 4;
    occRow[p + 2] = (eeData[18 + i] & 0x0F00) >> 8;
    occRow[p + 3] = (eeData[18 + i] & 0xF000) >> 12;
  }

  for (int i = 0; i < 24; i++) {
    if (occRow[i] > 7) {
      occRow[i] = occRow[i] - 16;
    }
  }

  for (int i = 0; i < 8; i++) {
    int p = i * 4;
    occColumn[p + 0] = (eeData[24 + i] & 0x000F);
    occColumn[p + 1] = (eeData[24 + i] & 0x00F0) >> 4;
    occColumn[p + 2] = (eeData[24 + i] & 0x0F00) >> 8;
    occColumn[p + 3] = (eeData[24 + i] & 0xF000) >> 12;
  }

  for (int i = 0; i < 32; i ++) {
    if (occColumn[i] > 7) {
      occColumn[i] = occColumn[i] - 16;
    }
  }

  for (int i = 0; i < 24; i++) {
    for (int j = 0; j < 32; j ++) {
      int p = 32 * i +j;
      mlx90640->offset[p] = (eeData[64 + p] & 0xFC00) >> 10;
      if (mlx90640->offset[p] > 31) {
	mlx90640->offset[p] = mlx90640->offset[p] - 64;
      }
      mlx90640->offset[p] *= (1 << occRemScale);
      mlx90640->offset[p] += offsetRef + (occRow[i] << occRowScale) + (occColumn[j] << occColumnScale);
    }
  }

  // ExtractKtaPixelParameters

  int8_t KtaRC[4];

  int8_t KtaRoCo = (eeData[54] & 0xFF00) >> 8;
  if (KtaRoCo > 127) {
    KtaRoCo = KtaRoCo - 256;
  }
  KtaRC[0] = KtaRoCo;

  int8_t KtaReCo = (eeData[54] & 0x00FF);
  if (KtaReCo > 127) {
    KtaReCo = KtaReCo - 256;
  }
  KtaRC[2] = KtaReCo;

  int8_t KtaRoCe = (eeData[55] & 0xFF00) >> 8;
  if (KtaRoCe > 127) {
    KtaRoCe = KtaRoCe - 256;
  }
  KtaRC[1] = KtaRoCe;

  int8_t KtaReCe = (eeData[55] & 0x00FF);
  if (KtaReCe > 127) {
    KtaReCe = KtaReCe - 256;
  }
  KtaRC[3] = KtaReCe;

//uint8_t ktaScale1 = ((eeData[56] & 0x00F0) >> 4) + 8;
  uint8_t ktaScale2 =  (eeData[56] & 0x000F);

  for (int i = 0; i < 24; i++) {
    for (int j = 0; j < 32; j ++) {
      int p = 32 * i + j;
      uint8_t split = 2 * (p/32 - (p/64)*2) + p%2;
      scratchData[p] = (eeData[64 + p] & 0x000E) >> 1;
      if (scratchData[p] > 3) {
	scratchData[p] -= 8;
      }
      scratchData[p] *= (1 << ktaScale2);
      scratchData[p] += KtaRC[split];
      scratchData[p] /= pow(2, (double) ktaScale1);
    }
  }

  temp = fabs(scratchData[0]);
  for (int i = 1; i < 768; i++) {
    if (fabs(scratchData[i]) > temp) {
      temp = fabs(scratchData[i]);
    }
  }

  ktaScale1 = 0;
  while (temp < 64) {
    temp *= 2;
    ktaScale1 += 1;
  }

  for (int i = 0; i < 768; i++) {
    temp = scratchData[i] * pow(2, (double) ktaScale1);
    if (temp < 0) {
      mlx90640->kta[i] = (temp - 0.5);
    } else {
      mlx90640->kta[i] = (temp + 0.5);
    }
  }
  mlx90640->ktaScale = ktaScale1;

  // ExtractKvPixelParameters

  int8_t KvT[4];

  int8_t KvRoCo = (eeData[52] & 0xF000) >> 12;
  if (KvRoCo > 7) {
    KvRoCo = KvRoCo - 16;
  }
  KvT[0] = KvRoCo;

  int8_t KvReCo = (eeData[52] & 0x0F00) >> 8;
  if (KvReCo > 7) {
    KvReCo = KvReCo - 16;
  }
  KvT[2] = KvReCo;

  int8_t KvRoCe = (eeData[52] & 0x00F0) >> 4;
  if (KvRoCe > 7) {
    KvRoCe = KvRoCe - 16;
  }
  KvT[1] = KvRoCe;

  int8_t KvReCe = (eeData[52] & 0x000F);
  if (KvReCe > 7) {
    KvReCe = KvReCe - 16;
  }
  KvT[3] = KvReCe;

//uint8_t kvScale = (eeData[56] & 0x0F00) >> 8;

  for (int i = 0; i < 24; i++) {
    for (int j = 0; j < 32; j++) {
      int p = 32 * i + j;
      uint8_t split = 2 * (p/32 - (p/64)*2) + p%2;
      scratchData[p]  = KvT[split];
      scratchData[p] /= pow(2, (double) kvScale);
    }
  }

  temp = fabs(scratchData[0]);
  for (int i = 1; i < 768; i++) {
    if (fabs(scratchData[i]) > temp) {
      temp = fabs(scratchData[i]);
    }
  }

  kvScale = 0;
  while (temp < 64) {
    temp *= 2;
    kvScale += 1;
  }

  for (int i = 0; i < 768; i++) {
    temp = scratchData[i] * pow(2, (double) kvScale);
    if (temp < 0) {
      mlx90640->kv[i] = (temp - 0.5);
    } else {
      mlx90640->kv[i] = (temp + 0.5);
    }
  }
  mlx90640->kvScale = kvScale;

  // ExtractCILCParameters

  float ilChessC[3];

  uint8_t calibrationModeEE = (eeData[10] & 0x0800) >> 4;
  mlx90640->calibrationModeEE = calibrationModeEE ^ 0x80;

  ilChessC[0] = (eeData[53] & 0x003F);
  if (ilChessC[0] > 31) {
    ilChessC[0] = ilChessC[0] - 64;
  }
  ilChessC[0] = ilChessC[0] / 16.0f;

  ilChessC[1] = (eeData[53] & 0x07C0) >> 6;
  if (ilChessC[1] > 15) {
    ilChessC[1] = ilChessC[1] - 32;
  }
  ilChessC[1] = ilChessC[1] / 2.0f;

  ilChessC[2] = (eeData[53] & 0xF800) >> 11;
  if (ilChessC[2] > 15) {
    ilChessC[2] = ilChessC[2] - 32;
  }
  ilChessC[2] = ilChessC[2] / 8.0f;

  mlx90640->ilChessC[0] = ilChessC[0];
  mlx90640->ilChessC[1] = ilChessC[1];
  mlx90640->ilChessC[2] = ilChessC[2];

  // ExtractDeviatingPixels

  int warn = 0;

  for (int p = 0; p < 5; p++) {
    mlx90640->brokenPixels[p] = 0xFFFF;
    mlx90640->outlierPixels[p] = 0xFFFF;
  }

  uint16_t brokenPixCnt = 0;
  uint16_t outlierPixCnt = 0;
  uint16_t pixCnt = 0;

  while (pixCnt < 768 && brokenPixCnt < 5 && outlierPixCnt < 5) {
    if (eeData[pixCnt+64] == 0) {
      mlx90640->brokenPixels[brokenPixCnt] = pixCnt;
      brokenPixCnt = brokenPixCnt + 1;
    } else if ((eeData[pixCnt+64] & 0x0001) != 0) {
      mlx90640->outlierPixels[outlierPixCnt] = pixCnt;
      outlierPixCnt = outlierPixCnt + 1;
    }
    pixCnt++;
  }

  if (brokenPixCnt > 4) {
    Serial.print("Broken pixels: ");
    Serial.println(brokenPixCnt);
    warn = -3;
  } else if (outlierPixCnt > 4) {
    Serial.print("Outlier pixels: ");
    Serial.println(outlierPixCnt);
    warn = -4;
  } else if ((brokenPixCnt + outlierPixCnt) > 4) {
    Serial.print("Broken+outlier pixels: ");
    Serial.println(brokenPixCnt + outlierPixCnt);
    warn = -5;
  } else {
    for (pixCnt = 0; pixCnt < brokenPixCnt; pixCnt++) {
      for(int i = pixCnt + 1; i < brokenPixCnt; i++) {
	warn = check_adjacent(mlx90640->brokenPixels[pixCnt], mlx90640->brokenPixels[i]);
	if (warn != 0) {
	  Serial.println("Broken pixel has adjacent broken pixel");
	  return warn;
	}
      }
    }

    for (pixCnt = 0; pixCnt < outlierPixCnt; pixCnt++) {
      for (int i = pixCnt + 1; i < outlierPixCnt; i++) {
	warn = check_adjacent(mlx90640->outlierPixels[pixCnt], mlx90640->outlierPixels[i]);
	if (warn != 0) {
	  Serial.println("Outlier pixel has adjacent outlier pixel");
	  return warn;
	}
      }
    }

    for (pixCnt = 0; pixCnt < brokenPixCnt; pixCnt++) {
      for (int i = 0; i < outlierPixCnt; i++) {
	warn = check_adjacent(mlx90640->brokenPixels[pixCnt], mlx90640->outlierPixels[i]);
	if (warn != 0) {
	  Serial.println("Broken pixel has adjacent outlier pixel");
	  return warn;
	}
      }
    }
  }

  return warn;
}

int MLX::check_adjacent(uint16_t pix1, uint16_t pix2) {
  int pixPosDif = pix1 - pix2;

  if (pixPosDif > -34 && pixPosDif < -30) {
    return -6;
  }
  if (pixPosDif > -2 && pixPosDif < 2) {
    return -6;
  }
  if (pixPosDif > 30 && pixPosDif < 34) {
    return -6;
  }
  return 0;
}

float MLX::get_Vdd()
{
  struct MLX_Parameters *params = &m_params;

  float vdd = m_raw[810];
  if (vdd > 32767) {
    vdd = vdd - 65536;
  }

  int resolutionRAM = m_Resolution;

  float resolutionCorrection = pow(2, (double) params->resolutionEE) / pow(2, (double) resolutionRAM);

  vdd = (resolutionCorrection * vdd - params->vdd25) / params->kVdd + 3.3;

  return vdd;
}

float MLX::calculate_ambient(float vdd) {
  struct MLX_Parameters *params = &m_params;

  float ptat = m_raw[800];
  if (ptat > 32767) {
    ptat = ptat - 65536;
  }

  float ptatArt = m_raw[768];
  if (ptatArt > 32767) {
    ptatArt = ptatArt - 65536;
  }
  ptatArt = (ptat / (ptat * params->alphaPTAT + ptatArt)) * pow(2, (double) 18);

  float ta = (ptatArt / (1 + params->KvPTAT * (vdd - 3.3)) - params->vPTAT25);
  ta = ta / params->KtPTAT + 25;

  m_ambient = ta; // record the ambient temperature

  return ta;
}

void MLX::calculate_temperatures() {
  struct MLX_Parameters *params = &m_params;

  const float openair = 8; // For a MLX90640 in the open air the shift is -8 degC.
  const float emissivity = 0.95;

  float vdd = get_Vdd();
  float ta  = calculate_ambient(vdd);
  float tr  = ta - openair;

  float _ta  = ta - 25;
  float _vdd = vdd - 3.3;

  float ta4 = (ta + 273.15);
  ta4 = ta4 * ta4;
  ta4 = ta4 * ta4;

  float tr4 = (tr + 273.15);
  tr4 = tr4 * tr4;
  tr4 = tr4 * tr4;

  float taTr = tr4 - (tr4 - ta4) / emissivity;

  float ktaScale   = pow(2, (double) params->ktaScale);
  float kvScale    = pow(2, (double) params->kvScale);
  float alphaScale = pow(2, (double) params->alphaScale);

  float alphaCorrR[4];
  alphaCorrR[0] = 1 / (1 + params->ksTo[0] * 40);
  alphaCorrR[1] = 1 ;
  alphaCorrR[2] = (1 + params->ksTo[1] * params->ct[2]);
  alphaCorrR[3] = alphaCorrR[2] * (1 + params->ksTo[2] * (params->ct[3] - params->ct[2]));

//------------------------- Gain calculation -----------------------------------
  float gain = m_raw[778];
  if (gain > 32767) {
    gain = gain - 65536;
  }
  gain = params->gainEE / gain;

//------------------------- To calculation -------------------------------------
  uint8_t mode = (m_Mode == MLX90640_CHESS) ? 0x80 : 0x00; // (m_raw[832] & 0x1000) >> 5;

  float irDataCP[2];
  irDataCP[0] = m_raw[776];
  irDataCP[1] = m_raw[808];
  for (int i = 0; i < 2; i++) {
    if (irDataCP[i] > 32767) {
      irDataCP[i] = irDataCP[i] - 65536;
    }
    irDataCP[i] = irDataCP[i] * gain;
  }
  irDataCP[0] -= params->cpOffset[0] * (1 + params->cpKta * _ta) * (1 + params->cpKv * _vdd);

  if (mode == params->calibrationModeEE) {
    irDataCP[1] -= params->cpOffset[1] * (1 + params->cpKta * _ta) * (1 + params->cpKv * _vdd);
  } else {
    irDataCP[1] -= (params->cpOffset[1] + params->ilChessC[0]) * (1 + params->cpKta * _ta) * (1 + params->cpKv * _vdd);
  }

  for (int pixelNumber = 0; pixelNumber < 768; pixelNumber++) {
    int8_t ilPattern    = pixelNumber / 32 - (pixelNumber / 64) * 2;
    int8_t chessPattern = ilPattern ^ (pixelNumber - (pixelNumber/2)*2);
    int8_t conversionPattern = ((pixelNumber + 2) / 4 - (pixelNumber + 3) / 4 + (pixelNumber + 1) / 4 - pixelNumber / 4) * (1 - 2 * ilPattern);

    int8_t pattern = (mode == 0) ? ilPattern : chessPattern;

    if (pattern == m_subpage) {
      float irData = m_raw[pixelNumber];
      if (irData > 32767) {
	irData = irData - 65536;
      }
      irData *= gain;

      float kta = params->kta[pixelNumber] / ktaScale;
      float kv  = params->kv[pixelNumber] / kvScale;

      irData -= params->offset[pixelNumber] * (1 + kta*_ta) * (1 + kv*_vdd);

      if (mode != params->calibrationModeEE) {
	irData += params->ilChessC[2] * (2 * ilPattern - 1) - params->ilChessC[1] * conversionPattern;
      }
      irData -= params->tgc * irDataCP[m_subpage];
      irData /= emissivity;

      float alphaCompensated = mlx_SCALEALPHA * alphaScale / params->alpha[pixelNumber];
      alphaCompensated *= (1 + params->KsTa * _ta);

      float Sx = alphaCompensated * alphaCompensated * alphaCompensated * (irData + alphaCompensated * taTr);
      Sx = sqrt(sqrt(Sx)) * params->ksTo[1];

      float To = sqrt(sqrt(irData/(alphaCompensated * (1 - params->ksTo[1] * 273.15) + Sx) + taTr)) - 273.15;

      int8_t range = 3;

      if (To < params->ct[1]) {
	range = 0;
      } else if (To < params->ct[2]) {
	range = 1;
      } else if (To < params->ct[3]) {
	range = 2;
      }

      To = sqrt(sqrt(irData / (alphaCompensated * alphaCorrR[range] * (1 + params->ksTo[range] * (To - params->ct[range]))) + taTr)) - 273.15;

      m_cam[pixelNumber] = To;
    }
  }
}
