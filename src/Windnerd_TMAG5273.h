/*
 * Copyright (c) 2025, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include "Arduino.h"

void wn_init_angle_sensor(uint8_t scl_pin, uint8_t sda_pin);
uint16_t wn_read_then_make_angle_sensor_sleep();
uint8_t wn_get_last_angle_sensor_i2c_error();
