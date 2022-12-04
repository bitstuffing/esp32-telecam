# ESP32 TeleCam
This project is based on PlatformIO and a chinese cheap ESP32-CAM to get a video and photos sender on this device.

In the last 0.1 revision this project was supporting networking and Telegram bots. Now it supports a dashboard and remote server connections.

## Current features

 - Wifi AP with web configurator like all IoT
 - Web servo motion ( dashboard with servo driver )
 - Sleeping mode to improve battery life
 - LED status (sleeping, working...)
 - User privilege control (no intrussions will request the control of the hardware)

## Future Features

 - Motion and focus detected target following
 - Hardware trigger (motion, light, water, distance...)
 - Battery monitor
 - REST API & native mobile apps

## 3D web model (OnShape CAD)

You're free to change and print the 3D model following the next link:

https://cad.onshape.com/documents/77f04ae4b5fcef7d9e99be35/w/0d8533bbdae142046f8fdeea/e/9711b1c9e78f932d2447bd4e?renderMode=0&uiState=638a0af07e7369624d2191ba

## Preview

![ESP32-TELECAM](https://i.ibb.co/N2p3gpr/esp32-telecam.png)

## Configuration

Use the web panel (dashboard) from your browser to configure dashboard. 
Default credentials are "admin" / "12345678"

## Licence
Some parts are licenced under GNU GPLv3 (also avi video encoder), and other parts has been adapted by @bitstuffing to the community under CC 4.0. You must reference the authors before use it and don't use in a business environment without explicit permision of the author.

## Based on the work of:
This project has been adapted to PlatformIO and developed by @bitstuffing, but all works have started from some repositories (to be mentioned):

https://github.com/jameszah/ESP32-CAM-Video-Telegram

https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
