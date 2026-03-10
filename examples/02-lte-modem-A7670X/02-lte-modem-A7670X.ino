/*
 * Copyright (c) 2026, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Arduino.h"
#include "Windnerd_Core.h"
#include "Windnerd_Wtp_Payload.h"
#include "stm32g0xx_hal.h"  // necessary to change clock settings

#define WTP_SECRET_KEY "af3ffa12c4937ddf"  // Replace with the secret key for your WTP device

//#define ENABLE_VOLTAGE  // uncomment this line for power voltage measurement (require divider bridge, see README)
//#define ENABLE_BME_280  // uncomment this line for extra temperature, humidity and pressure measurement with external BME280 sensor

#ifdef ENABLE_BME_280
#include <Bme280.h>
TwoWire Wire2;  // BME280 if present should be connected to I2C2
Bme280TwoWire sensor;
#else  // TX1 is available as debug port only if I2C2 is unused
HardwareSerial SerialDebug(USART1);
#endif

#define REPORT_INTERVAL_MN 2  // Interval in minutes between uploading wind data via WindNerd Transfer Protocol (https://windnerd.net/docs/wind-transfer-protocol)
#define NO_SLEEP_AFTER_START_UP_MN 5
#define SKIP_SLEEP_EVERY 15   // in upload cycles
#define REBOOT_MODEM_EVERY 30 * 24 // one reboot a day keeps the bugs away


WN_Core Anemometer;
HardwareSerial SerialOutput(USART2);  // to serial LTE modem (SIM7670E, SIM7080G, AIR780E...), only TX2 is used, RX2 is not read

WN_WTP_PAYLOAD Wtp_payload;

unsigned long last_uploading_time = millis();
unsigned post_cnt = 1;

// steps for uploading wind data
enum Modem_steps {
  START = 0,
  TERMINATE,
  INIT,
  SET_URL,
  SET_HEADERS,
  SET_DATA,
  POST,
  GO_SLEEP,
  SLEEP,
};

// variables for the state machine
unsigned long last_step_time = millis();
unsigned long time_to_wait_before_next_step = 0;
Modem_steps modem_step = SLEEP;

#ifdef ENABLE_VOLTAGE
float getPowerVoltage() {
  float raw_voltage = analogRead(PB0) * 3.3f / 1024;  // ADC voltage reference is Vcc regulated at 3.3V, 10 bits ADC -> 1024 measurement steps
  return raw_voltage * 2 + 0.22;               // we multiply by 2 because of bridge divider, we add 0.22 to compenstae for input diode voltage drop
}
#endif


// non blocking wait for the next modem operation, delay parameter in ms
void waitForNextStep(uint16_t time_to_wait) {
  last_step_time = millis();
  time_to_wait_before_next_step = time_to_wait;
  modem_step = static_cast<Modem_steps>(modem_step + 1);
}

void sendCommandToModem(char *cmd) {
  SerialOutput.println(cmd);  // command should end with line break
}

void sendTextToModem(char *txt) {
  SerialOutput.print(txt);
}


// non blocking state machine that sequentially sends AT commands and payloads
void processModem() {

  if (modem_step == START) {
    sendCommandToModem("AT");
    waitForNextStep(100);
    return;
  }

  if (modem_step != SLEEP && (millis() - last_step_time > time_to_wait_before_next_step)) {

    if (modem_step == TERMINATE) {
      sendCommandToModem("AT+HTTPTERM");  // replace with sendCommandToModem("AT+HTTPREAD=0,200"); if you want to read HTTP response (on modem TX) for debug purpose
      waitForNextStep(100);
      return;
    }

    if (modem_step == INIT) {
      sendCommandToModem("AT+HTTPINIT");
      waitForNextStep(100);  // on some A7670 (different firmware version?) HTTP INIT takes seconds! this timing might require increase
      return;
    }

    if (modem_step == SET_URL) {
      sendCommandToModem("AT+HTTPPARA=\"URL\",\"http://wtp.windnerd.net/post\"");
      waitForNextStep(100);
      return;
    }

    if (modem_step == SET_HEADERS) {

      // setting headers is mainly about specifying the length of data transmitted at next step, we set up the payload now so the helper object can calculate the length
      Wtp_payload.reset();
      Wtp_payload.setAnemometer(&Anemometer);
      Wtp_payload.setPeriodInMinutes(REPORT_INTERVAL_MN);
      Wtp_payload.setSecretKey(WTP_SECRET_KEY);
      Wtp_payload.enableWindSamples();
#ifdef ENABLE_VOLTAGE
      Wtp_payload.setVoltage(getPowerVoltage());
#endif
#ifdef ENABLE_BME_280
      Wtp_payload.setTemperature(sensor.getTemperature());
      Wtp_payload.setPressure(sensor.getPressure() / 100);
      Wtp_payload.setHumidity(sensor.getHumidity());
#endif

      char buffer[32];
      sprintf(buffer, "AT+HTTPDATA=%d,10000", Wtp_payload.calculatePayloadLength());
      sendCommandToModem(buffer);
      waitForNextStep(100);
      return;
    }

    if (modem_step == SET_DATA) {
      Wtp_payload.sendPayload(&SerialOutput);
      waitForNextStep(200);
      return;
    }

    if (modem_step == POST) {
      sendCommandToModem("AT+HTTPACTION=1");  // perform POST request
      waitForNextStep(2000);
      return;
    }

    if (modem_step == GO_SLEEP) {
      // we don't send modem to sleep immediately after start up to ensure enough time for attaching to mobile network
      // we skip a sleep cycle periodically, to give a chance to recover if something went wrong
      // we reboot the modem daily as extra precaution
      if (post_cnt % REBOOT_MODEM_EVERY == 0) {
        sendCommandToModem("AT+CPOF");
      } else if (millis() > NO_SLEEP_AFTER_START_UP_MN * 60 * 1000 && post_cnt % SKIP_SLEEP_EVERY != 0) {
        sendCommandToModem("AT+CSCLK=2");
      }
      modem_step = SLEEP;
      post_cnt++;
      return;
    }
  }
}

// watchdog activation sequence given in STM32G0 reference manual 28.3.2
void initWatchdog() {
  IWDG->KR = 0xCCCC;
  IWDG->KR = 0x5555;
  IWDG->PR = 0x4;      // prescaler 64
  IWDG->RLR = 0x0800;  // (64 * 2048) / 32000 ~= 4 sec
  while (IWDG->SR != 0) {
  }
  IWDG->KR = 0xAAAA;
}

static inline void feedWatchdog(void) {
  IWDG->KR = 0xAAAA;
}


void setup() {
  initWatchdog();
  SerialOutput.begin(115200);
  Anemometer.invertVanePolarity(false);                 // change to true if you notice north and south are inverted
  Anemometer.begin();
#ifdef ENABLE_BME_280
  Wire2.setSCL(PA11);
  Wire2.setSDA(PA12);
  Wire2.begin();
  sensor.begin(Bme280TwoWireAddress::Primary, &Wire2);
  sensor.setSettings(Bme280Settings::indoor());
#else
  SerialDebug.begin(115200);
#endif
  delay(1000);
}

void loop() {

  feedWatchdog();

  Anemometer.loop();

  // start the process for a new upload when time interval from last upload has expired
  if (modem_step == SLEEP && (millis() - last_uploading_time > (REPORT_INTERVAL_MN * 60 * 1000))) {
    modem_step = START;
    last_uploading_time = millis();
  }
  processModem();


  // put the MCU to sleep, the WindNerd Core library uses a timer interrupt to wake it up automatically every 100ms
  HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
}

// this is an override to set the SYS clock at 8MHz in order to reduce power consumption
// this code was generated by STM32CubeMx
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
  RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

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
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
     */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
    Error_Handler();
  }

  SystemCoreClockUpdate();
}
