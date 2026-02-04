/*
 * Copyright (c) 2025, windnerd.net
 * All rights reserved.
 *
 * This source code is licensed under the BSD 3-Clause License found in the
 * LICENSE file in the root directory of this source tree.
 * 
 * Air780E 4G - Dual Upload to WindNerd WTP + Windguru
 * Uses STM32 UID + random for unique Windguru salt
 * 
 * Reliability features for unattended deployment:
 * - Independent Watchdog (IWDG) - auto-reset if MCU hangs
 * - State timeout - escape stuck modem states
 * - Periodic modem reset - clear accumulated errors
 * - Ultra-low power mode - extends battery life
 */

#include "Arduino.h"
#include "Windnerd_Core.h"
#include "MD5.h"
#include "stm32g0xx_hal.h"

// STM32 Unique Device ID address
#define STM32_UID_BASE 0x1FFF7590

// ==================== CONFIGURATION ====================
#define REPORT_INTERVAL_MN 15             // Interval in minutes between uploads

// If set to 1, rotate reported wind direction by 180° because the
// vane magnet is installed upside-down. Set to 0 for normal polarity.
#define INVERT_VANE_POLARITY 1

// WindNerd WTP configuration
#define WTP_SECRET_KEY "YOUR_SECRET_KEY_HERE"  // Replace with your WTP device secret key
#define ENABLE_WTP_SAMPLES false         // Set to false to send only reports (saves ~90% data)

// Windguru configuration
#define WINDGURU_UID "YOUR_UID_HERE"     // Replace with your Windguru station UID
#define WINDGURU_PASSWORD "YOUR_PASSWORD_HERE" // Replace with your Windguru station password
#define ENABLE_WINDGURU true              // Set to false to disable Windguru uploads

// Network configuration
#define APN_FALLBACK "internet"      // Add an APN as fallback if modem doesnt auto-detect

// Sleep configuration
#define MODEM_BAUD_RATE 9600          // 9600 baud for reliable sleep/wake (per datasheet)
#define SLEEP_WAIT_TIME_SEC 3         // Time to wait before sleeping (1-5 sec recommended)
#define ULTRA_LOW_POWER_MODE 1        // 0=Sleep mode 2, 1=Sleep mode 3 (ultra-low power)

// Reliability configuration (for unattended deployment)
#define WATCHDOG_TIMEOUT_SEC 10           // Watchdog timeout - MCU resets if not kicked
#define STATE_TIMEOUT_SEC 90              // Max time in any state before aborting cycle
#define MODEM_RESET_CYCLES 96             // Hard reset modem every N cycles (96 = daily at 15min)

// ==================== GLOBALS ====================
WN_Core Anemometer;
HardwareSerial SerialDebug(USART1);       // Debug output
HardwareSerial SerialOutput(USART2);      // 4G modem

// First upload after 30 seconds (not full interval) to get modem sleeping faster
unsigned long last_uploading_time = 0;  // Will be set in setup()
uint16_t post_cnt = 0;

// Delay before first upload (seconds) - allows modem to connect
#define FIRST_UPLOAD_DELAY_SEC 30

// Unique salt for Windguru (STM32 UID + random)
char salt_str[16] = "0";
uint32_t device_uid = 0;  // Part of STM32 unique ID

// Wind data for Windguru (latest report)
float last_avg_speed = 0.0f;
float last_max_speed = 0.0f;
float last_min_speed = 0.0f;
int16_t last_avg_dir = 0;

// Modem sleep state tracking
bool modem_is_sleeping = false;  // Start awake until first sleep

// ==================== STATE MACHINE ====================
enum Modem_steps
{
    START = 0,
    CHECK_CSQ,        // Signal quality
    CHECK_CREG,       // GSM registration
    CHECK_CEREG,      // LTE registration
    CHECK_CGATT,      // GPRS attach status
    QUERY_APN,        // Query APN from network
    GET_TIME,         // Get network time for Windguru salt
    CHECK_CGPADDR,    // Check IP address
    CGATT,            // GPRS attach
    SAPBR_APN,        // Set APN in bearer profile
    SAPBR_OPEN,       // Open bearer
    SAPBR_QUERY,      // Query bearer status/IP
    
    // WindNerd WTP upload
    WTP_TERMINATE,
    WTP_INIT,
    WTP_SET_CID,
    WTP_SET_CONTENT,
    WTP_SET_URL,
    WTP_SET_DATA_LEN,
    WTP_SEND_DATA,
    WTP_POST,
    WTP_READ,
    
    // Windguru upload
    WG_TERMINATE,
    WG_INIT,
    WG_SET_CID,
    WG_SET_URL,
    WG_GET,
    WG_READ,
    
    GO_SLEEP,
    SLEEP,
};

unsigned long last_step_time = millis();
unsigned long time_to_wait_before_next_step = 0;
unsigned long cycle_start_time = 0;        // For state timeout detection
Modem_steps modem_step = SLEEP;

// ==================== HELPER FUNCTIONS ====================

void waitForNextStep(uint16_t time_to_wait)
{
    last_step_time = millis();
    time_to_wait_before_next_step = time_to_wait;
    modem_step = static_cast<Modem_steps>(modem_step + 1);
}

void jumpToStep(Modem_steps step, uint16_t time_to_wait)
{
    last_step_time = millis();
    time_to_wait_before_next_step = time_to_wait;
    modem_step = step;
}

void sendCommandToModem(const char *cmd)
{
    SerialOutput.println(cmd);
    SerialDebug.printf("\r\n>>> %s\r\n", cmd);
}

void sendTextToModem(const char *txt)
{
    SerialOutput.print(txt);
    SerialDebug.printf(">>> %s\r\n", txt);
}

// ==================== MODEM POWER MANAGEMENT ====================

void configureModemSleep() {
    SerialDebug.println("Configuring modem sleep parameters...");
    
    // Disable sleep first to ensure commands are received
    SerialOutput.println("AT+CSCLK=0");
    delay(200);
    
    // Set sleep wait time (seconds of UART idle before sleeping)
    // Default is 5s, we use 3s for faster sleep entry
    char buffer[32];
    sprintf(buffer, "AT+WAKETIM=%d", SLEEP_WAIT_TIME_SEC);
    SerialOutput.println(buffer);
    SerialDebug.printf(">>> %s\r\n", buffer);
    delay(200);
    
    // Save to NV so it persists across resets
    SerialOutput.println("AT&W");
    delay(200);
    
    SerialDebug.println("Modem sleep config complete");
    modem_is_sleeping = false;
}

void wakeModem() {
    if (!modem_is_sleeping) {
        return;  // Already awake
    }
    
    SerialDebug.println("Waking modem...");
    
    // Send multiple AT commands to wake modem
    // First bytes may be lost while modem wakes from sleep
    // No RX connected, so we can't check for response - just use delays
    for (int i = 0; i < 3; i++) {
        SerialOutput.println("AT");
        delay(200);
    }
    
    modem_is_sleeping = false;
    SerialDebug.println("Wake sequence sent");
}

void putModemToSleep() {
    SerialDebug.println("Enabling modem auto-sleep...");
    
    // Enable auto-sleep mode
    SerialOutput.println("AT+CSCLK=2");
    
    // CRITICAL: Modem needs WAKETIM seconds of UART silence to actually sleep!
    // We must NOT send anything during this time
    SerialDebug.printf("Waiting %ds for modem to enter sleep...\r\n", SLEEP_WAIT_TIME_SEC + 1);
    
    // Wait slightly longer than WAKETIM to ensure modem is actually sleeping
    // Kick watchdog during this wait to prevent reset
    for (int i = 0; i < (SLEEP_WAIT_TIME_SEC + 1) * 10; i++) {
        delay(100);
        kickWatchdog();
    }
    
    modem_is_sleeping = true;
    SerialDebug.println("Modem should now be sleeping");
}

// ==================== WINDNERD WTP FUNCTIONS ====================

void composeReportLine(unsigned i, char *buffer)
{
    wn_wind_report_t report = Anemometer.computeReportForPeriodInSecIndexedFromLast(60, i);
    char wa[6], wn[6], wx[6];
    dtostrf(report.avg_speed, 4, 1, wa);  // Fixed width 4 chars (e.g., " 0.0" or "12.3")
    dtostrf(report.min_speed, 4, 1, wn);
    dtostrf(report.max_speed, 4, 1, wx);
    sprintf(buffer, "r,wa=%s,wd=%03d,wn=%s,wx=%s;", wa, report.avg_dir, wn, wx);
    
    // Store for Windguru (use most recent report)
    if (i == 0) {
        last_avg_speed = report.avg_speed;
        last_max_speed = report.max_speed;
        last_min_speed = report.min_speed;
        last_avg_dir = report.avg_dir;
    }
}

void composeSampleLine(unsigned i, char *buffer)
{
    wn_instant_wind_sample_t sample = Anemometer.getSampleIndexedFromLast(i);
    char wi[6];
    dtostrf(sample.speed, 4, 1, wi);
    sprintf(buffer, "s,wi=%s,wd=%03d;", wi, sample.dir);
}

int calculateWtpPayloadLength()
{
    // k=<16chars>;  = 19 bytes
    // r,wa=XX.X,wd=XXX,wn=XX.X,wx=XX.X; = 33 bytes per report
    // s,wi=XX.X,wd=XXX; = 17 bytes per sample
    int len = 19 + 33 * REPORT_INTERVAL_MN;
    #if ENABLE_WTP_SAMPLES
    len += 17 * REPORT_INTERVAL_MN * 20;
    #endif
    return len;
}

// ==================== WINDGURU FUNCTIONS ====================

void generateWindguruHash(char* hashOut, const char* salt, const char* uid, const char* password)
{
    // Hash = MD5(salt + uid + password)
    // Build input string in a small buffer
    char input[80];
    snprintf(input, sizeof(input), "%s%s%s", salt, uid, password);
    
    unsigned char* hash = MD5::make_hash(input);
    char* md5str = MD5::make_digest(hash, 16);
    strcpy(hashOut, md5str);
    
    free(hash);
    free(md5str);
}

void buildWindguruUrl(char* url, size_t maxLen)
{
    // Use upload_counter as salt (unique, persists in flash)
    char hash[33];
    generateWindguruHash(hash, salt_str, WINDGURU_UID, WINDGURU_PASSWORD);
    
    // Convert wind speeds from m/s to knots (1 m/s = 1.94384 knots)
    float avgKnots = last_avg_speed * 1.94384f;
    float maxKnots = last_max_speed * 1.94384f;
    float minKnots = last_min_speed * 1.94384f;
    
    // Build URL with wind data (use minimal width to avoid spaces)
    char avgStr[8], maxStr[8], minStr[8];
    dtostrf(avgKnots, 1, 1, avgStr);  // Width 1 = no padding
    dtostrf(maxKnots, 1, 1, maxStr);
    dtostrf(minKnots, 1, 1, minStr);
    
    // interval = reporting period in seconds
    int interval = REPORT_INTERVAL_MN * 60;
    
    snprintf(url, maxLen,
        "AT+HTTPPARA=\"URL\",\"http://www.windguru.cz/upload/api.php?uid=%s&salt=%s&hash=%s&interval=%d&wind_avg=%s&wind_max=%s&wind_min=%s&wind_direction=%d\"",
        WINDGURU_UID, salt_str, hash, interval, avgStr, maxStr, minStr, last_avg_dir);
}

// ==================== STATE MACHINE ====================

void processModem()
{
    // Kick watchdog in every call (prevents reset during normal operation)
    kickWatchdog();
    
    // STATE TIMEOUT: If cycle takes too long, abort and go to sleep
    if (modem_step != SLEEP && modem_step != START && 
        (millis() - cycle_start_time > STATE_TIMEOUT_SEC * 1000UL))
    {
        SerialDebug.println("\r\n!!! STATE TIMEOUT - Aborting cycle !!!");
        sendCommandToModem("AT+HTTPTERM");  // Clean up HTTP
        delay(100);
        putModemToSleep();
        modem_step = SLEEP;
        return;
    }
    
    // START state - no wait needed
    if (modem_step == START)
    {
        cycle_start_time = millis();  // Record cycle start for timeout
        
        // Wake modem if sleeping
        if (modem_is_sleeping) {
            wakeModem();
        }
        
        // PERIODIC MODEM RESET: Every N cycles, hard reset modem
        if (post_cnt > 0 && post_cnt % MODEM_RESET_CYCLES == 0)
        {
            SerialDebug.println("\r\n*** PERIODIC MODEM RESET ***");
            SerialOutput.println("AT+CFUN=1,1");  // Full modem reset
            delay(5000);  // Wait for modem to restart
            
            // Re-establish communication at 9600
            SerialOutput.println("AT");
            delay(200);
            SerialOutput.println("AT");
            delay(200);
            
            configureModemSleep();
        }
        
        // Disable auto-sleep during active operations
        SerialOutput.println("AT+CSCLK=0");
        delay(100);
        
        // Generate unique salt: 32-bit UID + 32-bit random = 64-bit total uniqueness
        // Salt format: 8 hex chars (UID) + 8 hex chars (random) = 16 chars max
        // This ensures billions of uploads before any collision risk
        uint32_t rnd = (uint32_t)random(0, 0x7FFFFFFF) ^ ((uint32_t)random(0, 0x7FFFFFFF) << 1);
        sprintf(salt_str, "%lx%lx", device_uid, rnd);
        
        SerialDebug.println("\r\n========== UPLOAD CYCLE START ==========");
        post_cnt++;
        sendCommandToModem("AT");
        waitForNextStep(100);
        return;
    }

    // All other states wait for timeout
    if (modem_step != SLEEP && (millis() - last_step_time > time_to_wait_before_next_step))
    {
        // ==================== DIAGNOSTICS ====================
        if (modem_step == CHECK_CSQ)
        {
            sendCommandToModem("AT+CSQ");
            waitForNextStep(100);
            return;
        }

        if (modem_step == CHECK_CREG)
        {
            sendCommandToModem("AT+CREG?");
            waitForNextStep(100);
            return;
        }

        if (modem_step == CHECK_CEREG)
        {
            sendCommandToModem("AT+CEREG?");
            waitForNextStep(100);
            return;
        }

        if (modem_step == CHECK_CGATT)
        {
            sendCommandToModem("AT+CGATT?");
            waitForNextStep(100);
            return;
        }

        if (modem_step == QUERY_APN)
        {
            // Query APN from active PDP context
            // Response: +CGCONTRDP: 1,5,"apnname",...
            sendCommandToModem("AT+CGCONTRDP=1");
            waitForNextStep(500);
            return;
        }

        if (modem_step == GET_TIME)
        {
            // Get network time for Windguru salt
            // Response: +CCLK: "YY/MM/DD,HH:MM:SS+TZ"
            sendCommandToModem("AT+CCLK?");
            waitForNextStep(200);
            return;
        }

        if (modem_step == CHECK_CGPADDR)
        {
            sendCommandToModem("AT+CGPADDR=1");
            waitForNextStep(100);
            return;
        }

        // ==================== BEARER SETUP ====================
        if (modem_step == CGATT)
        {
            sendCommandToModem("AT+CGATT=1");
            waitForNextStep(500);
            return;
        }

        if (modem_step == SAPBR_APN)
        {
            char buffer[64];
            sprintf(buffer, "AT+SAPBR=3,1,\"APN\",\"%s\"", APN_FALLBACK);
            sendCommandToModem(buffer);
            waitForNextStep(100);
            return;
        }

        if (modem_step == SAPBR_OPEN)
        {
            sendCommandToModem("AT+SAPBR=1,1");
            waitForNextStep(2000);
            return;
        }

        if (modem_step == SAPBR_QUERY)
        {
            sendCommandToModem("AT+SAPBR=2,1");
            waitForNextStep(100);
            return;
        }

        // ==================== WINDNERD WTP UPLOAD ====================
        if (modem_step == WTP_TERMINATE)
        {
            SerialDebug.println("\r\n--- WindNerd WTP Upload ---");
            sendCommandToModem("AT+HTTPTERM");
            waitForNextStep(100);
            return;
        }

        if (modem_step == WTP_INIT)
        {
            sendCommandToModem("AT+HTTPINIT");
            waitForNextStep(100);
            return;
        }

        if (modem_step == WTP_SET_CID)
        {
            sendCommandToModem("AT+HTTPPARA=\"CID\",1");
            waitForNextStep(100);
            return;
        }

        if (modem_step == WTP_SET_CONTENT)
        {
            sendCommandToModem("AT+HTTPPARA=\"CONTENT\",\"text/plain\"");
            waitForNextStep(100);
            return;
        }

        if (modem_step == WTP_SET_URL)
        {
            sendCommandToModem("AT+HTTPPARA=\"URL\",\"http://wtp.windnerd.net/post\"");
            waitForNextStep(100);
            return;
        }

        if (modem_step == WTP_SET_DATA_LEN)
        {
            char buffer[32];
            sprintf(buffer, "AT+HTTPDATA=%d,10000", calculateWtpPayloadLength());
            sendCommandToModem(buffer);
            waitForNextStep(200);  // Wait for DOWNLOAD prompt
            return;
        }

        if (modem_step == WTP_SEND_DATA)
        {
            char buffer[64];
            
            // Send key
            sprintf(buffer, "k=%s;", WTP_SECRET_KEY);
            sendTextToModem(buffer);

            // Send reports
            for (unsigned i = 0; i < REPORT_INTERVAL_MN; i++)
            {
                composeReportLine(i, buffer);
                sendTextToModem(buffer);
            }

            // Send samples (if enabled)
            #if ENABLE_WTP_SAMPLES
            for (unsigned i = 0; i < REPORT_INTERVAL_MN * 20; i++)
            {
                composeSampleLine(i, buffer);
                sendTextToModem(buffer);
            }
            #endif

            waitForNextStep(500);
            return;
        }

        if (modem_step == WTP_POST)
        {
            SerialDebug.println(">>> WTP POST <<<");
            sendCommandToModem("AT+HTTPACTION=1");
            waitForNextStep(15000);  // Wait longer for server response
            return;
        }

        if (modem_step == WTP_READ)
        {
            SerialDebug.println(">>> WTP READ <<<");
            sendCommandToModem("AT+HTTPREAD");
            waitForNextStep(1000);
            return;
        }

        // ==================== WINDGURU UPLOAD ====================
        if (modem_step == WG_TERMINATE)
        {
            #if ENABLE_WINDGURU
            SerialDebug.println("\r\n--- Windguru Upload ---");
            sendCommandToModem("AT+HTTPTERM");
            waitForNextStep(100);
            #else
            jumpToStep(GO_SLEEP, 10);
            #endif
            return;
        }

        if (modem_step == WG_INIT)
        {
            sendCommandToModem("AT+HTTPINIT");
            waitForNextStep(100);
            return;
        }

        if (modem_step == WG_SET_CID)
        {
            sendCommandToModem("AT+HTTPPARA=\"CID\",1");
            waitForNextStep(100);
            return;
        }

        if (modem_step == WG_SET_URL)
        {
            char url[240];  // Increased for wind_min + interval params
            buildWindguruUrl(url, sizeof(url));
            sendCommandToModem(url);
            waitForNextStep(100);
            return;
        }

        if (modem_step == WG_GET)
        {
            sendCommandToModem("AT+HTTPACTION=0");  // GET request
            waitForNextStep(10000);
            return;
        }

        if (modem_step == WG_READ)
        {
            sendCommandToModem("AT+HTTPREAD");
            waitForNextStep(1000);
            return;
        }

        // ==================== CLEANUP & SLEEP ====================
        if (modem_step == GO_SLEEP)
        {
            // Terminate HTTP session
            sendCommandToModem("AT+HTTPTERM");
            delay(500);  // Give modem time to close HTTP
            
            SerialDebug.printf("\r\n========== CYCLE %d COMPLETE ==========\r\n", post_cnt);
            
            // Put modem to sleep - this waits for WAKETIM+1 seconds
            putModemToSleep();
            
            modem_step = SLEEP;
            return;
        }
    }
}

// ==================== SETUP & LOOP ====================

// ==================== WATCHDOG ====================
// Direct IWDG register access for maximum compatibility

// IWDG Key register values
#define IWDG_KEY_RELOAD     0xAAAA  // Reload the counter
#define IWDG_KEY_ENABLE     0xCCCC  // Enable the watchdog
#define IWDG_KEY_WRITE_ACCESS 0x5555 // Enable write access to PR and RLR

void kickWatchdog()
{
    // Reload the watchdog counter by writing key to KR register
    IWDG->KR = IWDG_KEY_RELOAD;
}

void initWatchdog()
{
    // Configure Independent Watchdog via direct register access
    // IWDG runs on ~32kHz LSI clock (varies 30-38kHz)
    // Timeout = (Prescaler * (Reload + 1)) / LSI_freq
    // With prescaler=128 (PR=5) and reload=2500: timeout ≈ 10 seconds
    
    // Enable LSI clock first (required for IWDG)
    RCC->CSR |= RCC_CSR_LSION;
    
    // Wait for LSI to be ready (with timeout)
    uint32_t timeout = 10000;
    while (!(RCC->CSR & RCC_CSR_LSIRDY) && timeout--) { }
    
    if (timeout == 0) {
        SerialDebug.println("LSI clock failed - watchdog disabled");
        return;
    }
    
    // Enable write access to IWDG_PR and IWDG_RLR
    IWDG->KR = IWDG_KEY_WRITE_ACCESS;
    
    // Set prescaler to 128 (PR = 5)
    // 0=4, 1=8, 2=16, 3=32, 4=64, 5=128, 6=256, 7=256
    IWDG->PR = 5;
    
    // Set reload value (12-bit max = 4095)
    // Timeout = 128 * 2500 / 32000 ≈ 10 seconds  
    IWDG->RLR = 2500;
    
    // Wait for registers to be updated (with timeout)
    timeout = 10000;
    while ((IWDG->SR != 0) && timeout--) { }
    
    // Enable the watchdog
    IWDG->KR = IWDG_KEY_ENABLE;
    
    // Initial reload
    kickWatchdog();
    
    SerialDebug.printf("Watchdog enabled (%ds timeout)\r\n", WATCHDOG_TIMEOUT_SEC);
}

void setup()
{
    SerialDebug.begin(115200);
    SerialOutput.begin(115200);
    
    // Turn off the onboard LED (usually PA4 or LED_BUILTIN)
    #ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    #endif
    
    // Get STM32 unique device ID and seed random generator
    device_uid = *(uint32_t*)(STM32_UID_BASE);
    randomSeed(device_uid ^ *(uint32_t*)(STM32_UID_BASE + 4) ^ *(uint32_t*)(STM32_UID_BASE + 8));
    
    SerialDebug.println("\r\n========================================");
    SerialDebug.println("  WindNerd Core 4G - Air780E");
    SerialDebug.println("  Dual Upload: WTP + Windguru");
    SerialDebug.println("  LOW POWER MODE");
    SerialDebug.println("========================================");
    SerialDebug.printf("Report interval: %d minutes\r\n", REPORT_INTERVAL_MN);
    SerialDebug.printf("Modem reset every: %d cycles\r\n", MODEM_RESET_CYCLES);
    SerialDebug.printf("Modem baud: %d\r\n", MODEM_BAUD_RATE);

    Anemometer.begin();
    // Apply compile-time inversion setting
    Anemometer.invertVanePolarity(INVERT_VANE_POLARITY);
#if INVERT_VANE_POLARITY
    SerialDebug.println("Vane polarity inversion ENABLED (180°)");
#else
    SerialDebug.println("Vane polarity inversion DISABLED");
#endif
    
    // Wait for modem to boot (2-3 seconds)
    SerialDebug.println("Waiting for modem boot...");
    delay(3000);
    
    // Initialize modem - try 115200 first, then switch to 9600
    // NOTE: No RX connected, so we can't verify responses
    SerialDebug.println("Initializing modem...");
    
    // Send AT at 115200 (factory default)
    SerialOutput.println("AT");
    delay(200);
    SerialOutput.println("AT");
    delay(200);
    
    // Switch modem to 9600 baud for reliable sleep/wake
    SerialDebug.printf("Setting modem to %d baud...\r\n", MODEM_BAUD_RATE);
    SerialOutput.printf("AT+IPR=%d\r\n", MODEM_BAUD_RATE);
    delay(500);  // Give modem time to process
    
    // Switch MCU serial to match
    SerialOutput.end();
    delay(100);
    SerialOutput.begin(MODEM_BAUD_RATE);
    delay(300);
    
    // Send AT at new baud to verify (modem will respond even if we can't read it)
    SerialOutput.println("AT");
    delay(200);
    SerialOutput.println("AT");
    delay(200);
    
    SerialDebug.printf("Modem baud set to %d (assumed OK)\r\n", MODEM_BAUD_RATE);
    
    // Configure modem sleep parameters (but don't enable sleep yet)
    configureModemSleep();
    
    // Initialize watchdog AFTER all setup is complete
    initWatchdog();
    
    // Set first upload to happen after FIRST_UPLOAD_DELAY_SEC
    // This gives modem time to connect before first upload, then sleep
    last_uploading_time = millis() - (REPORT_INTERVAL_MN * 60 * 1000UL) + (FIRST_UPLOAD_DELAY_SEC * 1000UL);
    
    delay(1000);
    SerialDebug.printf("System ready! First upload in %d seconds\r\n", FIRST_UPLOAD_DELAY_SEC);
}

void loop()
{
    Anemometer.loop();
    
    // Kick watchdog to prevent reset
    kickWatchdog();

    // Start upload cycle when interval expires
    if (modem_step == SLEEP && (millis() - last_uploading_time > (REPORT_INTERVAL_MN * 60 * 1000UL)))
    {
        SerialDebug.println("\r\n>>> UPLOAD INTERVAL EXPIRED <<<");
        
        // Wake modem from sleep
        wakeModem();
        
        // Start the upload cycle
        modem_step = START;
        last_uploading_time = millis();
        cycle_start_time = millis();
    }
    
    // Process modem state machine
    processModem();

    // MCU light sleep - millis() keeps running, wakes on any interrupt
    HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI);
}

// ==================== CLOCK CONFIG (8MHz for low power) ====================

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV2;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

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