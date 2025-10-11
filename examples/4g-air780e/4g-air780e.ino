/*
 * Copyright (c) 2025, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Arduino.h"
#include "Windnerd_Core.h"
#include "stm32g0xx_hal.h"

WN_Core Anemometer;

HardwareSerial Serial4G(USART2);     // TX2/RX2 for 4G module communication
HardwareSerial SerialDebug(USART1);  // RX1 and TX1 on WindNerd Core board (headers connector)

// 4G board control pin (using available GPIO)
#define AIR780E_POWER_PIN PA0

// Timing constants
#define WAKEUP_INTERVAL_MS (15 * 60 * 1000)  // 15 minutes in milliseconds
#define AIR780E_STARTUP_DELAY_MS 3000        // Time for 4G module to boot up
#define AIR780E_COMMAND_TIMEOUT_MS 5000      // Timeout for command responses

// Wind data storage
struct WindData {
  float speed;
  uint16_t direction;
  uint32_t timestamp;
  bool valid;
};

WindData latestWind = {0, 0, 0, false};
uint32_t lastTransmissionTime = 0;

// called every 3 seconds, gives instant wind speed + direction
void instantWindCallback(wn_instant_wind_report_t instant_report)
{
  // Store the latest wind data
  latestWind.speed = instant_report.speed;
  latestWind.direction = instant_report.dir;
  latestWind.timestamp = millis();
  latestWind.valid = true;
  
  SerialDebug.print("Wind: ");
  SerialDebug.print(instant_report.speed, 1);
  SerialDebug.print(" km/h, Dir: ");
  SerialDebug.println(instant_report.dir);
}

// called every minute, gives wind average, min and max over last 10 minutes
void windReportCallback(wn_wind_report_t report)
{
  SerialDebug.print("Avg: ");
  SerialDebug.print(report.avg_speed, 1);
  SerialDebug.print(" km/h, Dir: ");
  SerialDebug.print(report.avg_dir);
  SerialDebug.print(", Min: ");
  SerialDebug.print(report.min_speed, 1);
  SerialDebug.print(", Max: ");
  SerialDebug.println(report.max_speed, 1);
}

void setup4GModule()
{
  pinMode(AIR780E_POWER_PIN, OUTPUT);
  digitalWrite(AIR780E_POWER_PIN, LOW); // Start with 4G module powered off
  
  Serial4G.begin(115200); // Standard baud rate for Air780E
  SerialDebug.println("4G module initialized");
}

void wakeup4GModule()
{
  SerialDebug.println("Waking up 4G module...");
  
  // Power on the 4G module
  digitalWrite(AIR780E_POWER_PIN, HIGH);
  delay(100);  // Short pulse
  digitalWrite(AIR780E_POWER_PIN, LOW);
  
  // Wait for module to boot
  delay(AIR780E_STARTUP_DELAY_MS);
  
  SerialDebug.println("4G module started");
}

void sleep4GModule()
{
  SerialDebug.println("Putting 4G module to sleep...");
  
  // Send sleep command to Air780E
  Serial4G.println("AT+CSCLK=2");  // Enable auto sleep mode
  delay(100);
  
  // Power down
  digitalWrite(AIR780E_POWER_PIN, HIGH);
  delay(1200);  // Long pulse for shutdown
  digitalWrite(AIR780E_POWER_PIN, LOW);
  
  SerialDebug.println("4G module sleeping");
}

bool sendATCommand(const char* command, const char* expectedResponse, uint32_t timeout)
{
  Serial4G.println(command);
  SerialDebug.print("Sent: ");
  SerialDebug.println(command);
  
  uint32_t startTime = millis();
  String response = "";
  
  while (millis() - startTime < timeout) {
    if (Serial4G.available()) {
      char c = Serial4G.read();
      response += c;
      
      if (response.indexOf(expectedResponse) != -1) {
        SerialDebug.print("Response: ");
        SerialDebug.println(response);
        return true;
      }
    }
  }
  
  SerialDebug.println("Command timeout");
  return false;
}

void transmitWindData()
{
  if (!latestWind.valid) {
    SerialDebug.println("No valid wind data to transmit");
    return;
  }
  
  SerialDebug.println("Transmitting wind data to 4G module...");
  
  wakeup4GModule();
  
  // Wait for module to be ready
  if (!sendATCommand("AT", "OK", AIR780E_COMMAND_TIMEOUT_MS)) {
    SerialDebug.println("4G module not responding");
    sleep4GModule();
    return;
  }
  
  // Check network registration
  if (!sendATCommand("AT+CREG?", "+CREG:", AIR780E_COMMAND_TIMEOUT_MS)) {
    SerialDebug.println("Network check failed");
    sleep4GModule();
    return;
  }
  
  // Prepare wind data message
  char dataBuffer[128];
  snprintf(dataBuffer, sizeof(dataBuffer), 
           "WIND,%.1f,%d,%lu", 
           latestWind.speed, 
           latestWind.direction, 
           latestWind.timestamp);
  
  SerialDebug.print("Data to send: ");
  SerialDebug.println(dataBuffer);
  
  // Send data via AT command (example - adjust based on actual Air780E protocol)
  // This could be HTTP POST, MQTT, or other protocol depending on your setup
  Serial4G.print("AT+CIPSEND=");
  Serial4G.println(strlen(dataBuffer));
  delay(100);
  Serial4G.println(dataBuffer);
  
  SerialDebug.println("Data transmitted");
  
  // Put module back to sleep
  sleep4GModule();
  
  lastTransmissionTime = millis();
}

void setup()
{
  SerialDebug.begin(115200);
  SerialDebug.println("WindNerd Core with 4G Air780E - Starting...");
  
  // Initialize 4G module
  setup4GModule();
  
  // Configure Anemometer
  // set 10 minutes as averaging period
  if (!Anemometer.setAveragingPeriodInSec(600))
  {
    SerialDebug.println("Incorrect averaging period value");
  }

  // set 1 minute as update interval
  if (!Anemometer.setReportingIntervalInSec(60))
  {
    SerialDebug.println("Incorrect reporting interval value");
  }
  
  Anemometer.invertVanePolarity(false);
  Anemometer.setSpeedUnit(UNIT_KPH);
  Anemometer.onInstantWindUpdate(&instantWindCallback);
  Anemometer.onNewWindReport(&windReportCallback);

  Anemometer.begin();
  
  SerialDebug.println("System ready");
}

void loop()
{
  Anemometer.loop();
  
  // Check if it's time to transmit data
  if (millis() - lastTransmissionTime >= WAKEUP_INTERVAL_MS) {
    transmitWindData();
  }

  // put the MCU to sleep, the WindNerd Core library uses a timer interrupt to wake it up automatically when needed
  HAL_SuspendTick();
  HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
  HAL_ResumeTick();
}

// this is an override to set the SYS clock at 8MHz in order to reduce power consumption
// this code was generated by STM32CubeMx
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV2;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }

  SystemCoreClockUpdate();
}
