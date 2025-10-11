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

// Windguru API configuration
// IMPORTANT: Replace these with your actual Windguru station credentials
#define WINDGURU_STATION_UID "YOUR_STATION_UID"    // Replace with your station UID from Windguru
#define WINDGURU_STATION_PASSWORD "YOUR_PASSWORD"  // Replace with your station password

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

// Simple MD5 hash implementation for Windguru authentication
// Based on RFC 1321 - simplified for embedded systems
void md5Hash(const char* input, char* output) {
  // MD5 constants
  const uint32_t S[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
  };
  
  const uint32_t K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
  };
  
  uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
  
  // Prepare message
  int msgLen = strlen(input);
  int paddedLen = ((msgLen + 8) / 64 + 1) * 64;
  uint8_t* msg = (uint8_t*)calloc(paddedLen, 1);
  memcpy(msg, input, msgLen);
  msg[msgLen] = 0x80;
  
  // Append length
  uint64_t bitLen = msgLen * 8;
  memcpy(msg + paddedLen - 8, &bitLen, 8);
  
  // Process message in 512-bit chunks
  for (int offset = 0; offset < paddedLen; offset += 64) {
    uint32_t* M = (uint32_t*)(msg + offset);
    uint32_t A = a0, B = b0, C = c0, D = d0;
    
    for (int i = 0; i < 64; i++) {
      uint32_t F, g;
      if (i < 16) {
        F = (B & C) | ((~B) & D);
        g = i;
      } else if (i < 32) {
        F = (D & B) | ((~D) & C);
        g = (5 * i + 1) % 16;
      } else if (i < 48) {
        F = B ^ C ^ D;
        g = (3 * i + 5) % 16;
      } else {
        F = C ^ (B | (~D));
        g = (7 * i) % 16;
      }
      
      F = F + A + K[i] + M[g];
      A = D;
      D = C;
      C = B;
      B = B + ((F << S[i]) | (F >> (32 - S[i])));
    }
    
    a0 += A; b0 += B; c0 += C; d0 += D;
  }
  
  free(msg);
  
  // Format output as hex string
  sprintf(output, "%08x%08x%08x%08x", 
          (unsigned int)a0, (unsigned int)b0, (unsigned int)c0, (unsigned int)d0);
}

// Convert wind speed from km/h to knots (required by Windguru)
float kphToKnots(float kph) {
  return kph * 0.539957;
}

void transmitWindData()
{
  if (!latestWind.valid) {
    SerialDebug.println("No valid wind data to transmit");
    return;
  }
  
  SerialDebug.println("Transmitting wind data to Windguru API...");
  
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
  
  // Generate salt (using timestamp)
  char salt[32];
  snprintf(salt, sizeof(salt), "%lu", millis());
  
  // Calculate MD5 hash: MD5(salt + uid + password)
  char hashInput[256];
  snprintf(hashInput, sizeof(hashInput), "%s%s%s", 
           salt, WINDGURU_STATION_UID, WINDGURU_STATION_PASSWORD);
  
  char md5Output[33];
  md5Hash(hashInput, md5Output);
  
  // Convert wind speed to knots (Windguru requirement)
  float windSpeedKnots = kphToKnots(latestWind.speed);
  
  // Build Windguru API URL
  // Format: http://www.windguru.cz/upload/api.php?uid=XXX&salt=YYY&hash=ZZZ&wind_avg=W&wind_direction=D
  char urlBuffer[384];
  snprintf(urlBuffer, sizeof(urlBuffer),
           "http://www.windguru.cz/upload/api.php?uid=%s&salt=%s&hash=%s&wind_avg=%.1f&wind_direction=%d",
           WINDGURU_STATION_UID, salt, md5Output, windSpeedKnots, latestWind.direction);
  
  SerialDebug.print("URL: ");
  SerialDebug.println(urlBuffer);
  
  // Initialize HTTP
  if (!sendATCommand("AT+HTTPINIT", "OK", AIR780E_COMMAND_TIMEOUT_MS)) {
    SerialDebug.println("HTTP init failed");
    sleep4GModule();
    return;
  }
  
  // Set URL parameter
  char urlCmd[512];
  snprintf(urlCmd, sizeof(urlCmd), "AT+HTTPPARA=\"URL\",\"%s\"", urlBuffer);
  if (!sendATCommand(urlCmd, "OK", AIR780E_COMMAND_TIMEOUT_MS)) {
    SerialDebug.println("HTTP URL set failed");
    Serial4G.println("AT+HTTPTERM");
    delay(100);
    sleep4GModule();
    return;
  }
  
  // Execute HTTP GET request (action=0 for GET)
  if (!sendATCommand("AT+HTTPACTION=0", "OK", AIR780E_COMMAND_TIMEOUT_MS)) {
    SerialDebug.println("HTTP GET failed");
    Serial4G.println("AT+HTTPTERM");
    delay(100);
    sleep4GModule();
    return;
  }
  
  // Wait for response
  delay(3000);
  
  // Read HTTP response
  sendATCommand("AT+HTTPREAD", "OK", AIR780E_COMMAND_TIMEOUT_MS);
  
  // Terminate HTTP
  Serial4G.println("AT+HTTPTERM");
  delay(100);
  
  SerialDebug.println("Data transmitted to Windguru");
  
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
