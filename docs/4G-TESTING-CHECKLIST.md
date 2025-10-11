# 4G Air780E Integration - Testing Checklist

This document provides a testing and validation checklist for the Luat Air780E 4G board integration with WindNerd Core.

## Hardware Testing

### Initial Setup
- [ ] Air780E 4G module is correctly connected to WindNerd Core
  - [ ] PA0 connected to PWRKEY
  - [ ] TX2 (USART2) connected to Air780E RXD
  - [ ] RX2 (USART2) connected to Air780E TXD
  - [ ] GND connected to GND
  - [ ] Power supply (3.3V or 5V) connected to VBAT
- [ ] SIM card is inserted in Air780E module
- [ ] Antenna is connected to Air780E module
- [ ] Power supply can provide at least 500mA for 4G transmission

### Power Control
- [ ] Air780E powers up when power-on pulse is sent
- [ ] Air780E powers down when power-off pulse is sent
- [ ] Air780E enters sleep mode with AT+CSCLK=2 command
- [ ] Power consumption in sleep mode is < 5mA total system

### Communication
- [ ] UART communication established at 115200 baud
- [ ] AT commands receive "OK" responses
- [ ] AT+CREG? shows network registration
- [ ] Data transmission completes successfully
- [ ] SerialDebug (115200 on USART1) shows debug messages

## Software Testing

### Wind Data Collection
- [ ] Wind speed data is captured correctly
- [ ] Wind direction data is captured correctly
- [ ] Data is stored in latestWind structure
- [ ] Timestamp is recorded with each reading
- [ ] Valid flag is set when data is available

### Timing
- [ ] System wakes 4G module every 15 minutes
- [ ] Timing is accurate (check with SerialDebug timestamps)
- [ ] Timer doesn't drift over extended operation
- [ ] First transmission occurs after initial 15-minute delay

### Data Transmission
- [ ] Wind data is formatted correctly
- [ ] Data string follows format: `WIND,<speed>,<dir>,<timestamp>`
- [ ] Transmission completes within expected time (< 30 seconds)
- [ ] 4G module returns to sleep after transmission

### Error Handling
- [ ] System handles 4G module not responding
- [ ] System handles network registration failure
- [ ] System continues operation if transmission fails
- [ ] Debug messages indicate error conditions clearly

## Functional Testing

### Basic Operation
1. **Upload Sketch**
   - [ ] Sketch compiles without errors
   - [ ] Upload completes successfully
   - [ ] Board starts and initializes

2. **Monitor Serial Debug**
   - [ ] Open Serial Monitor at 115200 baud
   - [ ] "WindNerd Core with 4G Air780E - Starting..." appears
   - [ ] "4G module initialized" appears
   - [ ] "System ready" appears

3. **Wind Data Collection**
   - [ ] Wind data appears every 3 seconds
   - [ ] Average wind reports appear every minute
   - [ ] Data values are reasonable

4. **First Transmission** (wait 15 minutes)
   - [ ] "Waking up 4G module..." appears
   - [ ] "4G module started" appears
   - [ ] "Sent: AT" appears
   - [ ] "Response: OK" appears
   - [ ] "Data to send: WIND,..." appears
   - [ ] "Data transmitted" appears
   - [ ] "Putting 4G module to sleep..." appears

5. **Subsequent Transmissions**
   - [ ] Transmissions occur every 15 minutes
   - [ ] Data is current (not stale)
   - [ ] Timing is consistent

### Extended Operation
- [ ] System runs continuously for 1 hour
- [ ] System runs continuously for 24 hours
- [ ] No memory leaks or crashes observed
- [ ] Power consumption remains stable

## Integration Testing

### With Different Networks
- [ ] Works with 2G network (if supported by Air780E)
- [ ] Works with 3G network
- [ ] Works with 4G LTE network
- [ ] Works with different carriers/operators

### With Different Data Protocols
- [ ] Raw data transmission works
- [ ] HTTP POST transmission works (if implemented)
- [ ] MQTT transmission works (if implemented)
- [ ] TCP socket transmission works (if implemented)

### Environmental Conditions
- [ ] Operates in temperature range -10°C to 50°C
- [ ] Operates in high humidity conditions
- [ ] Operates in areas with weak signal
- [ ] Handles signal loss and recovery

## Performance Testing

### Power Consumption
- [ ] Measure and record power in sleep mode
- [ ] Measure and record power during transmission
- [ ] Calculate average power over 15-minute cycle
- [ ] Verify battery life estimates

### Transmission Reliability
- [ ] 100 transmissions with 100% success rate
- [ ] Network errors are handled gracefully
- [ ] Retry logic works (if implemented)

### Data Accuracy
- [ ] Compare transmitted data with SerialDebug output
- [ ] Verify wind speed values are correct
- [ ] Verify wind direction values are correct
- [ ] Verify timestamps are sequential

## Documentation Testing

### User Documentation
- [ ] README.md is clear and accurate
- [ ] 4G-INTEGRATION.md covers all features
- [ ] Example code snippets are correct
- [ ] Wiring diagram is accurate

### Code Documentation
- [ ] Code comments are clear
- [ ] Function purposes are documented
- [ ] Pin assignments are documented
- [ ] Timing constants are explained

## Issues Found

Document any issues discovered during testing:

| Issue # | Description | Severity | Status | Notes |
|---------|-------------|----------|--------|-------|
| | | | | |

## Test Results Summary

**Date:** _______________  
**Tester:** _______________  
**Hardware Version:** _______________  
**Firmware Version:** _______________  

**Overall Status:** ⬜ Pass ⬜ Pass with Issues ⬜ Fail

**Notes:**
_____________________________________________________________________________
_____________________________________________________________________________
_____________________________________________________________________________

## Recommendations

Based on testing results, list any recommendations for:
- Hardware improvements
- Software enhancements
- Documentation updates
- Additional features

_____________________________________________________________________________
_____________________________________________________________________________
_____________________________________________________________________________
