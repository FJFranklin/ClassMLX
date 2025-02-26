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

#ifndef ClassMLX_HH
#define ClassMLX_HH

#include <Arduino.h>
#include <i2c_device.h> // Teensy 4.0 i2c library

// Device address
const uint8_t MLX90640_I2CADDR_DEFAULT = 0x33;

// Registers
const uint16_t MLX90640_DEVICEID1 = 0x2407;
const uint16_t MLX90640_CONTROL1  = 0x800D;
const uint16_t MLX90640_STATUS1   = 0x8000;

const float mlx_SCALEALPHA = 0.000001;

enum mlx_Mode {
  MLX90640_CHESS = 0,
  MLX90640_INTERLEAVED
};

enum mlx_RefreshRate {
  MLX90640_0_5_HZ = 0,
  MLX90640_1_HZ,
  MLX90640_2_HZ,
  MLX90640_4_HZ,
  MLX90640_8_HZ,
  MLX90640_16_HZ,
  MLX90640_32_HZ,
  MLX90640_64_HZ
};

enum mlx_Resolution {
  MLX90640_ADC_16BIT = 0,
  MLX90640_ADC_17BIT,
  MLX90640_ADC_18BIT,
  MLX90640_ADC_19BIT
};

static const char *s_hex = "0123456789ABCDEF";

class MLX {
private:
  float    m_cam[32*24];
public:
  const float *get_frame() const { return m_cam; }
private:
  uint16_t m_raw[32*26];

  uint8_t m_buffer[64];
  uint8_t m_address;

  I2CMaster &m_i2c;

  uint16_t m_subpage;

  int  m_row;
  bool m_bCycling;
  bool m_bCalcT;

  float m_ambient;

  mlx_Mode m_Mode;
  mlx_RefreshRate m_RefreshRate;
  mlx_Resolution m_Resolution;

  struct MLX_Parameters {
    int16_t  kVdd;
    int16_t  vdd25;
    float    KvPTAT;
    float    KtPTAT;
    uint16_t vPTAT25;
    float    alphaPTAT;
    int16_t  gainEE;
    float    tgc;
    float    cpKv;
    float    cpKta;
    uint8_t  resolutionEE;
    uint8_t  calibrationModeEE;
    float    KsTa;
    float    ksTo[5];
    int16_t  ct[5];
    uint16_t alpha[768];
    uint8_t  alphaScale;
    int16_t  offset[768];
    int8_t   kta[768];
    uint8_t  ktaScale;
    int8_t   kv[768];
    uint8_t  kvScale;
    float    cpAlpha[2];
    int16_t  cpOffset[2];
    float    ilChessC[3];
    uint16_t brokenPixels[5];
    uint16_t outlierPixels[5];
  } m_params;
public:
  MLX(I2CMaster &i2c, uint8_t address = MLX90640_I2CADDR_DEFAULT) :
    m_address(address),
    m_i2c(i2c),
    m_subpage(0),
    m_row(26),
    m_bCycling(false),
    m_bCalcT(false),
    m_ambient(0.0),
    m_Mode(MLX90640_CHESS),
    m_RefreshRate(MLX90640_2_HZ),
    m_Resolution(MLX90640_ADC_19BIT)
  {
    // ...
  }
  ~MLX() {
    // ...
  }
  float get_ambient() const { // last calculated ambient temperature
    return m_ambient;
  }
private:
  int read_eeprom(); // returns non-zero if adjacent bad pixels
  int check_adjacent(uint16_t pix1, uint16_t pix2);

  float get_Vdd();
  float calculate_ambient(float vdd);
  void  calculate_temperatures();

  bool i2c_read_sync(uint16_t regaddr, uint16_t *word_buffer, uint16_t word_count = 1) {
    if (!word_count || !word_buffer || busy()) return false;

    if (word_count > 32) return false;

    uint16_t byte_count = word_count << 1;

    m_buffer[0] = regaddr >> 8;
    m_buffer[1] = regaddr & 0xFF;

    m_i2c.write_async(m_address, m_buffer, 2, false);
    while (!m_i2c.finished());
    if (m_i2c.has_error()) {
      Serial.print("[i2c-write-error]");
      return false;
    }
    m_i2c.read_async(m_address, m_buffer, byte_count, true);
    while (!m_i2c.finished());
    if (m_i2c.has_error()) {
      Serial.print("[i2c-read-error]");
      return false;
    }

    uint8_t *ptr = m_buffer;
    for (int i = 0; i < word_count; i++) {
      uint16_t hi = *ptr++;
      uint16_t lo = *ptr++;
      word_buffer[i] = hi << 8 | lo;
    }
    return true;
  }
  bool i2c_write_sync(uint16_t regaddr, uint16_t *word_buffer, uint16_t word_count = 1) {
    if (!word_count || !word_buffer || busy()) return false;

    if (word_count > 31) return false;

    uint8_t *ptr = m_buffer;
    *ptr++ = regaddr >> 8;
    *ptr++ = regaddr & 0xFF;

    for (int i = 0; i < word_count; i++) {
      *ptr++ = word_buffer[i] >> 8;
      *ptr++ = word_buffer[i] & 0xFF;
    }

    uint16_t byte_count = ptr - m_buffer;

    m_i2c.write_async(m_address, m_buffer, byte_count, true);
    while (!m_i2c.finished());
    if (m_i2c.has_error()) {
      Serial.print("[i2c-write-error]");
      return false;
    }
    return true;
  }
public:
  void begin() {
    m_i2c.begin(1000000U);
    read_eeprom();
    m_Mode = get_mode();
    m_RefreshRate = get_refresh_rate();
    m_Resolution = get_resolution();
  }
  bool busy() {
    return !m_i2c.finished();
  }
private:
  bool data_ready(uint16_t &subpage) {
    static uint16_t regaddr = MLX90640_STATUS1;
    uint16_t regvalue = 0;
    i2c_read_sync(regaddr, &regvalue);

    if (regvalue & 0x0008) {
      subpage = regvalue & 0x0001;
      return true;
    }
    return false;
  }
  void clear_data_ready() { // allow the device RAM to update
    const uint16_t regaddr = MLX90640_STATUS1;
    uint16_t regvalue = 0x0030;
    i2c_write_sync(regaddr, &regvalue);
  }
public:
  void set_mode(mlx_Mode mode) {
    const uint16_t regaddr = MLX90640_CONTROL1;
    uint16_t regvalue = 0;
    if (i2c_read_sync(regaddr, &regvalue)) {
      if (mode == MLX90640_CHESS) {
	regvalue |= 0x1000;
      } else {
	regvalue &= 0xEFFF;
      }
      i2c_write_sync(regaddr, &regvalue);
    }
    m_Mode = get_mode();
  }
  mlx_Mode get_mode() {
    const uint16_t regaddr = MLX90640_CONTROL1;
    uint16_t regvalue = 0;
    i2c_read_sync(regaddr, &regvalue);
    return (regvalue & 0x1000) ? MLX90640_CHESS : MLX90640_INTERLEAVED;
  }
  const char *mode_description(mlx_Mode mode) const;

  void set_refresh_rate(mlx_RefreshRate rate) {
    const uint16_t regaddr = MLX90640_CONTROL1;
    uint16_t regvalue = 0;
    i2c_read_sync(regaddr, &regvalue);
    regvalue &= ~0x0380;
    regvalue |= static_cast<uint16_t>(rate) << 7;
    i2c_write_sync(regaddr, &regvalue);
    m_RefreshRate = get_refresh_rate();
  }
  mlx_RefreshRate get_refresh_rate() {
    const uint16_t regaddr = MLX90640_CONTROL1;
    uint16_t regvalue = 0;
    i2c_read_sync(regaddr, &regvalue);
    regvalue = (regvalue & 0x0380) >> 7;
    return static_cast<mlx_RefreshRate>(regvalue);
  }
  const char *refresh_rate_description(mlx_RefreshRate rate) const;

  void set_resolution(mlx_Resolution resolution) {
    const uint16_t regaddr = MLX90640_CONTROL1;
    uint16_t regvalue = 0;
    i2c_read_sync(regaddr, &regvalue);
    regvalue &= ~0x0C00;
    regvalue |= static_cast<uint16_t>(resolution) << 10;
    i2c_write_sync(regaddr, &regvalue);
    m_Resolution = get_resolution();
  }
  mlx_Resolution get_resolution() {
    const uint16_t regaddr = MLX90640_CONTROL1;
    uint16_t regvalue = 0;
    i2c_read_sync(regaddr, &regvalue);
    regvalue = (regvalue & 0x0C00) >> 10;
    return static_cast<mlx_Resolution>(regvalue);
  }
  const char *resolution_description(mlx_Resolution resolution) const;

  const char *get_serial_number() {
    static char sno[13];

    const uint16_t regaddr = MLX90640_DEVICEID1;

    char *ptr = sno;
    uint16_t number[3];
    if (i2c_read_sync(regaddr, number, 3)) {
      for (int i = 0; i < 3; i++) {
	*ptr++ = s_hex[(number[i] >> 12) & 0x0F];
	*ptr++ = s_hex[(number[i] >>  8) & 0x0F];
	*ptr++ = s_hex[(number[i] >>  4) & 0x0F];
	*ptr++ = s_hex[(number[i]      ) & 0x0F];
      }
    }
    *ptr = 0;

    return sno;
  }
  void cycle_mode(bool cycling) {
    if (cycling && !m_bCycling) {
      m_row = 26;
      m_bCalcT = false;
    }
    m_bCycling = cycling;
  }
  bool cycle() { // returns true if temperatures updated
    bool bUpdated = false;

    if (!m_bCycling) return bUpdated;

    if (m_bCalcT) {
      m_bCalcT = false;
      calculate_temperatures();
      bUpdated = true;
    } else if (m_row == 26) { // we're waiting for new data
      uint16_t subpage = 0;
      if (data_ready(subpage)) {
	clear_data_ready();
	m_subpage = subpage;
	m_row = -1;    // now we're ready to collect
      }
    } else {
      if (++m_row == 26) {
	m_bCalcT = true; // this is the last read; next time calculate the temperatures
      }
      uint16_t *rowdata = m_raw  + (m_row << 5);
      uint16_t  regaddr = 0x0400 + (m_row << 5);
      i2c_read_sync(regaddr, rowdata, 32);
    }
    return bUpdated;
  }
};

#endif // ClassMLX_HH
