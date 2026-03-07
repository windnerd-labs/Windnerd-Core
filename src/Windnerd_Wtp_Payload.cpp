/*
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */
#include "Windnerd_Wtp_Payload.h"


WN_WTP_PAYLOAD::WN_WTP_PAYLOAD() {
}

void WN_WTP_PAYLOAD::reset() {
  _payload_config = {};
}
void WN_WTP_PAYLOAD::setAnemometer(WN_Core* anemometer) {
  _anemometer = anemometer;
}

void WN_WTP_PAYLOAD::setPeriodInMinutes(unsigned period) {
  _period_mn = period;
}

void WN_WTP_PAYLOAD::enableWindSamples() {
  _payload_config.has_wind_samples = true;
}

void WN_WTP_PAYLOAD::setTemperature(float temperature) {
  _temperature = temperature;
  _payload_config.has_temperature = true;
}

void WN_WTP_PAYLOAD::setHumidity(float humidity) {
  _humidity = humidity;
  _payload_config.has_humidity = true;
}

void WN_WTP_PAYLOAD::setPressure(float pressure) {
  _pressure = pressure;
  _payload_config.has_pressure = true;
}

void WN_WTP_PAYLOAD::setVoltage(float voltage) {
  _voltage = voltage;
  _payload_config.has_voltage = true;
}

void WN_WTP_PAYLOAD::setRSSI(float rssi) {
  _rssi = rssi;
  _payload_config.has_rssi = true;
}

void WN_WTP_PAYLOAD::setSecretKey(char* secret_key) {
  _secret_key = secret_key;
}

#define SPEED_MAX_LENGTH 4  // 99.9 m/s
#define DIR_MAX_LENGTH 4    // 359

#define TEMP_MAX_LENGTH 5      //-99.0 C/F
#define HUM_MAX_LENGTH 3       // 100
#define PRESSURE_MAX_LENGTH 6  // 1099.9
#define VOLTAGE_MAX_LENGTH 4   // 4.30
#define RSSI_MAX_LENGTH 5      // -99.9


unsigned int WN_WTP_PAYLOAD::calculatePayloadLength() {

  unsigned int payload_length = 0;

  payload_length += 3 + strlen(_secret_key);  //k=;

  const unsigned int wind_report_length = (4 + SPEED_MAX_LENGTH) * 3 + (4 + DIR_MAX_LENGTH) + 2;  // r;

  // first report line with extra data
  payload_length += wind_report_length;
  if (_payload_config.has_temperature) {
    payload_length += TEMP_MAX_LENGTH + 4;
  }
  if (_payload_config.has_humidity) {
    payload_length += HUM_MAX_LENGTH + 4;
  }
  if (_payload_config.has_pressure) {
    payload_length += PRESSURE_MAX_LENGTH + 4;
  }

  if (_payload_config.has_voltage) {
    payload_length += VOLTAGE_MAX_LENGTH + 4;
  }

  if (_payload_config.has_rssi) {
    payload_length += RSSI_MAX_LENGTH + 4;
  }

  // following report lines (only wind)
  payload_length += (_period_mn - 1) * wind_report_length;


  if (_payload_config.has_wind_samples) {
    payload_length += _period_mn * 20 * (SPEED_MAX_LENGTH + DIR_MAX_LENGTH + 10);
  }

  return payload_length;
}


// compose and print 1 or more wind reports via WTP
void WN_WTP_PAYLOAD::composeAndSendReportLine(unsigned int line_index, Print* modem, Print* debug) {

  String line = "r";
  wn_wind_report_t report = _anemometer->computeReportForPeriodInSecIndexedFromLast(60, line_index);

  char wa[SPEED_MAX_LENGTH + 1], wn[SPEED_MAX_LENGTH + 1], wx[SPEED_MAX_LENGTH + 1], wd[DIR_MAX_LENGTH + 1];
  dtostrf(report.avg_speed, SPEED_MAX_LENGTH, 1, wa);
  dtostrf(report.min_speed, SPEED_MAX_LENGTH, 1, wn);
  dtostrf(report.max_speed, SPEED_MAX_LENGTH, 1, wx);
  dtostrf(report.avg_dir, DIR_MAX_LENGTH, 0, wd);
  line += ",wa=";
  line += wa;
  line += ",wd=";
  line += wd;
  line += ",wn=";
  line += wn;
  line += ",wx=";
  line += wx;

  if (line_index == 0) {
    if (_payload_config.has_temperature) {
      char temperature[TEMP_MAX_LENGTH + 1];
      dtostrf(_temperature, TEMP_MAX_LENGTH, 1, temperature);
      line += ",tp=";
      line += temperature;
    }
    if (_payload_config.has_humidity) {
      char humidity[HUM_MAX_LENGTH + 1];
      dtostrf(_humidity, HUM_MAX_LENGTH, 0, humidity);
      line += ",hu=";
      line += humidity;
    }
    if (_payload_config.has_pressure) {
      char pressure[PRESSURE_MAX_LENGTH + 1];
      dtostrf(_pressure, PRESSURE_MAX_LENGTH, 1, pressure);
      line += ",pr=";
      line += pressure;
    }
    if (_payload_config.has_voltage) {
      char voltage[VOLTAGE_MAX_LENGTH + 1];
      dtostrf(_voltage, VOLTAGE_MAX_LENGTH, 2, voltage);
      line += ",vo=";
      line += voltage;
    }
    if (_payload_config.has_rssi) {
      char rssi[RSSI_MAX_LENGTH + 1];
      dtostrf(_rssi, RSSI_MAX_LENGTH, 1, rssi);
      line += ",rs=";
      line += rssi;
    }
  }

  line += ";";

  modem->print(line);

  if (debug) {
    debug->print("Sent to modem: ");
    debug->println(line);
  }
}



// compose a message to send aggregated instant wind samples via WTP
void WN_WTP_PAYLOAD::composeAndSendSampleLine(unsigned int line, Print* modem, Print* debug) {
  wn_instant_wind_sample_t sample = _anemometer->getSampleIndexedFromLast(line);
  char wi[SPEED_MAX_LENGTH + 1], wd[DIR_MAX_LENGTH + 1];
  dtostrf(sample.speed, SPEED_MAX_LENGTH, 1, wi);
  dtostrf(sample.dir, DIR_MAX_LENGTH, 0, wd);
  char buffer[32];
  sprintf(buffer, "s,wi=%s,wd=%s;", wi, wd);
  modem->print(buffer);
  if (debug) {
    debug->print("Sent to modem: ");
    debug->println(buffer);
  }
}


void WN_WTP_PAYLOAD::sendPayload(Print* modem, Print* debug) {

  char buffer[64];
  sprintf(buffer, "k=%s;", _secret_key);
  modem->print(buffer);

  if (debug) {
    debug->print("Sent to modem: ");
    debug->println(buffer);
  }

  for (unsigned i = 0; i < _period_mn; i++) {
    composeAndSendReportLine(i, modem, debug);
  }

  if (_payload_config.has_wind_samples) {
    for (unsigned i = 0; i < _period_mn * 20; i++) {
      composeAndSendSampleLine(i, modem, debug);
    }
  }
}
