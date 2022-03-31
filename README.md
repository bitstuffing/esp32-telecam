# ESP32 TeleCam
This project is based on PlatformIO and a chinese cheap ESP32-CAM to get a video and photos sender on this device, supporting networking and Telegram bots.

It's a working developer revision, but you could test and use it because is a **working** one and some features are taking into account to be developed in future

 ##Future features
 - Wifi AP with web configurator like all IoT
 - Sleeping mode to improve battery life
 - Hardware trigger (motion, light, water, distance...)

## Configuration
At this revision it's needed a configuration file stored at lib/core/configuration.h. for wifi and telegram credentials (API_TOKEN and SSID:PASSWORD)

## Licence
Some parts are licenced under GNU GPLv3 (also avi video encoder), and other parts has been adapted by @bitstuffing to the community under CC 4.0. You must reference the authors before use it and don't use in a business environment without explicit permision of the author.

## Based on the work of:
This project has been adapted to PlatformIO and developed by @bitstuffing, but all works has started from some repositories (to be mentioned):

https://github.com/jameszah/ESP32-CAM-Video-Telegram

https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
