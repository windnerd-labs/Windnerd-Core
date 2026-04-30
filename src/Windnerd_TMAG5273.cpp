/*
 * Copyright (c) 2025, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Windnerd_TMAG5273.h"
#include <Wire.h>

#define I2C_ADDRESS 0x35

static uint8_t _scl_pin = 0;
static uint8_t _sda_pin = 0;
static uint8_t _i2c_error = 0;


// Bitbang the I2C bus to release a slave stuck holding SDA low. Pulses SCL up
// to 9 times so the slave can finish the in-flight byte, then issues a STOP
// and re-inits Wire. Relies on external pull-ups (lines released = high).
static void wn_recover_i2c_bus()
{
  Wire.end();

  pinMode(_scl_pin, INPUT_PULLUP);
  pinMode(_sda_pin, INPUT_PULLUP);
  delayMicroseconds(10);

  for (uint8_t i = 0; i < 9; i++)
  {
    if (digitalRead(_sda_pin) == HIGH) break;
    pinMode(_scl_pin, OUTPUT);
    digitalWrite(_scl_pin, LOW);
    delayMicroseconds(5);
    pinMode(_scl_pin, INPUT_PULLUP);
    delayMicroseconds(5);
  }

  // STOP condition: SDA rises while SCL is high
  pinMode(_sda_pin, OUTPUT);
  digitalWrite(_sda_pin, LOW);
  delayMicroseconds(5);
  pinMode(_scl_pin, INPUT_PULLUP);
  delayMicroseconds(5);
  pinMode(_sda_pin, INPUT_PULLUP);
  delayMicroseconds(5);

  Wire.setSDA(_sda_pin);
  Wire.setSCL(_scl_pin);
  Wire.begin();
}

// registers
#define DEVICE_CONFIG_1 0x00
#define DEVICE_CONFIG_2 0x01
#define SENSOR_CONFIG_1 0x02
#define SENSOR_CONFIG_2 0x03
#define INT_CONFIG_1 0x08
#define ANGLE_RESULT_MSB 0x19

// values
#define SAMPLING_8X 0b00001100
#define ANGLE_FROM_X_Z 0b00001100

typedef enum operating_mode
{
  OPERATING_MODE_STANDBY = 0x0,
  OPERATING_MODE_SLEEP,
  OPERATING_MODE_MEASURE,
  OPERATING_MODE_WS
} operating_mode_t;


uint8_t wn_get_last_angle_sensor_i2c_error()
{
  return _i2c_error;
}

void wn_write_angle_sensor_register(uint8_t reg, uint8_t data)
{
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(reg);
  Wire.write(data);
  _i2c_error = Wire.endTransmission();

  if (_i2c_error != 0)
  {
    wn_recover_i2c_bus();
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(reg);
    Wire.write(data);
    _i2c_error = Wire.endTransmission();
  }
}

void wn_read_angle_sensor_register(uint8_t reg, uint8_t *data, size_t length)
{
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(reg);
  _i2c_error = Wire.endTransmission();
  if (_i2c_error != 0)
  {
    wn_recover_i2c_bus();
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(reg);
    _i2c_error = Wire.endTransmission();
    if (_i2c_error != 0) return;
  }

  if (Wire.requestFrom(I2C_ADDRESS, length) != length)
  {
    _i2c_error = 6; // short/no read — outside Wire's 0..5 endTransmission codes
    wn_recover_i2c_bus();
    return;
  }

  for (uint8_t i = 0; i < length && Wire.available(); i++)
  {
    data[i] = Wire.read();
  }
}

void wn_init_angle_sensor(uint8_t scl_pin, uint8_t sda_pin)
{
  _scl_pin = scl_pin;
  _sda_pin = sda_pin;

  Wire.begin();

  uint8_t rx[1];
  wn_read_angle_sensor_register(SENSOR_CONFIG_1, rx, 1); // read a register to wake up the sensor in case it was asleep

  wn_write_angle_sensor_register(SENSOR_CONFIG_1, 0x50);
  wn_read_angle_sensor_register(SENSOR_CONFIG_1, rx, 1);

  wn_write_angle_sensor_register(SENSOR_CONFIG_2, ANGLE_FROM_X_Z);
  wn_write_angle_sensor_register(DEVICE_CONFIG_1, SAMPLING_8X);
  wn_write_angle_sensor_register(INT_CONFIG_1, 1);

  wn_write_angle_sensor_register(DEVICE_CONFIG_2, OPERATING_MODE_SLEEP);
}

uint16_t wn_read_then_make_angle_sensor_sleep()
{
  uint8_t rx[1];
  wn_read_angle_sensor_register(DEVICE_CONFIG_2, rx, 1); // wake up the sensor with a read operation
  delay(1);
  wn_write_angle_sensor_register(DEVICE_CONFIG_2, OPERATING_MODE_MEASURE);
  delay(10);

  uint8_t angle_result[2];
  wn_read_angle_sensor_register(ANGLE_RESULT_MSB, angle_result, 2);

  wn_write_angle_sensor_register(DEVICE_CONFIG_2, OPERATING_MODE_SLEEP);

  uint16_t raw_angle = (angle_result[0] << 8) + angle_result[1]; // combine 2 bytes as a 16 bits variable
  return (raw_angle & 0b0001111111111111) >> 4;                  // to 9 bits resolution
}
