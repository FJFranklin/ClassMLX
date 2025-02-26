/* -*- mode: c++ -*-
 * 
 * Copyright 2020-25 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include "config.hh"
#include <Shell.hh>
#include <ClassMLX.hh>

using namespace MultiShell;

#ifdef Central_BufferLength
#undef Central_BufferLength
#endif
#define Central_BufferLength 128 // print buffer length

ShellStream serial_zero(Serial);

#ifdef FEATHER_M0_BTLE
ShellStream serial_btle(ShellStream::m_bt_onboard);
#endif

Command sc_hello ("hello",      "hello",                        "Say hi :-)");
Command sc_irmode("mode",       "mode [Chess|Interleaved]",     "IRCam acquisition mode");
Command sc_irrate("rate",       "rate [0.5|1|2|4|8|16|32|64]",  "IRCam frame rate");
Command sc_irres ("resolution", "resolution [16-19]",           "IRCam bit resolution");
Command sc_sshot ("snapshot",   "snapshot [ambient|ascii|b64]", "IRCam: take a snapshot [default: ambient]");

class Task_IRCam : public Task {
private:
  const float* m_ir;
  const float* m_ptr;
  int m_row;
  int m_col;
public:
  Task_IRCam(const float* ir) :
    m_ir(ir),
    m_ptr(0),
    m_row(0),
    m_col(0)
  {
    // ...
  }
  virtual ~Task_IRCam() {
    // ...
  }

  inline void reset() {
    m_ptr = m_ir;
    m_row = 0;
    m_col = -1;
  }

  virtual bool process_task(ShellStream& stream, int& afw) { // returns true on completion of task
    static const char* Base64 = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ=%";

    while (afw > 2) { // check we can write at least two characters [or, weird glitch, *more* than 2 - FIXME?]
      if (m_col < 0) { // start of row
	stream.write('{', afw);
	stream.write(Base64[m_row], afw); // add a row indicator
	m_col = 0;
      } else if (m_col == 32) { // end of row
	stream.write('}', afw);
	stream.write(';', afw);
	++m_col;
      } else if (m_col == 33) { // add a line-break, and progress
	stream.write_eol(afw);
	if (m_row == 23) {
	  m_ptr = 0;
	  break;
	}      
	++m_row; // start the next row
	m_col = -1;
	break;   // voluntary pause - does this make a difference?
      } else {
	unsigned int index = m_row;
	index = index << 5 | m_col;
	int t = 16 * (m_ir[index] + 40); // 12-bit representation of temperature in range [-40,216] degC
	if (t < 0) t = 0;
	if (t > 4095) t = 4095;
	stream.write(Base64[t >> 6], afw);
	stream.write(Base64[t & 0x3F], afw);
	++m_col;
      }
    }
    return !m_ptr;
  }
};

class IRCam : public Timer, public ShellHandler, public ShellStream::Responder {
private:
  CommandList m_list;
  Shell  m_zero;
#ifdef FEATHER_M0_BTLE
  Shell  m_btle;
#endif
  Shell *m_last;

  MLX m_cam;

  Task_IRCam m_task_ir;
  TaskOwner<Task_IRCam> m_owner_ir;

  char m_buffer[Central_BufferLength]; // temporary print buffer
  ShellBuffer m_B;

  char m_ascii[24][33]; // matrix of characters for ASCII representation
  bool m_bCamData;

public:
  IRCam() :
    m_list(this),
    m_zero(serial_zero, m_list, 'u'),
#ifdef FEATHER_M0_BTLE
    m_btle(serial_btle, m_list, 'b'),
#endif
    m_last(0),
    m_cam(Master), // teensy 4, i2c channel 0
    m_task_ir(m_cam.get_frame()),
    m_B(m_buffer, Central_BufferLength),
    m_bCamData(false)
  {
    m_list.add(sc_hello);     // The handler for the list is set in the constructor above
    m_list.add(sc_irmode);
    m_list.add(sc_irrate);
    m_list.add(sc_irres);
    m_list.add(sc_sshot);

    m_zero.set_handler(this); // Need to set shell handler for CommaComms
#ifdef FEATHER_M0_BTLE
    m_btle.set_handler(this);
    serial_btle.set_responder(this); // connection messages
#endif
    for (int row = 0; row < 24; row++) {
      m_ascii[row][32] = 0;   // zero-terminate each row of the matrix
    }

    m_cam.begin();

    m_cam.set_mode(MLX90640_CHESS);
    m_cam.set_refresh_rate(MLX90640_2_HZ);
    m_cam.set_resolution(MLX90640_ADC_16BIT);

    m_cam.cycle_mode(true);

    m_owner_ir.push(m_task_ir, true); // give ownership of the ir task to the ir owner
  }
  virtual ~IRCam() {
    // ...
  }

  virtual void stream_notification(ShellStream& stream, const char *message) {
    if (!strcmp(message, "Connected")) {
      m_bCamData = true;
    }
    else if (!strcmp(message, "Disconnected")) {
      m_bCamData = false;
    }
#ifdef ENABLE_FEEDBACK
    else if (Serial) {
      Serial.print(stream.name());
      Serial.print(": notify: ");
      Serial.println(message);
    }
#endif
  }

  virtual void shell_notification(Shell& origin, const char *message) {
#ifdef ENABLE_FEEDBACK
    if (Serial) {
      Serial.print(origin.name());
      Serial.print(": notify: ");
      Serial.println(message);
    }
#endif
  }
  virtual void comma_command(Shell& origin, CommaCommand& command) {
#ifdef ENABLE_FEEDBACK
    if (Serial) {
      Serial.print(origin.name());
      Serial.print(": command: ");
      Serial.print(command.m_command);
      Serial.print(": ");
      Serial.println(command.m_value);
    }
#endif
    unsigned long value = command.m_value;

    switch(command.m_command) {
    case 'c':
      m_bCamData = value;
      break;
    default:
      break;
    }
  }

  virtual void every_milli() { // runs once a millisecond, on average
    if (m_cam.cycle()) {
      // Serial.print("Ambient temperature: ");
      // Serial.println(m_cam.get_ambient());
    }
    tick(); // force a tick, in case swamped
  }

  virtual void every_10ms() { // runs once every 10ms, on average
    // ...
  }

  virtual void every_tenth(int tenth) { // runs once every tenth of a second, where tenth = 0..9
    digitalWrite(LED_BUILTIN, tenth == 0 || tenth == 8 || (tenth == 9 && m_bCamData));

#ifdef FEATHER_M0_BTLE
    m_btle.update();
  /*if (m_bCamData && !(tenth & 0x01)) {
      m_btle << "The quick brown fox jumps over the lazy dog. 0123456789ABCDEF" << 0;
    }*/
#endif
  }

  virtual void every_second() { // runs once every second
    // ...
  }

  virtual void tick() {
    m_zero.update();
  }

  virtual CommandError shell_command(Shell& origin, Args& args) {
    CommandError ce = ce_Okay;

    m_last = &origin;

    if (args == "hello") {
      origin << "Hi!" << 0;
    }
    else if (args == "resolution") {
      mlx_Resolution cur_res = m_cam.get_resolution();
      mlx_Resolution new_res = cur_res;
      ++args;
      if (args == "16") {
	new_res = MLX90640_ADC_16BIT;
      } else if (args == "17") {
	new_res = MLX90640_ADC_17BIT;
      } else if (args == "18") {
	new_res = MLX90640_ADC_18BIT;
      } else if (args == "19") {
	new_res = MLX90640_ADC_19BIT;
      }
      if (new_res != cur_res) {
	m_cam.set_resolution(new_res);
	cur_res = m_cam.get_resolution();
      }
      origin << "IRCam: Resolution = ";
      switch (cur_res) {
      case MLX90640_ADC_16BIT: origin << "16 bit" << 0; break;
      case MLX90640_ADC_17BIT: origin << "17 bit" << 0; break;
      case MLX90640_ADC_18BIT: origin << "18 bit" << 0; break;
      case MLX90640_ADC_19BIT: origin << "19 bit" << 0; break;
      }
    }
    else if (args == "rate") {
      mlx_RefreshRate rate = m_cam.get_refresh_rate();
      mlx_RefreshRate rnew = rate;
      ++args;
      if (args == "0.5") {
	rnew = MLX90640_0_5_HZ;
      } else if (args == "1") {
	rnew = MLX90640_1_HZ;
      } else if (args == "2") {
	rnew = MLX90640_2_HZ;
      } else if (args == "4") {
	rnew = MLX90640_4_HZ;
      } else if (args == "8") {
	rnew = MLX90640_8_HZ;
      } else if (args == "16") {
	rnew = MLX90640_16_HZ;
      } else if (args == "32") {
	rnew = MLX90640_32_HZ;
      } else if (args == "64") {
	rnew = MLX90640_64_HZ;
      }
      if (rnew != rate) {
	m_cam.set_refresh_rate(rnew);
	rate = m_cam.get_refresh_rate();
      }
      origin << "IRCam: Frame rate = ";
      switch (rate) {
      case MLX90640_0_5_HZ: origin << "0.5 Hz" << 0; break;
      case MLX90640_1_HZ:   origin << "1 Hz"   << 0; break;
      case MLX90640_2_HZ:   origin << "2 Hz"   << 0; break;
      case MLX90640_4_HZ:   origin << "4 Hz"   << 0; break;
      case MLX90640_8_HZ:   origin << "8 Hz"   << 0; break;
      case MLX90640_16_HZ:  origin << "16 Hz"  << 0; break;
      case MLX90640_32_HZ:  origin << "32 Hz"  << 0; break;
      case MLX90640_64_HZ:  origin << "64 Hz"  << 0; break;
      }
    }
    else if (args == "mode") {
      ++args;
      if (args == "Chess") {
	m_cam.set_mode(MLX90640_CHESS);
      } else if (args == "Interleaved") {
	m_cam.set_mode(MLX90640_INTERLEAVED);
      }
      origin << "IRCam: Mode = ";
      if (m_cam.get_mode() == MLX90640_CHESS) {
	origin << "Chess" << 0;
      } else {
	origin << "Interleaved" << 0;
      }
    }
    else if (args == "snapshot") {
      elapsedMicros timer;
      ++args;
      if (args == "b64") {
	Task_IRCam *ir = m_owner_ir.pop();
	if (ir) {
	  ir->reset();
	  origin << *ir;
	}
      } else if (args == "ascii") {
	const float *frame = m_cam.get_frame();
	for (uint8_t h=0; h<24; h++) {
	  for (uint8_t w=0; w<32; w++) {
	    float t = frame[h*32 + w];
	    char c = '&';
	    if (t < 20) c = ' ';
	    else if (t < 23) c = '.';
	    else if (t < 25) c = '-';
	    else if (t < 27) c = '*';
	    else if (t < 29) c = '+';
	    else if (t < 31) c = 'x';
	    else if (t < 33) c = '%';
	    else if (t < 35) c = '#';
	    else if (t < 37) c = 'X';
	    m_ascii[h][w] = c;
	  }
	  origin << m_ascii[h] << 0;
	}
      } else {
	float ambient = m_cam.get_ambient();
	m_B.clear();
	m_B.printf("IRCam: Ambient temperature = %.1f degC.", ambient);
	origin << m_B << 0;
      }
    }

    return ce;
  }
};

void setup() {
  delay(500);

  serial_zero.begin(115200); // Shell on USB

#ifdef FEATHER_M0_BTLE
  const char* status = 0;
  serial_btle.begin(status); // Shell on Bluetooth
  if (status && Serial) {
    Serial.println(status);
  }
#endif

  pinMode(LED_BUILTIN, OUTPUT);

  IRCam().run();
}

void loop() {
  // ...
}
