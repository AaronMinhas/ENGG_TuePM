# Current UI design

<img width="2380" height="1115" alt="image" src="https://github.com/user-attachments/assets/dd3e76e6-b5ba-49d3-a1b4-350abc8b1162" />

# Intro
This guide should help get your own local VSCode Workspace setup with the required extensions for work within this ENGG2000/3000 Unit!

## VSCode Setup
1. Download [git](https://git-scm.com/downloads), and [VSCode](https://code.visualstudio.com/download) if you haven't already installed them on your machine.
2. Clone this repo to your local machine
3. Open VSCode and select File > "Open Workspace from Folder"
4. When prompted, accept and download recommended extensions for this workspace
5. To finish installing PlatformIO, on the left hand menu select the alien icon for PlatformIO
6. Once done, reload your VSCode window

## Testing your code (connecting with the ESP)
1. Connect the ESP32 to your device.
2. Create a credentials.h file and populate it with your Wifi's credentials: (ensure your wifi is 2.4ghz compatible)
```
#pragma once

#define WIFI_SSID "xxxx"
#define WIFI_PASSWORD "xxxx"
```
3. Using PlatformIO build the code then upload and monitor it, you can do this by clicking the alien icon on the left side of your screen. This will open the serial monitor.
4. If everything is successful you will see an output similar to the following:
```
WiFi connected successfully!
IP address: 172.20.10.13
```
5. Using the IP address provided in the serial output you need to modify two files. `frontend/src/types/GenTypes.ts` and `frontend/src/Apps.tsx`
6. In `GenTypes.ts` you will create a new alias with the IP, like: `AARON_2 = "ws://172.20.10.13/ws",`
7. In `Apps.tsx` you need to modify the following to use the alias created:
```
  useEffect(() => {
    const client = getESPClient(IP.AARON_2);
    client.onStatus(setWsStatus);

where XXXX in getESPCLient(IP.XXXX) is the new alias.
```
8. From here you need to deploy the frontend server locally, you can do this by inputting the following in your terminal:
```
# First ensure you are in the root directory of the project then enter:
cd frontend
npm run dev
```
9. Upon deployment you will click the link provided, it will look similar to: `Local:   http://localhost:5173/`
10. If everything is successful you will see `Client # connected` in the serial output.
11. If you make any changes to the ESP code you will need to build and upload it. Any frontend changes reflect immediately.

**Further guidance:**
Ridiculously helpful [guide](https://randomnerdtutorials.com/vs-code-platformio-ide-esp32-esp8266-arduino/)

## Credits
Toast for their wonderful README which helped alot <3
