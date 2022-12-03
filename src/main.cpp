/*******************************************************************
ESP32-CAM-Video-Telegram is a project based on PlatformIO extracted
from UniversalTelegramBot examples.

A Telegram bot for taking a photo and short video with an ESP32Cam
without SD card use and other streaming features

Parts used:
ESP32-CAM module* - http://s.click.aliexpress.com/e/bnXR1eYs

ESP32-CAM-Video-Telegram is a project based on PlatformIO extracted
from UniversalTelegramBot examples and forked from
https://github.com/jameszah/ESP32-CAM-Video-Telegram, some parts
are licenced under GNU GPLv3 (also avi video encoder), and other parts
has been adapted by @bitstuffing under CC 4.0

You needs to write in configuration file (lib/core/configuration.h) and fill
your SSID, PASSWORD, BOT_TOKEN and if you want your private Telegram CHAT_ID
*******************************************************************/
#include "Arduino.h"
#include "core.cpp"

/** SETUP **/
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Serial.begin(115200);
  Serial.println("------------------------------------");
  Serial.printf("- ESP32-CAM Video-Telegram %s -\n", vernum);
  Serial.println("------------------------------------");
  Serial.println("--------- AVAILABLE MEMORY ---------");
  Serial.println("------------------------------------");
  Serial.print("- Total heap: "); Serial.println(ESP.getHeapSize());
  Serial.print("\n- Free heap: "); Serial.println(ESP.getFreeHeap());
  Serial.print("\n- Total PSRAM: "); Serial.println(ESP.getPsramSize());
  Serial.print("\n- Free PSRAM: "); Serial.println(ESP.getFreePsram());
  Serial.print("\n- Connection times: ");Serial.println(bootCount);
  Serial.println("------------------------------------");

  bootCount++;

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, flashState); //defaults to low

  pinMode(12, INPUT_PULLUP);        // pull this down to stop recording

  pinMode(33, OUTPUT);             // little red led on back of chip
  digitalWrite(33, LOW);           // turn on the red LED on the back of chip

  avi_buf_size = 1000 * 1024; // = 1000 kb = 60 * 50 * 1024;
  idx_buf_size = 200 * 10 + 20;
  psram_avi_buf = (uint8_t*)ps_malloc(avi_buf_size);
  if (psram_avi_buf == 0) Serial.printf("psram_avi allocation failed\n");
  psram_idx_buf = (uint8_t*)ps_malloc(idx_buf_size); // save file in psram
  if (psram_idx_buf == 0) Serial.printf("psram_idx allocation failed\n");

  if (!setupCamera()) {
    Serial.println("Camera Setup Failed!");
    while (true) {
      delay(100);
    }
  }

  for (int j = 0; j < 7; j++) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera Capture Failed");
    } else {
      Serial.print("Pic, len="); Serial.print(fb->len);
      Serial.printf(", new fb %X\n", (long)fb->buf);
      esp_camera_fb_return(fb);
      
      delay(20);
    }
  }

  if(init_wifi()){
    bot.longPoll = 5; //Make the bot wait for a new message 5 seconds
    client.setInsecure();
    String stat = "Reboot\nDevice: " + devstr + "\nVer: " + String(vernum) + "\nRssi: " + String(WiFi.RSSI()) + "\nIP: " +  WiFi.localIP().toString() + "\n/start";
    bot.sendMessage(chat_id, stat, "");
    avi_enabled = true;
  }else{
    Serial.println("sleeping to wait for network connection router...");
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }
}

/** LOOP **/
void loop() {

  client.setHandshakeTimeout(120000);

  if (reboot_request) {
    String stat = "Rebooting on request\nDevice: " + devstr + "\nVer: " + String(vernum) + "\nRssi: " + String(WiFi.RSSI()) + "\nip: " +  WiFi.localIP().toString() ;
    bot.sendMessage(chat_id, stat, "");
    delay(10000);
    ESP.restart();
  }
  if(sleep_mode){
    bot.sendMessage(chat_id, "Sleeping "+String(TIME_TO_SLEEP)+" seconds", "");
    digitalWrite(33, LOW);
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }

  if (picture_ready) {
    picture_ready = false;
    sendPicture();
  }

  if (video_ready) {
    video_ready = false;
    sendVideo();
  }

  
  digitalWrite(33, led_is_on ? HIGH : LOW);
  
  
  if (millis() > bot_lasttime + BOT_MTBS )  {

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("***** WiFi reconnect *****");
      WiFi.reconnect();
      delay(5000);
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("***** WiFi restart *****");
        if(!init_wifi()){
          Serial.println("sleeping to wait for network connection router...");
          esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
          esp_deep_sleep_start();
        }
      }
    }

    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    bot_lasttime = millis();
  }
  
  delay(500); //reasonable loop timeout
}
