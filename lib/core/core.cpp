#include "core.h"
#include <ESPmDNS.h>

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

  vid_fb = get_good_jpeg(); // esp_camera_fb_get();
  if (!vid_fb) {
    Serial.println("Camera capture failed");
    //bot.sendMessage(chat_id, "Camera capture failed", "");
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

      digitalWrite(33, frame_cnt % 2);
      if (movi_size > avi_buf_size * .95) break;
    }

    // stop a movie
    Serial.println("End the Avi");

    esp_camera_fb_return(fb_curr);
    frame_cnt++;
    fb_curr = fb_next;
    fb_next = NULL;
    another_save_avi(fb_curr );
    digitalWrite(33, frame_cnt % 2);
    esp_camera_fb_return(fb_curr);
    fb_curr = NULL;
    end_avi();                                // end the movie
    digitalWrite(33, HIGH);          // light off
    avi_end_time = millis();
    float fps = 1.0 * frame_cnt / ((avi_end_time - avi_start_time) / 1000) ;
    Serial.printf("End the avi at %d.  It was %d frames, %d ms at %.2f fps...\n", millis(), frame_cnt, avi_end_time - avi_start_time, fps);
    frame_cnt = 0;             // start recording again on the next loop
    video_ready = true;
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
  digitalWrite(33, HIGH);
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
    if (from_name == "") from_name = "Guest";

    String hi = "Got: ";
    hi += text;
    bot.sendMessage(chat_id, hi, "Markdown");
    client.setHandshakeTimeout(120000);
    if (text == "/flash") {
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
    }

    if (text == "/status") {
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
    }

    if (text == "/reboot") {
      reboot_request = true;
    }
    if (text == "/enavi") {
      avi_enabled = true;
    }

    if (text == "/disavi") {
      avi_enabled = false;
    }

    if (text == "/fast") {
      max_frames = 50;
      frame_interval = 0;
      speed_up_factor = 0.5;
      avi_enabled = true;
    }

    if (text == "/med") {
      max_frames = 50;
      frame_interval = 125;
      speed_up_factor = 1;
      avi_enabled = true;
    }

    if (text == "/slow") {
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

      xTaskCreatePinnedToCore( the_camera_loop, "the_camera_loop", 10000, NULL, 1, &the_camera_loop_task, 1);
      //xTaskCreatePinnedToCore( the_camera_loop, "the_camera_loop", 10000, NULL, 1, &the_camera_loop_task, 0);  //v8.5

      if ( the_camera_loop_task == NULL ) {
        //vTaskDelete( xHandle );
        Serial.printf("do_the_steaming_task failed to start! %d\n", the_camera_loop_task);
      }
    }else if (text == "/start") {
      String welcome = "ESP32Cam Telegram bot.\n\n";
      welcome += "/photo: take a photo\n";
      welcome += "/flash: toggle flash LED\n";
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
  // Attempt to connect to Wifi network:
  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);

  // Set WiFi to station mode and disconnect from an AP if it was Previously connected
  devstr.toCharArray(devname, devstr.length() + 1); // name of your camera for mDNS, Router, and filenames
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(devname);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

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

void sendPicture() {
  digitalWrite(33, LOW); // back light on
  currentByte = 0;
  fb_length = vid_fb->len;
  fb_buffer = vid_fb->buf;

  Serial.println("\n>>>>> Sending as 512 byte blocks, with jzdelay of 0, bytes=  " + String(fb_length));

  String sent = bot.sendMultipartFormDataToTelegramWithCaption("sendPhoto", "photo", "img.jpg",
                  "image/jpeg", "Photo Requested", chat_id, fb_length,
                  isMoreDataAvailable, getNextByte, nullptr, nullptr);
  esp_camera_fb_return(vid_fb);
  bot.longPoll =  0;
  digitalWrite(33, HIGH); // back light off
  if (!avi_enabled) active_interupt = false;
}

void sendVideo() {
  digitalWrite(33, LOW); // back light on
  Serial.println("\n\n\nSending clip with caption");
  Serial.println("\n>>>>> Sending as 512 byte blocks, with a caption, and with jzdelay of 0, bytes=  " + String(psram_avi_ptr - psram_avi_buf));
  avi_buf = psram_avi_buf;

  avi_ptr = 0;
  avi_len = psram_avi_ptr - psram_avi_buf;

  String sent2 = bot.sendMultipartFormDataToTelegramWithCaption("sendDocument", "document", strftime_buf,
                 "image/jpeg", "Video Requested", chat_id, psram_avi_ptr - psram_avi_buf,
                 avi_more, avi_next, nullptr, nullptr);

  Serial.println("done!");
  digitalWrite(33, HIGH); // back light off

  bot.longPoll = 5;
  active_interupt = false;
}
