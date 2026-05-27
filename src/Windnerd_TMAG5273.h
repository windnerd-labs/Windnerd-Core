/*
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include "Arduino.h"

void wn_init_angle_sensor(uint8_t scl_pin, uint8_t sda_pin);

// Wakes the sensor, performs a measurement, puts it back to sleep, and writes
// the angle (0..359, 9-bit resolution scaled to degrees by the caller's mapping)
// to `*angle`. Returns true on success. On I2C failure returns false and leaves
// `*angle` unmodified — callers must check the return value before using it.
bool wn_read_then_make_angle_sensor_sleep(uint16_t *angle);

uint8_t wn_get_last_angle_sensor_i2c_error();
