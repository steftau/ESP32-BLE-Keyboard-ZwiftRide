# ESP32 BLE Keyboard library with ESP32 Program as a Zwift Ride BLE keyboard

This library fork allows you to compile @fuenfachsen's Zword_ZwiftRide-to-BLE-Keyboard.
https://github.com/Fuenfachsen/Zword_ZwiftRide-to-BLE-Keyboard

Lines 105 and 116 changed in ESP32_BLE_Keyboard/BleKeyboard.cpp

LINE 106: BLEDevice::init(deviceName.c_str());
...
LINE 117: hid->manufacturer()->setValue(deviceManufacturer.c_str()); 

See original issue: https://github.com/T-vK/ESP32-BLE-Keyboard/issues/291

## Zword example: ESP32 as a Zwift Ride BLE keyboard

A copy of the Zword sketch is included here under [`Zword_v0-0-2`](Zword_v0-0-2/Zword_v0-0-2.ino) so it can be opened straight from the Arduino IDE ("File" -> "Examples" -> "ESP32 BLE Keyboard" -> "Zword") after installing this library - no separate download needed.

Zword turns an ESP32 into a bridge between a **Zwift Ride** controller and a training app: it connects to the Ride controller over BLE as a client, reads its button-press notifications, and re-sends them as a standard **BLE keyboard** (advertised as "Zword") that any app can pair with like a regular keyboard. It's set up for the shortcuts of the **MyWhoosh** app. The Zwift Ride's "paddle" is intentionally ignored, and only the left/right Ride controllers are supported (not Zwift Play - those use encrypted messages).

**Connection sequence:**
1. Pair "Zword" as a Bluetooth keyboard on your PC/Tablet/Phone once (Bluetooth settings), then power off the ESP32.
2. For normal use: turn on the LEFT Ride controller (flashes blue), power on the ESP32, wait for the LEFT controller to vibrate once (connected) or twice (keyboard also connected), then turn on the RIGHT controller (flashes blue, then solid).
3. Open your training app - it now receives the button presses below as keyboard input.

**Button mapping:**

| Left controller | Action | Right controller | Action |
|---|---|---|---|
| Any side button | Shift Down | Any side button | Shift Up |
| Power | Play/Pause | A | Hello emote |
| Left / Right | Prev. / Next track | Y | Hide/Show UI |
| Up / Down | Volume up / down | B | Battery Low emote |
| | | Z | Thumbs Up emote |
| | | Power | Pause/Resume ride |

Progress is mirrored on the Serial monitor (115200 baud) while it's running.

### Zword: connection stops working after a Zwift Ride firmware update

Zwift occasionally pushes a firmware update to the Ride controllers that can change the BLE service UUID the sketch relies on to find them. If the ESP32 used to connect fine and suddenly can't find the controller anymore, that's the most likely cause. The sketch only ever auto-connects based on that known service UUID - it never guesses by device name, since a full Zwift Ride setup has other BLE hardware nearby (e.g. a KICKR CORE trainer) that must not be mistaken for the Ride controller.

No re-flash needed to fix it: open the Arduino IDE's Serial monitor (115200 baud) - the sketch prints its available commands on boot:
- `?scan` - scans for 10s and logs every nearby BLE device (name, address, advertised service UUIDs), so you can identify the Ride controller among your other Bluetooth hardware and see what UUID it advertises now.
- `!uuid=<uuid>` - sets and **persists** (survives reboot) a new Zwift service UUID and immediately reconnects with it, e.g. `!uuid=0000FC82-0000-1000-8000-00805F9B34FB`.

If it manages to connect but the known GATT service still isn't found, all service UUIDs actually present on the device are printed too - set the correct one with `!uuid=`.


----------------------------------------------------------------------------------------------------------
Changes only in 

You might also be interested in:
- [ESP32-BLE-Mouse](https://github.com/T-vK/ESP32-BLE-Mouse)
- [ESP32-BLE-Gamepad](https://github.com/lemmingDev/ESP32-BLE-Gamepad)


## Features

 - [x] Send key strokes
 - [x] Send text
 - [x] Press/release individual keys
 - [x] Media keys are supported
 - [ ] Read Numlock/Capslock/Scrolllock state
 - [x] Set battery level (basically works, but doesn't show up in Android's status bar)
 - [x] Compatible with Android
 - [x] Compatible with Windows
 - [x] Compatible with Linux
 - [x] Compatible with MacOS X (not stable, some people have issues, doesn't work with old devices)
 - [x] Compatible with iOS (not stable, some people have issues, doesn't work with old devices)

## Installation
- (Make sure you can use the ESP32 with the Arduino IDE. [Instructions can be found here.](https://github.com/espressif/arduino-esp32#installation-instructions))
- [Download the latest release of this library from the release page.](https://github.com/T-vK/ESP32-BLE-Keyboard/releases)
- In the Arduino IDE go to "Sketch" -> "Include Library" -> "Add .ZIP Library..." and select the file you just downloaded.
- You can now go to "File" -> "Examples" -> "ESP32 BLE Keyboard" and select any of the examples to get started.

## Example

``` C++
/**
 * This example turns the ESP32 into a Bluetooth LE keyboard that writes the words, presses Enter, presses a media key and then Ctrl+Alt+Delete
 */
#include <BleKeyboard.h>

BleKeyboard bleKeyboard;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  bleKeyboard.begin();
}

void loop() {
  if(bleKeyboard.isConnected()) {
    Serial.println("Sending 'Hello world'...");
    bleKeyboard.print("Hello world");

    delay(1000);

    Serial.println("Sending Enter key...");
    bleKeyboard.write(KEY_RETURN);

    delay(1000);

    Serial.println("Sending Play/Pause media key...");
    bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);

    delay(1000);
    
   //
   // Below is an example of pressing multiple keyboard modifiers 
   // which by default is commented out. 
   // 
   /* Serial.println("Sending Ctrl+Alt+Delete...");
    bleKeyboard.press(KEY_LEFT_CTRL);
    bleKeyboard.press(KEY_LEFT_ALT);
    bleKeyboard.press(KEY_DELETE);
    delay(100);
    bleKeyboard.releaseAll();
    */

  }
  Serial.println("Waiting 5 seconds...");
  delay(5000);
}
```

## API docs
The BleKeyboard interface is almost identical to the Keyboard Interface, so you can use documentation right here:
https://www.arduino.cc/reference/en/language/functions/usb/keyboard/

Just remember that you have to use `bleKeyboard` instead of just `Keyboard` and you need these two lines at the top of your script:
```
#include <BleKeyboard.h>
BleKeyboard bleKeyboard;
```

In addition to that you can send media keys (which is not possible with the USB keyboard library). Supported are the following:
- KEY_MEDIA_NEXT_TRACK
- KEY_MEDIA_PREVIOUS_TRACK
- KEY_MEDIA_STOP
- KEY_MEDIA_PLAY_PAUSE
- KEY_MEDIA_MUTE
- KEY_MEDIA_VOLUME_UP
- KEY_MEDIA_VOLUME_DOWN
- KEY_MEDIA_WWW_HOME
- KEY_MEDIA_LOCAL_MACHINE_BROWSER // Opens "My Computer" on Windows
- KEY_MEDIA_CALCULATOR
- KEY_MEDIA_WWW_BOOKMARKS
- KEY_MEDIA_WWW_SEARCH
- KEY_MEDIA_WWW_STOP
- KEY_MEDIA_WWW_BACK
- KEY_MEDIA_CONSUMER_CONTROL_CONFIGURATION // Media Selection
- KEY_MEDIA_EMAIL_READER

There is also Bluetooth specific information that you can set (optional):
Instead of `BleKeyboard bleKeyboard;` you can do `BleKeyboard bleKeyboard("Bluetooth Device Name", "Bluetooth Device Manufacturer", 100);`. (Max lenght is 15 characters, anything beyond that will be truncated.)  
The third parameter is the initial battery level of your device. To adjust the battery level later on you can simply call e.g.  `bleKeyboard.setBatteryLevel(50)` (set battery level to 50%).  
By default the battery level will be set to 100%, the device name will be `ESP32 Bluetooth Keyboard` and the manufacturer will be `Espressif`.  
There is also a `setDelay` method to set a delay between each key event. E.g. `bleKeyboard.setDelay(10)` (10 milliseconds). The default is `8`.  
This feature is meant to compensate for some applications and devices that can't handle fast input and will skip letters if too many keys are sent in a small time frame.  

## NimBLE-Mode
The NimBLE mode enables a significant saving of RAM and FLASH memory.

### Comparison (SendKeyStrokes.ino at compile-time)

**Standard**
```
RAM:   [=         ]   9.3% (used 30548 bytes from 327680 bytes)
Flash: [========  ]  75.8% (used 994120 bytes from 1310720 bytes)
```

**NimBLE mode**
```
RAM:   [=         ]   8.3% (used 27180 bytes from 327680 bytes)
Flash: [====      ]  44.2% (used 579158 bytes from 1310720 bytes)
```

### Comparison (SendKeyStrokes.ino at run-time)

|   | Standard | NimBLE mode | difference
|---|--:|--:|--:|
| `ESP.getHeapSize()`   | 296.804 | 321.252 | **+ 24.448**  |
| `ESP.getFreeHeap()`   | 143.572 | 260.764 | **+ 117.192** |
| `ESP.getSketchSize()` | 994.224 | 579.264 | **- 414.960** |

## How to activate NimBLE mode?

### ArduinoIDE: 
Uncomment the first line in BleKeyboard.h
```C++
#define USE_NIMBLE
```

### PlatformIO:
Change your `platformio.ini` to the following settings
```ini
lib_deps = 
  NimBLE-Arduino

build_flags = 
  -D USE_NIMBLE
```

## Credits

Credits to [chegewara](https://github.com/chegewara) and [the authors of the USB keyboard library](https://github.com/arduino-libraries/Keyboard/) as this project is heavily based on their work!  
Also, credits to [duke2421](https://github.com/T-vK/ESP32-BLE-Keyboard/issues/1) who helped a lot with testing, debugging and fixing the device descriptor!
And credits to [sivar2311](https://github.com/sivar2311) for adding NimBLE support, greatly reducing the memory footprint, fixing advertising issues and for adding the `setDelay` method.
