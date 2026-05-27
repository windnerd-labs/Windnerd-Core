/*
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Windnerd_TMAG5273.h"
#include "Windnerd_I2C.h"

#define I2C_ADDRESS 0x35

static WN_I2C _i2c(Wire);

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
  return _i2c.lastError();
}

void wn_init_angle_sensor(uint8_t scl_pin, uint8_t sda_pin)
{
  _i2c.begin(scl_pin, sda_pin);

  uint8_t rx[1];
  _i2c.readRegister(I2C_ADDRESS, SENSOR_CONFIG_1, rx, 1); // wake up the sensor in case it was asleep

  _i2c.writeRegister(I2C_ADDRESS, SENSOR_CONFIG_1, 0x50);
  _i2c.readRegister(I2C_ADDRESS, SENSOR_CONFIG_1, rx, 1);

  _i2c.writeRegister(I2C_ADDRESS, SENSOR_CONFIG_2, ANGLE_FROM_X_Z);
  _i2c.writeRegister(I2C_ADDRESS, DEVICE_CONFIG_1, SAMPLING_8X);
  _i2c.writeRegister(I2C_ADDRESS, INT_CONFIG_1, 1);

  _i2c.writeRegister(I2C_ADDRESS, DEVICE_CONFIG_2, OPERATING_MODE_SLEEP);
}

bool wn_read_then_make_angle_sensor_sleep(uint16_t *angle)
{
  uint8_t rx[1];
  _i2c.readRegister(I2C_ADDRESS, DEVICE_CONFIG_2, rx, 1); // wake up the sensor with a read operation
  delay(1);

  if (_i2c.writeRegister(I2C_ADDRESS, DEVICE_CONFIG_2, OPERATING_MODE_MEASURE) != WN_I2C_OK)
    return false;
  delay(10);

  uint8_t angle_result[2];
  if (_i2c.readRegister(I2C_ADDRESS, ANGLE_RESULT_MSB, angle_result, 2) != WN_I2C_OK)
  {
    // Best-effort attempt to leave the sensor asleep before bailing out.
    _i2c.writeRegister(I2C_ADDRESS, DEVICE_CONFIG_2, OPERATING_MODE_SLEEP);
    return false;
  }

  _i2c.writeRegister(I2C_ADDRESS, DEVICE_CONFIG_2, OPERATING_MODE_SLEEP);

  uint16_t raw_angle = (angle_result[0] << 8) + angle_result[1];
  *angle = (raw_angle & 0b0001111111111111) >> 4; // to 9 bits resolution
  return true;
}
