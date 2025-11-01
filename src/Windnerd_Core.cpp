/*
 * Copyright (c) 2025, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Windnerd_Core.h"
#include "Windnerd_TMAG5273.h"
#include <HardwareTimer.h>

HardwareTimer *tickerTimer = nullptr;

// LED pins for WindNerd Core board
#define SPEED_LED PA7
#define NORTH_LED PB3

// Pulse/speed input pin for WindNerd Core board
#define SPEED_INPUT PC15

// frequency to speed ratio for standard rotor
#define HZ_TO_MS 1.31

// default wind vector averaging period in seconds
#define DEFAULT_AVG_PERIOD_SEC 60
// default time between wind avg update in seconds
#define DEFAULT_UPDATE_PERIOD_SEC 60

// 30 ticks at 10 HZ -> speed pulses are counted for each 3 sec periods
#define TICK_HZ 10
#define SAMPLING_WINDOW_TICKS 30
#define SAMPLE_DURATION (SAMPLING_WINDOW_TICKS / TICK_HZ)

// in low power mode, measure vane angle every 500 ms
#define LOW_POWER_VANE_TICKS 5

static volatile bool wn_ticker = false;         // flag to indicate that a tick interrupt has happened
static volatile uint32_t speed_pulse_count = 0; // to be incremented by rising edge interrupts on speed pulse input
static volatile bool low_power_mode = false;

void onSpeedPulseISR()
{
  if (!low_power_mode)
    digitalWrite(SPEED_LED, HIGH); // signal pulse by turning the speed LED ON, it will be turned OFF during the next tick
  speed_pulse_count++;
}

void onTickerTimerISR()
{
  wn_ticker = true;
}

WN_Core::WN_Core()
{
  _speed_led_pin = SPEED_LED;
  _north_led_pin = NORTH_LED;
  _speed_input_pin = SPEED_INPUT;
  _HZ_to_ms = HZ_TO_MS;
  _wind_average_period_sec = DEFAULT_AVG_PERIOD_SEC;
  _wind_update_period_sec = DEFAULT_UPDATE_PERIOD_SEC;
}

void WN_Core::begin()
{

  // turn on all LEDs so the board shows life a startup
  pinMode(_speed_led_pin, OUTPUT);
  digitalWrite(_speed_led_pin, HIGH);

  pinMode(_north_led_pin, OUTPUT);
  digitalWrite(_north_led_pin, HIGH);

  wn_init_angle_sensor();

  tickerTimer = new HardwareTimer(TIM3);
  tickerTimer->setOverflow(TICK_HZ, HERTZ_FORMAT);
  tickerTimer->attachInterrupt(onTickerTimerISR);
  tickerTimer->resume();

  pinMode(_speed_input_pin, INPUT);
  attachInterrupt(digitalPinToInterrupt(SPEED_INPUT), onSpeedPulseISR, RISING);
  last_sampling_window_millis = millis();
}

// set the averaging period for average wind report
bool WN_Core::setAveragingPeriodInSec(uint16_t period)
{
  if (period >= SAMPLE_DURATION && period <= ROLLING_BUFFER_LENGTH * SAMPLE_DURATION)
  {
    _wind_average_period_sec = period;
    return true;
  }
  else
  {
    return false;
  }
}

// set the time interval between average wind reports
bool WN_Core::setReportingIntervalInSec(uint16_t period)
{
  if (period >= SAMPLE_DURATION)
  {
    _wind_update_period_sec = period;
    return true;
  }
  else
  {
    return false;
  }
}

void WN_Core::invertVanePolarity(bool should_invert)
{
  _invert_polarity = should_invert;
}

// turn on North led when vane is roughly north
void WN_Core::signalIfNorth(uint16_t angle)
{

  if (!low_power_mode && (angle > 355 || angle < 5))
  {
    digitalWrite(_north_led_pin, HIGH);
  }
  else
  {
    digitalWrite(_north_led_pin, LOW);
  }
}

void WN_Core::loop()
{

  if (!wn_ticker)
    return;
  wn_ticker = false;

  ticks_cnt++;

  if (!low_power_mode || ticks_cnt % LOW_POWER_VANE_TICKS == 0)
  {

    uint16_t angle = wn_read_then_make_angle_sensor_sleep();

    if (_invert_polarity)
    {
      angle = angle + 180;
    }

    angle = angle % 360; // cap value from 0 to 359
    signalIfNorth(angle);

    // accumulate with an arbitrary magnitude, we are interested only in direction avg
    VaneAverager.accumulate((uint32_t)1, angle);
  }

  // reset the speed led for flash effect
  digitalWrite(_speed_led_pin, LOW);

  if (ticks_cnt % SAMPLING_WINDOW_TICKS == 0)
  { // counting window has elapsed

    // check timing, drop the sample if one or more ticks were missed (would be likely caused by a blocking delay in user program loop)
    if (millis() - last_sampling_window_millis < SAMPLE_DURATION * 1000 + 1000 / TICK_HZ)
    {
      // we average the wind direction during that time and store the data point in a circular/rolling buffer
      wn_raw_wind_report_t vane_raw_report;
      VaneAverager.computeReportFromAccumulatedValues(&vane_raw_report);
      wn_raw_wind_sample_t raw_sample = {speed_pulse_count, vane_raw_report.dir_avg, true};

      // reset pulse counter as soon as sample is recorded
      speed_pulse_count = 0;
      last_sampling_window_millis = millis();

      RollingBuffer.addRawSample(raw_sample);

      wn_instant_wind_sample_t instant_wind_sample = formatRawSample(raw_sample);
      // trigger the instant wind callback set by user
      triggerInstantWindCb(instant_wind_sample);
    }
    else
    {
      speed_pulse_count = 0;
      last_sampling_window_millis = millis();
    }
  }

  if (ticks_cnt % (_wind_update_period_sec * TICK_HZ) == 0)
  { // time interval between wind avg updates has elapsed
    wn_wind_report_t report = computeReportForRecentPeriodInSec(_wind_average_period_sec);
    triggerAvgWindCb(report);
  }
}

wn_wind_report_t WN_Core::computeReportForRecentPeriodInSec(uint16_t period)
{
  return computeReportForPeriodInSecIndexedFromLast(period, 0);
}

wn_instant_wind_sample_t WN_Core::getSampleIndexedFromLast(uint16_t index)
{
  wn_raw_wind_sample_t raw_sample = RollingBuffer.get(index);
  return formatRawSample(raw_sample);
}

wn_wind_report_t WN_Core::computeReportForPeriodInSecIndexedFromLast(uint16_t period, uint16_t index)
{

  uint16_t samples_to_average = period / (SAMPLING_WINDOW_TICKS / TICK_HZ); // how many samples should be read depends on the average period set
  uint16_t shift = (index * period) / (SAMPLING_WINDOW_TICKS / TICK_HZ);
  // read last samples from circular/rolling buffer and accumulate their cartesian coordinates
  WN_VECTOR_AVERAGER periodAverager;
  for (uint16_t i = 0 + shift; i < samples_to_average + shift; i++)
  {
    wn_raw_wind_sample_t sample = RollingBuffer.get(i);
    if (sample.valid)
    {
      periodAverager.accumulate(sample);
    }
  }

  // compute 2D averaging, min, max for period
  wn_raw_wind_report_t avg_raw_wind_report;
  periodAverager.computeReportFromAccumulatedValues(&avg_raw_wind_report);

  // convert them to speed and trigger the averaging wind callback set by user
  wn_wind_report_t report = formatRawReport(avg_raw_wind_report);
  return report;
}

// set the callback function that will be called when new instant wind update is available
void WN_Core::onInstantWindUpdate(void (*cb)(wn_instant_wind_sample_t instant_report))
{
  instantWindCb = cb;
}

// set the callback function that will be called when new average wind report is available
void WN_Core::onNewWindReport(void (*cb)(wn_wind_report_t report))
{
  avgWindCb = cb;
}

void WN_Core::triggerInstantWindCb(wn_instant_wind_sample_t &instant_report)
{
  if (instantWindCb)
  {
    instantWindCb(instant_report); // call only if set
  }
}

void WN_Core::triggerAvgWindCb(wn_wind_report_t &report)
{
  if (avgWindCb)
  {
    avgWindCb(report); // call only if set
  }
}

wn_instant_wind_sample_t WN_Core::formatRawSample(wn_raw_wind_sample_t &raw_sample)
{
  wn_instant_wind_sample_t sample;

  sample.speed = pulsesToSpeedUnitInUse(raw_sample.pulses);
  sample.dir = raw_sample.dir;
  return sample;
}

wn_wind_report_t WN_Core::formatRawReport(wn_raw_wind_report_t &raw_report)
{
  wn_wind_report_t report;
  report.avg_dir = raw_report.dir_avg;
  report.avg_speed = pulsesToSpeedUnitInUse(raw_report.pulses_avg);
  report.min_speed = pulsesToSpeedUnitInUse(raw_report.pulses_min);
  report.max_speed = pulsesToSpeedUnitInUse(raw_report.pulses_max);
  return report;
}

void WN_Core::setSpeedUnit(wn_wind_unit_t unit)
{
  _unit_in_use = unit;
}

float WN_Core::pulsesToSpeedUnitInUse(float pulses)
{

  float speed_ms = pulses * _HZ_to_ms / (SAMPLING_WINDOW_TICKS / TICK_HZ);
  switch (_unit_in_use)
  {
  case UNIT_MS:
    return speed_ms;
  case UNIT_KN: // knots
    return speed_ms * 1.94384f;
  case UNIT_KPH: // kilometers per hour
    return speed_ms * 3.6f;
  case UNIT_MPH: // miles per hour
    return speed_ms * 2.23694f;
  default:
    return speed_ms; // fallback to m/s
  }
}

void WN_Core::enableLowPowerMode()
{
  low_power_mode = true;
}

void WN_Core::disableLowPowerMode()
{
  low_power_mode = false;
}

bool WN_Core::isLowPowerMode()
{
  return low_power_mode;
}