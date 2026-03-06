/*
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include "Arduino.h"
#include "Windnerd_Core.h"


typedef struct {
  bool has_temperature = false;
  bool has_humidity = false;
  bool has_pressure = false;
  bool has_voltage = false;
  bool has_rssi = false;
  bool has_wind_samples = false;
  bool hmac_enabled = false;

} wn_payload_config_t;

class WN_WTP_PAYLOAD {

public:
  WN_WTP_PAYLOAD();

  void setTemperature(float temperature);
  void setHumidity(float humidity);
  void setPressure(float pressure);
  void setVoltage(float voltage);
  void setRSSI(float rssi);
  void setAnemometer(WN_Core* anemometer);
  void enableWindSamples();
  void setPeriodInMinutes(unsigned int period_mn);
  void setSecretKey(char* secret_key);
  void sendPayload(Print* modem, Print* debug = NULL);
  void reset();

  unsigned int calculatePayloadLength();

private:
  wn_payload_config_t _payload_config;
  WN_Core* _anemometer;
  float _temperature;
  float _humidity;
  float _pressure;
  float _voltage;
  float _rssi;
  unsigned int _period_mn = 1;
  char* _secret_key;
  void composeAndSendReportLine(unsigned int line_index, Print* modem, Print* debug);
  void composeAndSendSampleLine(unsigned int line, Print* modem, Print* debug);
};
