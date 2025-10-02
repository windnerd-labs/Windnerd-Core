/*
 * Copyright (c) 2025, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include "Arduino.h"
#include "Windnerd_Rolling_Buffer.h"
#include "Windnerd_Vector_Averager.h"

typedef struct
{
  float speed = 0;
  uint16_t dir = 0;
} wn_instant_wind_report_t;

typedef struct
{
  float avg_speed = 0;
  uint16_t avg_dir = 0;
  float min_speed = 0;
  float max_speed = 0;
} wn_wind_report_t;

typedef enum
{
  UNIT_MS = 0,
  UNIT_KN,
  UNIT_KPH,
  UNIT_MPH
} wn_wind_unit_t;

class WN_Core
{
public:
  // Constructor
  WN_Core();

  void loop(void);
  // set a callback function that will be triggered  every 3 sec for instant wind update
  void onInstantWindUpdate(void (*cb)(wn_instant_wind_report_t instant_report));
  void triggerInstantWindCb(wn_instant_wind_report_t &instant_report);

  // set a callback function that will be triggered every 1 minute for average wind update, the
  void onAvgWindUpdate(void (*cb)(wn_wind_report_t report));
  void triggerAvgWindCb(wn_wind_report_t &report);

  void begin();
  bool setAveragingPeriodInSec(uint16_t period);
  bool setUpdateIntervalInSec(uint16_t period);
  void setSpeedUnit(wn_wind_unit_t unit);
  void invertPolarity(bool should_invert);

private:
  float _HZ_to_ms;
  uint16_t _timeBetweenRefresh;
  uint8_t _speed_led_pin;
  uint8_t _north_led_pin;
  uint8_t _speed_input_pin;

  uint16_t _wind_average_period_sec;
  uint16_t _wind_update_period_sec;
  uint32_t ticks_cnt = 0; // ticks counter to be used as time base for periodic functions
  wn_wind_unit_t _unit_in_use = UNIT_MS;
  bool _invert_polarity = false;

  WN_ROLLINGBUFFER RollingBuffer;
  WN_VECTOR_AVERAGER VaneAverager;

  void (*instantWindCb)(wn_instant_wind_report_t instant_report) = nullptr;
  void (*avgWindCb)(wn_wind_report_t report) = nullptr;
  wn_wind_report_t formatRawReport(wn_raw_wind_report_t &raw_report);
  float pulsesToSpeedUnitInUse(uint32_t pulses);
  void signalIfNorth(uint16_t angle);
};
