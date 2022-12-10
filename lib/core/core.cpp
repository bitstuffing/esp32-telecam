#include "core.h"

//wifi AP part
IPAddress initWifiAP(String wifi_ssid,String wifi_password){  
  WiFi.softAP(wifi_ssid.c_str(), wifi_password.c_str());
  
  Serial.print("wifi ssid: ");
  Serial.print(wifi_ssid);
  Serial.print(", password: ");
  Serial.println(wifi_password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  return IP;
}

/*
* Take a picture and make sure it has a good jpeg
*/
camera_fb_t* get_good_jpeg() {

  camera_fb_t* fb;

  long start;
  int failures = 0;

  do {
    int fblen = 0;
    int foundffd9 = 0;

    fb = esp_camera_fb_get();

    if (!fb) {
      Serial.println("Camera Capture Failed");
      failures++;
    } else {
      int get_fail = 0;
      fblen = fb->len;

      for (int j = 1; j <= 1025; j++) {
        if (fb->buf[fblen - j] != 0xD9) {
        } else {
          if (fb->buf[fblen - j - 1] == 0xFF ) {
            foundffd9 = 1;
            break;
          }
        }
      }

      if (!foundffd9) {
        Serial.printf("Bad jpeg, Frame %d, Len = %d \n", frame_cnt, fblen);
        esp_camera_fb_return(fb);
        failures++;
      } else {
        break;
      }
    }

  } while (failures < 10);   // normally leave the loop with a break()

  // if we get 10 bad frames in a row, then quality parameters are too high - set them lower
  if (failures == 10) {
    Serial.printf("10 failures");
    sensor_t * ss = esp_camera_sensor_get();
    int qual = ss->status.quality ;
    ss->set_quality(ss, qual + 3);
    quality = qual + 3;
    Serial.printf("\n\nDecreasing quality due to frame failures %d -> %d\n\n", qual, qual + 5);
    delay(1000);
  }
  return fb;
}

void the_camera_loop (void* pvParameter) {
  if(flashState == HIGH){
    digitalWrite(FLASH_LED_PIN, flashState);
  }
  
  vid_fb = get_good_jpeg(); // esp_camera_fb_get();
  if (!vid_fb) {
    Serial.println("Camera capture failed");
    return;
  }
  picture_ready = true;

  if (avi_enabled) {
    frame_cnt = 0;

    // start a movie
    avi_start_time = millis();
    Serial.printf("\nStart the avi ... at %d\n", avi_start_time);
    Serial.printf("Framesize %d, quality %d, length %d seconds\n\n", framesize, quality,  max_frames * frame_interval / 1000);

    fb_next = get_good_jpeg(); // should take zero time
    last_frame_time = millis();

    start_avi();

    // all the frames of movie
    for (int j = 0; j < max_frames - 1 ; j++) { // max_frames
      current_frame_time = millis();

      if (current_frame_time - last_frame_time < frame_interval) {
        if (frame_cnt < 5 || frame_cnt > (max_frames - 5) )Serial.printf("frame %d, delay %d\n", frame_cnt, (int) frame_interval - (current_frame_time - last_frame_time));
        delay(frame_interval - (current_frame_time - last_frame_time));             // delay for timelapse
      }

      last_frame_time = millis();
      frame_cnt++;

      if (frame_cnt !=  1) esp_camera_fb_return(fb_curr);
      fb_curr = fb_next; // we will write a frame, and get the camera preparing a new one

      another_save_avi(fb_curr );
      fb_next = get_good_jpeg(); // should take near zero, unless the sd is faster than the camera, when we will have to wait for the camera

      digitalWrite(LED_PIN, frame_cnt % 2);
      if (movi_size > avi_buf_size * .95) break;
    }

    // stop a movie
    Serial.println("End the Avi");

    esp_camera_fb_return(fb_curr);
    frame_cnt++;
    fb_curr = fb_next;
    fb_next = NULL;
    another_save_avi(fb_curr );
    digitalWrite(LED_PIN, frame_cnt % 2);
    esp_camera_fb_return(fb_curr);
    fb_curr = NULL;
    end_avi();                                // end the movie

    avi_end_time = millis();
    float fps = 1.0 * frame_cnt / ((avi_end_time - avi_start_time) / 1000) ;
    Serial.printf("End the avi at %d.  It was %d frames, %d ms at %.2f fps...\n", millis(), frame_cnt, avi_end_time - avi_start_time, fps);
    frame_cnt = 0;             // start recording again on the next loop
    video_ready = true;
  }
  if(!light){
    digitalWrite(FLASH_LED_PIN, LOW); //always reset flash
  }

  Serial.println("Deleting the camera task");
  delay(100);
  //vTaskDelete(the_camera_loop_task);
  vTaskDelete(NULL);
}

/*
* This method opens the files and writes in headers
*/
void start_avi() {

  Serial.println("Starting an avi ");

  time(&now);
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "DoorCam %F %H.%M.%S.avi", &timeinfo);

  //memset(psram_avi_buf, 0, avi_buf_size);  // save some time
  //memset(psram_idx_buf, 0, idx_buf_size);

  psram_avi_ptr = 0;
  psram_idx_ptr = 0;

  memcpy(buf + 0x40, frameSizeData[framesize].frameWidth, 2);
  memcpy(buf + 0xA8, frameSizeData[framesize].frameWidth, 2);
  memcpy(buf + 0x44, frameSizeData[framesize].frameHeight, 2);
  memcpy(buf + 0xAC, frameSizeData[framesize].frameHeight, 2);

  psram_avi_ptr = psram_avi_buf;
  psram_idx_ptr = psram_idx_buf;

  memcpy( psram_avi_ptr, buf, AVIOFFSET);
  psram_avi_ptr += AVIOFFSET;

  startms = millis();

  jpeg_size = 0;
  movi_size = 0;
  uVideoLen = 0;
  idx_offset = 4;

} // end of start avi

/*
* Another_save_avi saves another frame to the avi file, uodates index.
* Pass in a fb pointer to the frame to add
*/
void another_save_avi(camera_fb_t * fb ) {

  int fblen;
  fblen = fb->len;

  int fb_block_length;
  uint8_t* fb_block_start;

  jpeg_size = fblen;

  remnant = (4 - (jpeg_size & 0x00000003)) & 0x00000003;

  long bw = millis();
  long frame_write_start = millis();

  memcpy(psram_avi_ptr, dc_buf, 4);

  int jpeg_size_rem = jpeg_size + remnant;

  print_quartet(jpeg_size_rem, psram_avi_ptr + 4);

  fb_block_start = fb->buf;

  if (fblen > fbs * 1024 - 8 ) {                     // fbs is the size of frame buffer static
    fb_block_length = fbs * 1024;
    fblen = fblen - (fbs * 1024 - 8);
    memcpy( psram_avi_ptr + 8, fb_block_start, fb_block_length - 8);
    fb_block_start = fb_block_start + fb_block_length - 8;
  } else {
    fb_block_length = fblen + 8  + remnant;
    memcpy( psram_avi_ptr + 8, fb_block_start, fb_block_length - 8);
    fblen = 0;
  }

  psram_avi_ptr += fb_block_length;

  while (fblen > 0) {
    if (fblen > fbs * 1024) {
      fb_block_length = fbs * 1024;
      fblen = fblen - fb_block_length;
    } else {
      fb_block_length = fblen  + remnant;
      fblen = 0;
    }

    memcpy( psram_avi_ptr, fb_block_start, fb_block_length);

    psram_avi_ptr += fb_block_length;

    fb_block_start = fb_block_start + fb_block_length;
  }

  movi_size += jpeg_size;
  uVideoLen += jpeg_size;

  print_2quartet(idx_offset, jpeg_size, psram_idx_ptr);
  psram_idx_ptr += 8;

  idx_offset = idx_offset + jpeg_size + remnant + 8;

  movi_size = movi_size + remnant;

} // end of another_pic_avi

/*
* This method writes the index, and closes the files
*/
void end_avi() {

  Serial.println("End of avi - closing the files");

  if (frame_cnt <  5 ) {
    Serial.println("Recording screwed up, less than 5 frames, forget index\n");
  } else {

    elapsedms = millis() - startms;

    float fRealFPS = (1000.0f * (float)frame_cnt) / ((float)elapsedms) * speed_up_factor;

    float fmicroseconds_per_frame = 1000000.0f / fRealFPS;
    uint8_t iAttainedFPS = round(fRealFPS) ;
    uint32_t us_per_frame = round(fmicroseconds_per_frame);

    //Modify the MJPEG header from the beginning of the file, overwriting various placeholders

    print_quartet(movi_size + 240 + 16 * frame_cnt + 8 * frame_cnt, psram_avi_buf + 4);
    print_quartet(us_per_frame, psram_avi_buf + 0x20);

    unsigned long max_bytes_per_sec = (1.0f * movi_size * iAttainedFPS) / frame_cnt;
    print_quartet(max_bytes_per_sec, psram_avi_buf + 0x24);
    print_quartet(frame_cnt, psram_avi_buf + 0x30);
    print_quartet(frame_cnt, psram_avi_buf + 0x8c);
    print_quartet((int)iAttainedFPS, psram_avi_buf + 0x84);
    print_quartet(movi_size + frame_cnt * 8 + 4, psram_avi_buf + 0xe8);

    Serial.println(F("\n*** Video recorded and saved ***\n"));

    Serial.printf("Recorded %5d frames in %5d seconds\n", frame_cnt, elapsedms / 1000);
    Serial.printf("File size is %u bytes\n", movi_size + 12 * frame_cnt + 4);
    Serial.printf("Adjusted FPS is %5.2f\n", fRealFPS);
    Serial.printf("Max data rate is %lu bytes/s\n", max_bytes_per_sec);
    Serial.printf("Frame duration is %d us\n", us_per_frame);
    Serial.printf("Average frame length is %d bytes\n", uVideoLen / frame_cnt);

    Serial.printf("Writng the index, %d frames\n", frame_cnt);

    memcpy (psram_avi_ptr, idx1_buf, 4);
    psram_avi_ptr += 4;

    print_quartet(frame_cnt * 16, psram_avi_ptr);
    psram_avi_ptr += 4;

    psram_idx_ptr = psram_idx_buf;

    for (int i = 0; i < frame_cnt; i++) {
      memcpy (psram_avi_ptr, dc_buf, 4);
      psram_avi_ptr += 4;
      memcpy (psram_avi_ptr, zero_buf, 4);
      psram_avi_ptr += 4;

      memcpy (psram_avi_ptr, psram_idx_ptr, 8);
      psram_avi_ptr += 8;
      psram_idx_ptr += 8;
    }
  }

  Serial.println("---");
  digitalWrite(LED_PIN, HIGH);
}

/**
* Main Telegram handler, if you needs to edit or write new command
* this is the place.
*/
void handleNewMessages(int numNewMessages) {

  for (int i = 0; i < numNewMessages; i++) {
    chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    Serial.printf("\nGot a message %s\n", text);
    
    String from_name = bot.messages[i].from_name;
    String username = bot.messages[i].username;
    if (from_name == "") from_name = "Guest";

    Serial.print("message from: ");
    Serial.println(username);
    
    String hi = "Got: ";
    hi += text;
    bot.sendMessage(chat_id, hi, "Markdown");
    client.setHandshakeTimeout(120000);

    boolean auth = false;
    if(authUsers!=nullptr){
      for(int i=0;!auth && i<NUM_ITEMS(authUsers);i++){
        String target = authUsers[i];
        auth = target != nullptr && target == username;
      }
    }

    if(auth){
      
      Serial.print(username);
      Serial.println(" is authorized, continue...");

      if (text==("/light")){
        light = !light;
      }else if (text==("/flash")) {
        flashState = flashState == HIGH ? LOW : HIGH;
        //TODO write status changed
      }else if (text==("/status")) {
        String stat = "Device: " + devstr + "\nVer: " + String(vernum) + "\nRssi: " + String(WiFi.RSSI()) + "\nip: " +  WiFi.localIP().toString() + "\nAvi Enabled: " + avi_enabled;
        if (frame_interval == 0) {
          stat = stat + "\nFast 3 sec";
        } else if (frame_interval == 125) {
          stat = stat + "\nMed 10 sec";
        } else {
          stat = stat + "\nSlow 40 sec";
        }
        stat = stat + "\nQuality: " + quality;

        bot.sendMessage(chat_id, stat, "");
      }else if(text.startsWith("/sleep")){ 
        sleep_mode = true;
        int timeInt = 1;
        if(text.indexOf(" ")>0){ //custom time
          String time = text.substring(text.indexOf(" "), sizeof(text));
          int multiplier = 1;
          if(time.indexOf(" ")>0){ //get seconds, min, hours, days...
            String unit = time.substring(time.indexOf(" "), sizeof(time));
            if(unit.startsWith("m")){ //minutes
              multiplier = 60;
            }else if(unit.startsWith("h")){ //hours
              multiplier = 60*60;
            }else if(unit.startsWith("d")){ //days
              multiplier = 60*60*24;
            }
          }
          sleepTime = time.toInt() * multiplier;
        }else{
          sleepTime = TIME_TO_SLEEP; //default
        }
      }else if (text==("/reboot")) {
        reboot_request = true;
      }else if (text==("/enavi")) {
        avi_enabled = true;
      }else if(text==("/disavi")) {
        avi_enabled = false;
      }else if (text==("/fast")) {
        max_frames = 50;
        frame_interval = 0;
        speed_up_factor = 0.5;
        avi_enabled = true;
      }else if (text==("/med")) {
        max_frames = 50;
        frame_interval = 125;
        speed_up_factor = 1;
        avi_enabled = true;
      }else if (text==("/slow")) {
        max_frames = 50;
        frame_interval = 500;
        speed_up_factor = 5;
        avi_enabled = true;
      }

      for (int j = 0; j < 4; j++) {
        camera_fb_t * newfb = esp_camera_fb_get();
        if (!newfb) {
          Serial.println("Camera Capture Failed");
        } else {
          esp_camera_fb_return(newfb);
          delay(10);
        }
      }

      if ( text == "/photo" || text == "/caption" ) {

        fb = NULL;

        if(flashState == HIGH){
          digitalWrite(FLASH_LED_PIN, flashState);
        }

        // Take Picture with Camera
        fb = esp_camera_fb_get();

        if (!fb) {
          Serial.println("Camera capture failed");
          bot.sendMessage(chat_id, "Camera capture failed", "");
          return;
        }

        currentByte = 0;
        fb_length = fb->len;
        fb_buffer = fb->buf;

        if(!light){
          delay(2000);
          digitalWrite(FLASH_LED_PIN, LOW); //always reset flash
        }

        if (text == "/caption") {
          Serial.println("\n>>>>> Sending with a caption, bytes=  " + String(fb_length));
          String sent = bot.sendMultipartFormDataToTelegramWithCaption("sendPhoto", "photo", "img.jpg",
                        "image/jpeg", "Your photo", chat_id, fb_length,
                        isMoreDataAvailable, getNextByte, nullptr, nullptr);
          Serial.println("done!");
        } else {
          Serial.println("\n>>>>> Sending, bytes=  " + String(fb_length));
          bot.sendPhotoByBinary(chat_id, "image/jpeg", fb_length,
                                isMoreDataAvailable, getNextByte,
                                nullptr, nullptr);
          dataAvailable = true;
          Serial.println("done!");
        }
        esp_camera_fb_return(fb);
      }else if (text == "/clip") {
        // record the video
        bot.longPoll =  0;
        xTaskCreatePinnedToCore( the_camera_loop, "the_camera_loop", 10000, nullptr, 1, &the_camera_loop_task, 1);
        if ( the_camera_loop_task == nullptr ) {
          //vTaskDelete( xHandle );
          Serial.printf("do_the_steaming_task failed to start! %d\n", the_camera_loop_task);
        }
      }else if (text == "/start") {
        String welcome = "ESP32Cam Telegram bot.\n\n";
        welcome += "/photo: take a photo\n";
        welcome += "/light: toggle LED light status\n";
        welcome += "/flash: toggle flash in photo/video\n";
        welcome += "/caption: photo with caption\n";
        welcome += "/clip: short video clip\n";
        welcome += "\n Configure the clip\n";
        welcome += "/enavi: enable avi\n";
        welcome += "/disavi: disable avi\n";
        welcome += "\n/fast: 25 fps - 3  sec - play .5x speed\n";
        welcome += "/med: 8  fps - 10 sec - play 1x speed\n";
        welcome += "/slow: 2  fps - 40 sec - play 5x speed\n";
        welcome += "\n/status: status\n";
        welcome += "/reboot: reboot\n";
        welcome += "/start: start\n";
        bot.sendMessage(chat_id, welcome, "Markdown");
      }
    }else{
      Serial.print("WARNING!!! Not authorized user tries to send command: ");
      Serial.println(username);
    }
  }
}

bool setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if (psramFound()) {
    config.frame_size = configframesize;
    config.jpeg_quality = qualityconfig;
    config.fb_count = 4;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  //Serial.printf("Internal Total heap %d, internal Free Heap %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
  //Serial.printf("SPIRam Total heap   %d, SPIRam Free Heap   %d\n", ESP.getPsramSize(), ESP.getFreePsram());

  static char * memtmp = (char *) malloc(32 * 1024);
  static char * memtmp2 = (char *) malloc(32 * 1024); //32767

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return false;
  }
  free(memtmp2);
  memtmp2 = NULL;
  free(memtmp);
  memtmp = NULL;
  //Serial.printf("Internal Total heap %d, internal Free Heap %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
  //Serial.printf("SPIRam Total heap   %d, SPIRam Free Heap   %d\n", ESP.getPsramSize(), ESP.getFreePsram());

  sensor_t * s = esp_camera_sensor_get();

  //  drop down frame size for higher initial frame rate
  s->set_framesize(s, (framesize_t)framesize);
  s->set_quality(s, quality);
  delay(200);
  return true;
}

bool init_wifi() {
  uint32_t brown_reg_temp = READ_PERI_REG(RTC_CNTL_BROWN_OUT_REG);
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  if (SPIFFS.exists("/config.json")){

    Serial.println("opening config.json");

    doc = getFileContent("/config.json");

    //if configuration exists try to connect
    if(doc.containsKey("configuration")){
      Serial.println("reading wifi...");
      String ssid = doc["configuration"]["wifi"]["bssid"].as<String>();;
      String password = doc["configuration"]["wifi"]["password"].as<String>();;

      // Attempt to connect to Wifi network:
      Serial.print("Connecting Wifi: ");
      Serial.println(ssid);

      // Set WiFi to station mode and disconnect from an AP if it was Previously connected
      devstr.toCharArray(devname, devstr.length() + 1); // name of your camera for mDNS, Router, and filenames
      WiFi.mode(WIFI_STA);
      WiFi.setHostname(devname);
      WiFi.begin(ssid.c_str(), password.c_str());

      int retries = 0;

      while (retries < LIMIT_RETRIES && WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
        retries++;
      }

    }
  }else{
    Serial.println("FAIL opening config.json");
  }  

  if(WiFi.status() == WL_CONNECTED){ //ntp
    //TODO check with esp_err_t set_ps = returned value, esp_err_t and sys.exit(-1) code
    esp_wifi_set_ps(WIFI_PS_NONE);

    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, brown_reg_temp);

    configTime(0, 0, "pool.ntp.org");
    char tzchar[80];
    TIMEZONE.toCharArray(tzchar, TIMEZONE.length()); // name of your camera for mDNS, Router, and filenames
    setenv("TZ", tzchar, 1);  // mountain time zone from #define at top
    tzset();

    if (!MDNS.begin(devname)) {
      Serial.println("Error setting up MDNS responder!");
      return false;
    } else {
      Serial.printf("mDNS responder started '%s'\n", devname);
    }
    time(&now);

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  return false;
}

void sendPicture() {
  currentByte = 0;
  fb_length = vid_fb->len;
  fb_buffer = vid_fb->buf;

  Serial.println("\n>>>>> Sending as 512 byte blocks, with jzdelay of 0, bytes=  " + String(fb_length));

  String sent = bot.sendMultipartFormDataToTelegramWithCaption("sendPhoto", "photo", "img.jpg",
                  "image/jpeg", "Photo Requested", chat_id, fb_length,
                  isMoreDataAvailable, getNextByte, nullptr, nullptr);
  esp_camera_fb_return(vid_fb);
  bot.longPoll =  0;
  if (!avi_enabled) active_interupt = false;
}

void sendVideo() {
  Serial.println("\n\n\nSending clip with caption");
  Serial.println("\n>>>>> Sending as 512 byte blocks, with a caption, and with jzdelay of 0, bytes=  " + String(psram_avi_ptr - psram_avi_buf));
  avi_buf = psram_avi_buf;

  avi_ptr = 0;
  avi_len = psram_avi_ptr - psram_avi_buf;

  String sent2 = bot.sendMultipartFormDataToTelegramWithCaption("sendDocument", "document", strftime_buf,
                 "image/jpeg", "Video Requested", chat_id, psram_avi_ptr - psram_avi_buf,
                 avi_more, avi_next, nullptr, nullptr);

  Serial.println("done!");

  bot.longPoll = 5;
  active_interupt = false;
}


//FILE SYSTEM FUNCTIONS
bool saveDocument(DynamicJsonDocument doc){
  File configFile = SPIFFS.open("/config.json", FILE_WRITE);
  if (!configFile) {
    Serial.println("Failed to open config file in write mode");
    return false;
  }
  Serial.println("writting to file...");
  if (serializeJson(doc, configFile) == 0) {
    Serial.println("Failed to parse config file");
    return false;
  }
  configFile.close();
  Serial.println("done!");
  return true;
}

bool saveTelegram(AsyncWebServerRequest *request){
  if(request->hasArg("telegramToken") && request->hasArg("telegramToken")){

    //build json document
    doc = getFileContent("/config.json");
    doc["configuration"]["bottoken"] = request->arg("telegramToken");

    String result = saveDocument(doc) ? "ok" : "error";

    StaticJsonDocument<100> data;
    data["result"] = result;
    String response;
    serializeJson(data, response);
    request->send(200, "application/json", response);
  }
  return true;
}

bool saveConnection(AsyncWebServerRequest *request){
  if(request->hasArg("wifiSSID") && request->hasArg("wifiPassword")){
    //build json document
    doc = getFileContent("/config.json");
    
    doc["configuration"]["wifi"]["bssid"] = request->arg("wifiSSID");
    doc["configuration"]["wifi"]["password"] = request->arg("wifiPassword");
    doc["configuration"]["wifi"]["ap"] = request->arg("wifiClient") == "true";
    
    //now store
    
    String result = saveDocument(doc) ? "ok" : "error";

    StaticJsonDocument<100> data;
    data["result"] = result;
    String response;
    serializeJson(data, response);
    request->send(200, "application/json", response);
  }
  return true;
}

void initWebServer(){
  //Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/www/index.html", String(), false, processor);
  });
  server.on("^/([a-zA-z0-9-_.]+)?\\.json$", HTTP_GET, [] (AsyncWebServerRequest *request) {
      String url = request->url();
      request->send(SPIFFS, url, "application/json");
  }).setAuthentication("user", "pass"); //TODO read from config
  server.on("/connect", HTTP_POST, [](AsyncWebServerRequest *request){
    saveConnection(request);
    //connectWifi(request);
    ESP.restart();
  });
  server.on("/getAuthUsers", HTTP_GET, [](AsyncWebServerRequest *request){
    getAuthUsers(request);
  });
  server.on("/addAuthUser", HTTP_POST, [](AsyncWebServerRequest *request){
    addAuthUser(request);
  });
  server.on("/removeAuthUser", HTTP_POST, [](AsyncWebServerRequest *request){
    removeAuthUser(request);
  });
  server.on("/telegram", HTTP_POST, [](AsyncWebServerRequest *request){
    saveTelegram(request);
    ESP.restart();
  });
  //server.serveStatic("/", SPIFFS, "/www");
  server.on("^/([a-zA-z0-9-_.]+)?\\.js$", HTTP_GET, downloadPage);
  server.on("^/([a-zA-z0-9-_.]+)?\\.css$", HTTP_GET, downloadPage);
  server.on("^/([a-zA-z0-9-_.]+)?\\.html$", HTTP_GET, downloadPage);
  // Start server
  server.begin();
}

static void downloadPage(AsyncWebServerRequest* request){
    deferredRequest = request;
}

void getAuthUsers(AsyncWebServerRequest *request){
  int count = NUM_ITEMS(authUsers);
  int size = sizeof(authUsers)*2;
  doc = getFileContent("/config.json");
  String response;  
  serializeJson(doc["configuration"]["authUsers"],response);
  request->send(200, "application/json", response);
}
void addAuthUser(AsyncWebServerRequest *request){
  doc = getFileContent("/config.json");
  String username = request->arg("newUser");
  JsonArray array = doc["configuration"]["authUsers"];
  boolean found = false;
  for(int i=0;!found && i<array.size();i++){
    found = (array[i].as<String>() == username);
  }
  StaticJsonDocument<100> data;
  if(!found){
    array.add(username);
    //doc["configuration"]["authUsers"] = array;
    String result = saveDocument(doc) ? "ok" : "error";
    data["result"] = result;
  }else{
    data["result"] = "repeated";
  }
  String response;
  serializeJson(data, response);
  request->send(200, "application/json", response);
}
void removeAuthUser(AsyncWebServerRequest *request){
  doc = getFileContent("/config.json");
  String username = request->arg("delUser");
  JsonArray array = doc["configuration"]["authUsers"];
  boolean found = false;
  for(int i=0;!found && i<array.size();i++){
    found = (array[i].as<String>() == username);
    if(found){
      array.remove(i);
    }
  }
  StaticJsonDocument<100> data;
  if(found){
    String result = saveDocument(doc) ? "ok" : "error";
    data["result"] = result;
  }else{
    data["result"] = "not_found";
  }
  String response;
  serializeJson(data, response);
  request->send(200, "application/json", response);
}

DynamicJsonDocument getFileContent(String filename){
  File configFile = SPIFFS.open(filename, FILE_READ);
  if (!configFile) {
    Serial.println("Failed to open config file");
  }
  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large"); //ups ;) 
  }
  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);
  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. 
  configFile.readBytes(buf.get(), size);
  auto error = deserializeJson(doc, buf.get());
  if (error) {
    Serial.println("Failed to parse config file");
    Serial.println(error.f_str());
    
  }
  configFile.close();
  return doc;
}

void fillAuthUsers(JsonArray result){
  int count = result.size();
  authUsers = (char**)malloc(count * sizeof(char*));
  for(int i=0;i<count;i++){
    const char* element = result[i].as<const char*>();
    authUsers[i] = (char*) malloc(sizeof(element));
    authUsers[i] = (char*) element;
  }
}

bool connectWifi(AsyncWebServerRequest *request){
  if (SPIFFS.exists("/config.json")){
      doc = getFileContent("/config.json");

      //if configuration exists try to connect
      if(doc.containsKey("wifi")){
        String wifi_ssid = doc["configuration"]["wifi"]["bssid"].as<String>();
        String wifi_password = doc["configuration"]["wifi"]["password"].as<String>();

        ap_enabled = doc["configuration"]["wifi"]["ap"].as<bool>();

        if(ap_enabled){
          initWifiAP(wifi_ssid,wifi_password); //warning, needs to reset ap mode
        }else{
          //Connect to Wi-Fi
          WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
          int counter = 0;
          while (counter++<LIMIT_RETRIES && WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.println("Connecting to WiFi..");
          }
          if(counter++<LIMIT_RETRIES){
            //(debug) print ESP32 Local IP Address
            Serial.println(WiFi.localIP());
          }else{
            Serial.print("Fail to connect. Reset AP...");
            initWifiAP(wifi_ssid,wifi_password); //warning, needs to reset ap mode
            Serial.println(" done!");
          }
        }
        
      } else{
        Serial.println("No wifi data");
      }
    }else if(request){
      request->send(SPIFFS, "/connect.html", String(), false, processor);
      return false;
    }
    return true;
}

String processor(const String& var){
  Serial.println(var);
  if (var == "IP_ADDRESS" ) {
    Serial.println(WiFi.localIP());
    IPAddress address = WiFi.localIP();
    return String() + address[0] + "." + address[1] + "." + address[2] + "." + address[3];
  }
  return String();
}