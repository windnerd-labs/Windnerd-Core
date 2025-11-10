/*
 * Copyright (c) 2025, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Arduino.h"
#include "Windnerd_Core.h"
#include "stm32g0xx_hal.h" // necessary to change clock settings

#define REPORT_INTERVAL_MN 2              // Interval in minutes between uploading wind data via WindNerd Transfer Protocol (https://windnerd.net/docs/wind-transfer-protocol)
#define WTP_SECRET_KEY "af3ffa12c4937ddf" // Replace with the secret key for your WTP device
#define NO_SLEEP_AFTER_START_UP_MN 5
#define SKIP_SLEEP_EVERY 15

WN_Core Anemometer;
HardwareSerial SerialOutput(USART2); // to serial LTE modem (SIM7670E, SIM7080G, AIR780E...), only TX2 is used, RX2 is not read
HardwareSerial SerialDebug(USART1);  // TX1 only to serial debug console

unsigned long last_uploading_time = millis();
unsigned post_cnt = 0;

// steps for uploading wind data
enum Modem_steps
{
    START = 0,
    INIT,
    SET_URL,
    SET_HEADERS,
    SET_DATA,
    POST,
    TERMINATE,
    GO_SLEEP,
    SLEEP,
};

// variables for the state machine
unsigned long last_step_time = millis();
unsigned long time_to_wat_before_next_step = 0;
Modem_steps modem_step = SLEEP;

// non blocking wait for the next modem operation
void waitForNextStep(uint16_t time_to_wait)
{
    last_step_time = millis();
    time_to_wat_before_next_step = time_to_wait;
    modem_step = static_cast<Modem_steps>(modem_step + 1);
}

void sendCommandToModem(char *cmd)
{
    SerialOutput.println(cmd);
    SerialDebug.printf("\r\n>>> Sent to modem: %s\r\n", cmd);
}

void sendTextToModem(char *txt)
{
    SerialOutput.print(txt);
    SerialDebug.printf("\r\n>>> Sent to modem: %s\r\n", txt);
}

// compose a message to send 1 or more wind reports via WTP
void composeReportLine(unsigned i, char *buffer)
{
    wn_wind_report_t report = Anemometer.computeReportForPeriodInSecIndexedFromLast(60, i);
    char wa[6], wn[6], wx[6];
    // fixed length conversions so we can determine payload total length easily
    dtostrf(report.avg_speed, 4, 1, wa);
    dtostrf(report.min_speed, 4, 1, wn);
    dtostrf(report.max_speed, 4, 1, wx);
    sprintf(buffer, "r,wa=%s,wd=%03d,wn=%s,wx=%s;", wa, report.avg_dir, wn, wx);
}

// compose a message to send aggregated instany wind samples via WTP
void composeSampleLine(unsigned i, char *buffer)
{
    wn_instant_wind_sample_t sample = Anemometer.getSampleIndexedFromLast(i);
    char wi[6];
    dtostrf(sample.speed, 4, 1, wi); // fixed length conversion so we can determine payload total length easily
    sprintf(buffer, "s,wi=%s,wd=%03d;", wi, sample.dir);
}

// non blocking state machine that sequentially sends AT commands and payloads
void processModem()
{

    if (modem_step == START)
    {
        sendCommandToModem("AT");
        waitForNextStep(100);
        return;
    }

    if (modem_step != SLEEP && (millis() - last_step_time > time_to_wat_before_next_step))
    {

        if (modem_step == INIT)
        {
            sendCommandToModem("AT+HTTPINIT");
            waitForNextStep(100);
            return;
        }

        if (modem_step == SET_URL)
        {
            sendCommandToModem("AT+HTTPPARA=\"URL\",\"http://wtp.windnerd.net/post\"");
            waitForNextStep(100);
            return;
        }

        if (modem_step == SET_HEADERS)
        {
            char buffer[32];
            sprintf(buffer, "AT+HTTPDATA=%d,10000", 19 + 33 * REPORT_INTERVAL_MN + 17 * REPORT_INTERVAL_MN * 20); // we calculate the length of the following payload
            sendCommandToModem(buffer);
            waitForNextStep(100);
            return;
        }

        if (modem_step == SET_DATA)
        {

            char buffer[64];
            sprintf(buffer, "k=%s;", WTP_SECRET_KEY);
            sendTextToModem(buffer);

            for (unsigned i = 0; i < REPORT_INTERVAL_MN; i++)
            {
                composeReportLine(i, buffer);
                sendTextToModem(buffer);
            }

            for (unsigned i = 0; i < REPORT_INTERVAL_MN * 20; i++)
            {
                composeSampleLine(i, buffer);
                sendTextToModem(buffer);
            }

            waitForNextStep(200);
            return;
        }

        if (modem_step == POST)
        {
            sendCommandToModem("AT+HTTPACTION=1"); // perform POST request
            waitForNextStep(2000);                 // increase the waiting time if you see don't see result like HTTPACTION: 1,200,2 on modem TX
            return;
        }

        if (modem_step == TERMINATE)
        {
            sendCommandToModem("AT+HTTPTERM"); // replace with sendCommandToModem("AT+HTTPREAD=0,200"); if you want to read HTTP response (on modem TX) for debug purpose
            waitForNextStep(100);
            return;
        }

        if (modem_step == GO_SLEEP)
        {
            // we don't send modem to sleep immediately after start up to ensure enough time for attaching to mobile network
            // we skip a sleep cycle periodically, to give a chance to recover if something went wrong
            if (millis() > NO_SLEEP_AFTER_START_UP_MN * 60 * 1000 && post_cnt % SKIP_SLEEP_EVERY != 0)
            {
                sendCommandToModem("AT+CSCLK=2");
            }
            modem_step = SLEEP;
            post_cnt++;
            return;
        }
    }
}

void setup()
{
    SerialDebug.begin(115200);
    SerialOutput.begin(115200);
    SerialDebug.println("=== LTE Modem Example Debug Port ===");

    Anemometer.begin();

    delay(2000);
}

void loop()
{
    Anemometer.loop();

    // start the process for a new upload when time interval from last upload has expired
    if (modem_step == SLEEP && (millis() - last_uploading_time > (REPORT_INTERVAL_MN * 60 * 1000)))
    {
        modem_step = START;
        last_uploading_time = millis();
    }
    processModem();

    // put the MCU to sleep, the WindNerd Core library uses a timer interrupt to wake it up automatically every 100ms
    HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
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
