/* -*- mode: c++ -*-
 */
#include "ClassMLX.hh"

MLX s_cam(Master);

void setup() {
  while (!Serial);
  Serial.begin(115200);

  s_cam.begin();

  Serial.print("MLX90640 Serial Number: ");
  Serial.println(s_cam.get_serial_number());

  s_cam.set_mode(MLX90640_CHESS);
  Serial.print("Mode: ");
  Serial.println(s_cam.mode_description(s_cam.get_mode()));

  s_cam.set_refresh_rate(MLX90640_2_HZ);
  Serial.print("Refresh Rate: ");
  Serial.println(s_cam.refresh_rate_description(s_cam.get_refresh_rate()));

  s_cam.set_resolution(MLX90640_ADC_16BIT);
  Serial.print("Resolution: ");
  Serial.println(s_cam.resolution_description(s_cam.get_resolution()));

  s_cam.cycle_mode(true);
}

void loop() {
  if (s_cam.cycle()) {
    Serial.print("Ambient temperature: ");
    Serial.println(s_cam.get_ambient());
  }
}
