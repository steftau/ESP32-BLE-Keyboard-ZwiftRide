#include <BLEDevice.h>
#include <BleKeyboard.h>
#include <Preferences.h>

//General Options
bool enableHapticFeedback = false; //Vibration when Zwift Ride button is pressed. Initial "Connection Completed" is not affected

// Troubleshooting: connection stops working after a Zwift Ride firmware update
// ------------------------------------------------------------------------------
// Zwift occasionally pushes firmware updates to the Ride controllers that can
// change the BLE service UUID used to find them. If the ESP32 used to connect
// fine and suddenly can't find the controller anymore, open the Serial monitor
// (115200 baud) and use these commands (also printed on boot):
//  - "?scan"          Scans for 10s and logs every nearby BLE device (name,
//                      address, advertised service UUIDs) - this never
//                      auto-connects by guessing from the name, since a full
//                      Zwift Ride setup has other BLE hardware nearby (e.g. a
//                      KICKR CORE trainer) that must not be mistaken for the
//                      Ride controller. Use it to find the controller among
//                      your other Bluetooth devices and see what UUID it
//                      advertises now.
//  - "!uuid=<uuid>"    Sets and persists (survives reboot) a new Zwift service
//                      UUID and immediately restarts scanning with it - no
//                      re-flash needed. Example:
//                      !uuid=0000FC82-0000-1000-8000-00805F9B34FB
// If the connection to the GATT service still fails after connecting with a
// matching service UUID, all service UUIDs actually present on the device are
// printed too.


// BLE Keyboard initialization
BleKeyboard bleKeyboard("Zword", "Derguitarjo", 100);

//ZwiftRide Bluetooth UUIDs
const char* DEFAULT_ZWIFT_CUSTOM_SERVICE_UUID = "0000FC82-0000-1000-8000-00805F9B34FB";
String zwiftServiceUUID; // loaded from NVS in setup(), can be changed at runtime via the "!uuid=" Serial command
Preferences preferences;
const char* ZWIFT_ASYNC_CHARACTERISTIC_UUID = "00000002-19CA-4651-86E5-FA29DCDD09D1"; //Notify
const char *ZWIFT_SYNC_RX_CHARACTERISTIC_UUID = "00000003-19CA-4651-86E5-FA29DCDD09D1"; //Write Without Response
const char *ZWIFT_SYNC_TX_CHARACTERISTIC_UUID = "00000004-19CA-4651-86E5-FA29DCDD09D1"; //Indicate, Read
const char *ZWIFT_Unknown_CHARACTERISTIC_UUID = "00000006-19CA-4651-86E5-FA29DCDD09D1"; //Indicate, Read, Write, Write Without Response


BLEAdvertisedDevice* myDevice;
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pNotifyCharacteristic;
BLERemoteCharacteristic* pRxCharacteristic;
BLERemoteCharacteristic* pTxCharacteristic;

//Helpers
bool doConnect = false;
bool connected = false;
bool subscribed = false;
bool sentMessage = false;

volatile bool shouldVibrate = false;

//Button Logic helpers
uint8_t prevData2 = 0xFF;
uint8_t prevData3 = 0xFF;
uint8_t prevData4 = 0xFF;

String notification = "";

///////////////////
//BUTTON MAPPINGS//
///////////////////
// Mywoosh
#define KEY_PAUSE       ' '   // Space = Pause
#define KEY_HIDE_UI     'u'   // Hide/unhide UI
#define KEY_SHIFT_DOWN  'i'   // Shift down
#define KEY_SHIFT_UP    'k'   // Shift up
#define KEY_PEACE       '1'   // Peace emote
#define KEY_HELLO       '2'   // Hello emote
#define KEY_POWER       '3'   // Power emote
#define KEY_DAB         '4'   // Dab emote
#define KEY_AGAIN       '5'   // Again emote
#define KEY_BATTERY_LOW '6'   // Battery low emote
#define KEY_THUMBS_UP   '7'   // Thumbs up emote
#define KEY_MEDIA       '<'   //unhappy use as a flag


struct ButtonMapping {
  uint8_t code;        // From pData[x]
  char key;
};

//Character Mapping
const ButtonMapping pData2Mappings[] = {
  {0xFD, KEY_MEDIA},              // Left Controller Up
  {0xF7, KEY_MEDIA},              // Left Controller Down
  {0xFE, KEY_MEDIA},              // Left Controller Left
  {0xFB, KEY_MEDIA},              // Left Controller Right
  {0xEF, KEY_HELLO},              // Right Controller A Button
  {0xBF, KEY_HIDE_UI},            // Right Controller Y Button
  {0xDF, KEY_BATTERY_LOW},        // Right Controller B Button
};

const ButtonMapping pData3Mappings[] = {
  {0xEF, KEY_MEDIA},              // Left Controller Power
  {0xFD, KEY_SHIFT_DOWN},         // Left Controller Side Upper Button
  {0xFB, KEY_SHIFT_DOWN},         // Left Controller Side Middle Button
  {0xF7, KEY_SHIFT_DOWN},         // Left Controller Side Lower Button
  {0xFE, KEY_THUMBS_UP},          // Right Controller Z Button
  {0xDF, KEY_SHIFT_UP },          // Right Controller Side Upper Button
  {0xBF, KEY_SHIFT_UP },          // Right Controller Side Middle Button
};

const ButtonMapping pData4Mappings[] = {
  {0xFD, KEY_PAUSE},              // Right Controller Power Button
  {0xFE, KEY_SHIFT_UP },          // Right Controller Side Lower Button
};


class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {

  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.println("scanning...");

    // Diagnostic only, doesn't affect connecting: log every device seen during
    // scanning (name, address, advertised service UUIDs). If the ESP32 ever
    // stops finding the Zwift Ride controller again (e.g. after a controller
    // firmware update changes its service UUID), this log lets you see what's
    // actually being advertised nearby - including other Zwift Ride hardware
    // setup devices like a KICKR CORE trainer, which use a completely
    // different UUID and can be told apart by name - and set the new one live
    // via the "!uuid=" Serial command instead of guessing blind.
    Serial.print("  seen: ");
    Serial.print(advertisedDevice.haveName() ? advertisedDevice.getName().c_str() : "(no name)");
    Serial.print(" [");
    Serial.print(advertisedDevice.getAddress().toString().c_str());
    Serial.print("]");
    if (advertisedDevice.haveServiceUUID()) {
      Serial.print(" UUIDs: ");
      for (int i = 0; i < advertisedDevice.getServiceUUIDCount(); i++) {
        Serial.print(advertisedDevice.getServiceUUID(i).toString().c_str());
        Serial.print(" ");
      }
    }
    Serial.println();

    // Only ever auto-connect based on the known Zwift hardware/manufacturer
    // service UUID - never guess based on name, since a full Zwift Ride setup
    // has other BLE devices nearby (e.g. a KICKR CORE trainer) that must not
    // be mistaken for the Ride controller.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(zwiftServiceUUID.c_str()))) {
      Serial.println("Found Zwift Ride");
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
    }
  }
};

char getMappedKey(const ButtonMapping* mappings, size_t size, uint8_t code) {
  for (size_t i = 0; i < size; ++i) {
    if (mappings[i].code == code) {
      return mappings[i].key;
    }
  }
  return 0;  // zero or '\0' means no mapping found
}

//Very ugly solution.... If it works it works I heard someone saying
void mediaKeyHandler (uint8_t position, uint8_t key){
  Serial.println("MediaKeyEvent");
  if(position == 2){
    switch (key){
      case 0xFD: bleKeyboard.write(KEY_MEDIA_VOLUME_UP);     Serial.println("Released UP");          break; //UP
      case 0xF7: bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);   Serial.println("Released DOWN");        break; //DONW
      case 0xFE: bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);Serial.println("Released LEFT");        break; //Left
      case 0xFB: bleKeyboard.write(KEY_MEDIA_NEXT_TRACK);    Serial.println("Released RIGHT");       break; //RIGHT
      //case 0xEF: break; //A
      //case 0xBF: break; //Y
      //case 0xDF: break; //B
    }
    shouldVibrate = true;
  }
  if(position == 3){
    switch(key){
      case 0xEF: bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);     Serial.println("Released LEFT Power");    break; //LEFT POWER
      //case 0xFD: break; //LEFT UPPER
      //case 0xFB: break; //LEFT MIDDLE
      //case 0xF7: break; //LEFT LOWER
      //case 0xFE: break; //Z
      //case 0xDF: break; //RIGHT UPPER
      //case 0xBF: break; //RIGHT MIDDLE

    }
    shouldVibrate = true;

  }

  if(position == 4){
    switch(key){
      //case 0xFD: break; //RIGHT POWER
      //case 0xFE: break; //RICHT LOWER
    }
    shouldVibrate = true;
  }


}

void vibrate() {
  //Serial.println("Vibrate");
  if (pRxCharacteristic && pRxCharacteristic->canWriteNoResponse()) {
    // Base vibration pattern
    uint8_t basePattern[] = {0x12, 0x12, 0x08, 0x0A, 0x06, 0x08, 0x02, 0x10, 0x00, 0x18}; //Default: 0x12, 0x12, 0x08, 0x0A, 0x06, 0x08, 0x01, 0x10, 0x00, 0x18
    const size_t baseLen = sizeof(basePattern);

    // Extended pattern with 0x20 appended
    uint8_t fullPattern[baseLen + 1];
    memcpy(fullPattern, basePattern, baseLen);
    fullPattern[baseLen] = 0x20;

    // Send the pattern
    pRxCharacteristic->writeValue(fullPattern, sizeof(fullPattern), false); // false = no response
    //Serial.println("Vibration command sent with extension 0x20.");
  } else {
    Serial.println("RX characteristic not writable or not available.");
  }
}


static void notifyCallback(BLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {

//Periodic "keep alives are "0x15" and "0x19 0x10 0x62" -> let's ignore them
    //We get a "Yes I am ready" kind of message: 2A 08 03 12 10 12 0E 08 18 10 00 18 90 03 22 05 FF FF FF FF 1F that we want to ignore
    //and also the "nothing is pressed" state after a button press that we don't need to further evaluate:
    //23 08 FF FF FF FF 0F 1A 04 08 00 10 00 1A 04 08 01 10 00 1A 04 08 02 10 00 1A 04 08 03 10 00

    //Buttons (only when pressed one by one, pressing multiple buttons simultanously gives other results):
    //Left Controller
      //up: pData[2] == 0xFD
      //down: pData[2] == 0xF7
      //left: pData[2] == 0xFE
      //right: pData[2] == 0xFB
      //Power: pData[3] == 0xEF
      //side upper: pData[3] == 0xFD
      //side middle: pData[3] == 0xFB
      //side lower: pData[3] == 0xF7
      //Ignoring the pedal for now
    //Right Controller
      //Y: pData[2] == 0xBF
      //B: pData[2] == 0xDF
      //Z: pData[3] == 0xFE
      //A: pData[2] == 0xEF
      //Power: pData[4] == 0xFD
      //side upper: pData[3] == 0xDF
      //side middle: pData[3] == 0xBF
      //side lower: pData[4] == 0xFE
      //Ignoring the pedal for now
    //Unpressed is always 0xFF

    //Logic to send Keystrokes is on button release (this way we prevent multiple detections in a row).
    if (prevData2 != 0xFF && pData[2] == 0xFF) {
        char mappedKey = getMappedKey(pData2Mappings, sizeof(pData2Mappings)/sizeof(ButtonMapping), prevData2);
      if (mappedKey) {
        if(mappedKey == KEY_MEDIA){
          mediaKeyHandler(2,prevData2);
        }
        bleKeyboard.write(mappedKey);
        //Serial.print("Released pData[2] mapped key: ");
        Serial.println(mappedKey);
        shouldVibrate = true;
      }
    }

    // Handle pData[3]
    if (prevData3 != 0xFF && pData[3] == 0xFF) {
      char mappedKey = getMappedKey(pData3Mappings, sizeof(pData3Mappings)/sizeof(ButtonMapping), prevData3);
      if (mappedKey) {
        if(mappedKey == KEY_MEDIA){
          mediaKeyHandler(3,prevData3);
        }
        bleKeyboard.write(mappedKey);
        //Serial.print("Released pData[3] mapped key: ");
        Serial.println(mappedKey);
        shouldVibrate = true;
      }
    }

    // For pData[4]
    if (prevData4 != 0xFF && pData[4] == 0xFF) {
      char mappedKey = getMappedKey(pData4Mappings, sizeof(pData4Mappings)/sizeof(ButtonMapping), prevData4);
      if (mappedKey) {
        if(mappedKey == KEY_MEDIA){
          mediaKeyHandler(4,prevData4);
        }
        bleKeyboard.write(mappedKey);
        //Serial.print("Released pData[4] mapped key: ");
        Serial.println(mappedKey);
        shouldVibrate = true;
      }
    }

  // Save current states for next comparison
  prevData2 = pData[2];
  prevData3 = pData[3];
  prevData4 = pData[4];


/*
  // used for figuring out button press messages
  const uint8_t ignoredMessage[] = {
      0x23, 0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F,
      0x1A, 0x04, 0x08, 0x00, 0x10, 0x00,
      0x1A, 0x04, 0x08, 0x01, 0x10, 0x00,
      0x1A, 0x04, 0x08, 0x02, 0x10, 0x00,
      0x1A, 0x04, 0x08, 0x03, 0x10, 0x00
    };
  const size_t ignoredLength = sizeof(ignoredMessage);

    if (pData[0] == 0x23){
      if (memcmp(pData, ignoredMessage, ignoredLength) != 0)
          //Serial.print("length: ");
          //Serial.println(length);
          //Serial.print("Notification received: ");
          for (int i = 0; i < length; i++) {
            Serial.printf("%02X ", pData[i]);
          }
          Serial.println();
    }
*/

}



bool connectAndHandshakeZwiftRide() {
  Serial.println("starting the BLE Client...");
  pClient = BLEDevice::createClient();
  Serial.println("Created BLE client");

  if (!pClient->connect(myDevice)) {
    Serial.println("Failed to connect");
    return false;
  }

  Serial.println("Connected to Service");


//Connect to Controller
  BLERemoteService* pService = pClient->getService(BLEUUID(zwiftServiceUUID.c_str()));
  if (pService == nullptr) {
    Serial.println("Service not found using the currently configured Zwift service UUID.");
    Serial.println("Listing all GATT services actually present on this device (likely cause: a controller firmware update changed the UUID):");
    std::map<std::string, BLERemoteService*>* services = pClient->getServices();
    for (auto& kv : *services) {
      Serial.print("  Service UUID: ");
      Serial.println(kv.first.c_str());
    }
    Serial.println("-> Set the matching UUID from above with the \"!uuid=\" Serial command.");
    pClient->disconnect();
    return false;
  }


  //Send handshake message to RX Char
  Serial.println("Send Handshake...");
  pRxCharacteristic = pService->getCharacteristic(BLEUUID(ZWIFT_SYNC_RX_CHARACTERISTIC_UUID));
  if (pRxCharacteristic == nullptr) {
    Serial.println("RX characteristic not found");
    pClient->disconnect();
    return false;
  }
//
// if (!pRxCharacteristic->canWrite()) {
//      Serial.println("RX characteristic not writable");
//      pClient->disconnect();
//      return false;
//  }

  String handshakeMessage = "RideOn";
  pRxCharacteristic->writeValue(handshakeMessage.c_str(), handshakeMessage.length());

  //Check if handshake was accepted by reading the TX Char -> Only works if subscribed to the Tx Char... Let's just assume it always works

  //Subscribe to the Async Char
  pNotifyCharacteristic = pService->getCharacteristic(BLEUUID(ZWIFT_ASYNC_CHARACTERISTIC_UUID));
  if (pNotifyCharacteristic == nullptr) {
    Serial.println("Notify characteristic not found");
    pClient->disconnect();
    return false;
  }

  if (pNotifyCharacteristic->canNotify()) {
    pNotifyCharacteristic->registerForNotify(notifyCallback);
    Serial.println("Subscribed to notifications");
    subscribed = true;
  }

  connected = true;
  return true;
}

void startScan(uint32_t durationSeconds) {
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pScan->setInterval(1349);
  pScan->setWindow(449);
  pScan->setActiveScan(true);
  pScan->start(durationSeconds, false);
}

void printCommandHelp() {
  Serial.println("Serial commands:");
  Serial.println("  ?scan          - scan for 10s and list all nearby BLE devices (name/address/UUIDs)");
  Serial.println("  !uuid=<uuid>   - set & persist a new Zwift service UUID and reconnect with it, e.g.:");
  Serial.println("                   !uuid=0000FC82-0000-1000-8000-00805F9B34FB");
}

void handleSerialCommands() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  if (line.equalsIgnoreCase("?scan")) {
    Serial.println("Scanning for 10s...");
    startScan(10);
    return;
  }

  if (line.startsWith("!uuid=")) {
    String newUUID = line.substring(6);
    newUUID.trim();
    if (newUUID.length() < 4) {
      Serial.println("Invalid UUID, ignoring.");
      return;
    }
    zwiftServiceUUID = newUUID;
    preferences.putString("serviceUUID", zwiftServiceUUID);
    Serial.print("Saved new Zwift service UUID: ");
    Serial.println(zwiftServiceUUID);
    if (connected && pClient != nullptr) {
      pClient->disconnect();
    }
    connected = false;
    subscribed = false;
    doConnect = false;
    Serial.println("Restarting scan with new UUID...");
    startScan(30);
    return;
  }

  Serial.print("Unknown command: ");
  Serial.println(line);
  printCommandHelp();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE Client + HID Keyboard");

  preferences.begin("zword", false);
  zwiftServiceUUID = preferences.getString("serviceUUID", DEFAULT_ZWIFT_CUSTOM_SERVICE_UUID);
  Serial.print("Using Zwift service UUID: ");
  Serial.println(zwiftServiceUUID);

  printCommandHelp();

  BLEDevice::init("Zword");
  bleKeyboard.begin();

  startScan(30);
}

void loop() {

  handleSerialCommands();

  if (shouldVibrate) {
    if(enableHapticFeedback){
      vibrate();
      shouldVibrate = false;
    }
  }

  if (doConnect) {
    if (connectAndHandshakeZwiftRide()) {
      Serial.println("Connected to Controller device");
    delay(500);
    vibrate();
    delay(2000);
      if (bleKeyboard.isConnected()) {
        if (subscribed) {
          //bleKeyboard.print("1");
          //Serial.println("Sent '1' via keyboard");
          vibrate();
          delay(500);
          vibrate();
        } else {
          //bleKeyboard.print("0");
          //Serial.println("Sent '0' via keyboard");
        }
        sentMessage = true;
      }
      doConnect = false;
    } else {
      Serial.println("Connection failed, restarting scan...");
      startScan(15);
    }


  }

  delay(100);
}