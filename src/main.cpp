/*******************************************************************
ESP32-CAM-Video-Telegram is a project based on PlatformIO extracted
from UniversalTelegramBot examples.

A Telegram bot for taking a photo and short video with an ESP32Cam
without a SD card use and other streaming features

Parts used:
ESP32-CAM module* - http://s.click.aliexpress.com/e/bnXR1eYs

ESP32-CAM-Video-Telegram is a project based on PlatformIO extracted
from UniversalTelegramBot examples and forked from
https://github.com/jameszah/ESP32-CAM-Video-Telegram, some parts
are licenced under GNU GPLv3 (also avi video encoder), and other parts
has been adapted by @bitstuffing under CC 4.0

You needs to write in configuration file (data/config.json) and fill
your SSID, PASSWORD, BOT_TOKEN and if you want your private Telegram
conversations fill AUTH_USERS
*******************************************************************/
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

  pinMode(LED_PIN, OUTPUT);             // little red led on back of chip
  digitalWrite(LED_PIN, LOW);           // turn on the red LED on the back of chip

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
  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
      Serial.println("SPIFFS Mount Failed");
      return;
  }

  doc = getFileContent("/config.json");
  
  //fill params from config
  ap_enabled = doc["configuration"]["wifi"]["ap"].as<bool>();
  led_is_on = doc["configuration"]["led"].as<bool>();
  flashState = doc["configuration"]["flash"].as<bool>() ? HIGH : LOW;
  light = doc["configuration"]["flash"].as<bool>();
  BOTtoken = doc["configuration"]["bottoken"].as<String>();
  bot.updateToken(BOTtoken);

  //fill auth users from config
  JsonArray result = doc["configuration"]["authUsers"];
  fillAuthUsers(result);

  if(!ap_enabled) {
    

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

  }else{
    String wifi_ssid = doc["configuration"]["wifi"]["bssid"].as<String>();
    String wifi_password = doc["configuration"]["wifi"]["password"].as<String>();

    initWifiAP(wifi_ssid,wifi_password); //warning, needs to reset ap mode
    
  }
  initWebServer();
  doc = NULL;
}

void loop_events(){
  if (reboot_request) {
    String stat = "Rebooting on request\nDevice: " + devstr + "\nVer: " + String(vernum) + "\nRssi: " + String(WiFi.RSSI()) + "\nip: " +  WiFi.localIP().toString() ;
    bot.sendMessage(chat_id, stat, "");
    delay(10000);
    ESP.restart();
  }
  if(sleep_mode){
    digitalWrite(FLASH_LED_PIN,LOW); //always turn off light (preserve batteries / low power consumption)
    bot.sendMessage(chat_id, "Sleeping "+String(TIME_TO_SLEEP)+" seconds", "");
    if(!led_is_on){
      digitalWrite(LED_PIN, LOW);
    }
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }
  digitalWrite(FLASH_LED_PIN,light?HIGH:LOW);

  if (picture_ready) {
    picture_ready = false;
    if(flashState == HIGH){
      digitalWrite(FLASH_LED_PIN,HIGH);
      delay(2000);
    }
    sendPicture();
    if(!light){
      digitalWrite(FLASH_LED_PIN,LOW);
    }
  }

  if (video_ready) {
    video_ready = false;
    sendVideo();
  }
  
  digitalWrite(LED_PIN, led_is_on ? HIGH : LOW);
}

void check_connection(){
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("***** WiFi reconnect *****");
    WiFi.reconnect();
    delay(5000);
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("***** WiFi restart *****");
      if(!init_wifi()){
        Serial.println("sleeping to wait for network connection, router signal could be bad :'(...");
        esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
        esp_deep_sleep_start();
      }
    }
  }
}

void check_messages(){
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  while (numNewMessages) {
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}

/** LOOP **/
void loop() {

  client.setHandshakeTimeout(120000);

  loop_events();

  if(nullptr == doc || !doc["configuration"]["wifi"]["ap"]){
    Serial.println("forcing opening config...");
    doc = getFileContent("/config.json");
  }

  ap_enabled = doc["configuration"]["wifi"]["ap"].as<bool>();

  if (nullptr != deferredRequest){ //trick to win ;)
    deferredRequest->send(SPIFFS, "/www"+deferredRequest->url(), String(), true);
    deferredRequest = nullptr;
  }
  
  if(!ap_enabled) {
    if (millis() > bot_lasttime + BOT_MTBS )  {
      check_connection();
      check_messages();
      bot_lasttime = millis();
    }
  }
  
  delay(500); //reasonable loop timeout
}
