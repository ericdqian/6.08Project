#include <SPI.h>
#include <WiFi.h> //Connect to WiFi Network
#include <TFT_eSPI.h>
#include <WiFiClientSecure.h>

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <string.h>

#include "button.cpp"

#define IDLE 0
#define REGISTER 1
#define RECORD_NAME 2
#define NAME_VERIFY 3
#define VIEW_REGISTERED 4
#define SELECTION_MENU 7
#define RECORD_DESCRIPTION 5
#define DESCRIPTION_VERIFY 6
#define TRACK 8
#define RECOMMENDATIONS 9

#define SHORTPRESS 1
#define LONGPRESS 2

#define NONE 0
#define SUCCESS 1
#define FAIL 2

TFT_eSPI tft = TFT_eSPI();


char network[] = "MIT";

char password[] = "";

const char USER[] = "jenning";

//Some constants and some resources:
const uint16_t RESPONSE_TIMEOUT = 6000;
const uint16_t IN_BUFFER_SIZE = 1000; //size of buffer to hold HTTP request
const uint16_t OUT_BUFFER_SIZE = 1000; //size of buffer to hold HTTP response
char request_buffer[IN_BUFFER_SIZE]; //char array buffer to hold HTTP request
char response[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP request

const uint8_t PIN_1 = 16; //button 1
const uint8_t PIN_2 = 5; //button 2

uint32_t primary_timer;
uint32_t timer;
uint32_t timeout_timer;

const uint8_t TIMEOUT_PERIOD = 2500; //milliseconds

uint8_t toggle;
uint8_t toggle_state;
uint8_t toggle_selection;
uint8_t num_items;
uint8_t state;
uint8_t screen_color;

/* MIC VARIABLES*/

const int DELAY = 1000;
const int SAMPLE_FREQ = 8000;                          // Hz, telephone sample rate
const int SAMPLE_DURATION = 3;                        // duration of fixed sampling (seconds)
const int NUM_SAMPLES = SAMPLE_FREQ * SAMPLE_DURATION;  // number of of samples
const int ENC_LEN = (NUM_SAMPLES + 2 - ((NUM_SAMPLES + 2) % 3)) / 3 * 4;  // Encoded length of clip


/* CONSTANTS */
//Prefix to POST request:
const char PREFIX[] = "{\"config\":{\"encoding\":\"MULAW\",\"sampleRateHertz\":8000,\"languageCode\": \"en-US\",\"profanityFilter\":true,\"speechContexts\":[{\"phrases\": [\"draw\", \"rectangle\",\"circle\",  \"draw rectangle\",\"draw circle\"]}]}, \"audio\": {\"content\":\"";
const char SUFFIX[] = "\"}}"; //suffix to POST request
const int AUDIO_IN = A0; //pin where microphone is connected
const char API_KEY[] = "AIzaSyD6ETx_Ammh1jgbfxG_wggm8eGa08yzQzQ"; //don't change this


/* Global variables*/
uint8_t button_state; //used for containing button state and detecting edges
int old_button_state; //used for detecting button edges
uint8_t button_state2; //used for containing button state and detecting edges
int old_button_state2; //used for detecting button edges
uint32_t time_since_sample;      // used for microsecond timing

char temp_transcript[1000];
char name_transcript[100];
char description_transcript[1000];

char speech_data[ENC_LEN + 200] = {0}; //global used for collecting speech data
const char*  SERVER = "speech.google.com";  // Server URL

uint8_t old_val;

WiFiClientSecure client; //global WiFiClient Secure object

boolean in_welcome;
char select_char[] = "-"; // selection indicator variable
char address[400]; // stores uuid
char prevPairedId[5][15]; //tracks what things have been paired
char prevPairedName[5][15]; //tracks what things have been paired
char prevPairedSyncName[5][15];

static BLEUUID SERVICE_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID TRACK_CHARACTERISTIC_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static BLEUUID PAIR_CHARACTERISTIC_UUID("ceb5483e-36e1-4688-b7f5-ea07361b26a3");

int refreshOrSelectPin = 16;
int togglePin = 5;
int lastButtonPress;
int scrollPosition;
int arrayPtr = 0;
int prevPairedPtr = 0;
char manufactureDesc[15];
bool paired = false;
bool connected = false;
bool tracking = false;
bool beep = false;
char name[30];
int buzzerPin = 22;
bool connectSuccessful = false;
int selected[10];
int selectPtr = 0;

Button refreshOrSelectButton(refreshOrSelectPin);
Button toggleButton(togglePin);
BLEAdvertisedDevice* devices[5]; // can have a list of 4 devices that are advertised at any given time


BLEScan* pBLEScan;;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      Serial.println(advertisedDevice.toString().c_str());
      char deviceAddress[100];
      strcpy(deviceAddress, advertisedDevice.getAddress().toString().c_str());
      char test[100];
      strcpy(test, "4fafc201-1fb5-459e-8fcc-c5c9c331914b");
      if (!tracking) {
        if (advertisedDevice.haveServiceUUID() && strcmp(advertisedDevice.getServiceUUID().toString().c_str(), test) == 0) {
          Serial.println("GREAAT SUCESS PAIRING");
          if (arrayPtr < 4) {
            devices[arrayPtr] = new BLEAdvertisedDevice(advertisedDevice);
            arrayPtr++;
          }
        }
      } else  {
        if (advertisedDevice.haveServiceUUID() && strcmp(advertisedDevice.getServiceUUID().toString().c_str(), test) == 0) {
          for (int i = 0; i < prevPairedPtr; i++) {
            if (strcmp(deviceAddress, prevPairedId[i]) == 0) {
              if (arrayPtr < 4) {

                devices[arrayPtr] = new BLEAdvertisedDevice(advertisedDevice);
                strcpy(prevPairedSyncName[arrayPtr], prevPairedName[i]);
                Serial.println(prevPairedSyncName[arrayPtr]);
                arrayPtr++;
              }
            }
          }
          Serial.println("GREAAT SUCESS TRACKING");
          if (arrayPtr < 4) {
            devices[arrayPtr] = new BLEAdvertisedDevice(advertisedDevice);
            arrayPtr++;
          }
        }
      }

    }
};

void rerender() {
  tft.fillScreen(TFT_BLACK); //fill background
  for (int i = 0; i < arrayPtr; i++) {
    char deviceName[20];
    if (tracking) {
      strcpy(deviceName, prevPairedName[i]);
      //      strcpy(deviceName, devices[i]->getName().c_str());
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      for (int j = 0; j < 10; j++) {
        if (selected[j] == i) {
          tft.setTextColor(TFT_GREEN, TFT_BLACK);
        }
      }
    } else {
      //      strcpy(deviceName, prevPairedName[i]);
      strcpy(deviceName, devices[i]->getAddress().toString().c_str());
    }

    tft.drawString(deviceName, 10, 10 * ( i + 1), 1); //change back to just i
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    if (i == scrollPosition) {
      tft.drawString(">", 0, 10 * (i + 1), 1); //change back to just i
    }
  }
  tft.drawString("Long press left to", 0, 100, 1);
  tft.drawString("rescan for tags", 0, 110, 1);
  tft.drawString("Long press right to", 0, 130, 1);
  tft.drawString("return to main menu", 0, 140, 1);
}

class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
      connected = true;
      Serial.println("onConnect");
    }

    void onDisconnect(BLEClient* pclient) {
      connected = false;
      Serial.println("onDisconnect");
      Serial.println(tracking);
      Serial.print("connectSuccessful");

      if (tracking) {
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Connection lost!", 0, 50, 1);
        tft.drawString("Press right button to exit.", 0, 60, 1);
        beep = true;
      }
    }
};

void setup() {
  Serial.begin(115200);               // Set up serial port
  pinMode(PIN_1, INPUT_PULLUP);
  pinMode(PIN_2, INPUT_PULLUP);
  tft.init();  //init screen
  tft.setRotation(2); //adjust rotation
  tft.setTextSize(1); //default font size
  tft.fillScreen(TFT_BLACK); //fill background
  tft.setTextColor(TFT_WHITE, TFT_BLACK); //set col
  WiFi.begin(network, password); //attempt to connect to wifi
  uint8_t count = 0; //count used for Wifi check times
  Serial.print("Attempting to connect to ");
  Serial.println(network);
  while (WiFi.status() != WL_CONNECTED && count < 12) {
    delay(500);
    Serial.print(".");
    count++;
  }
  delay(2000);
  if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
    Serial.println("CONNECTED!");
    Serial.println(WiFi.localIP().toString() + " (" + WiFi.macAddress() + ") (" + WiFi.SSID() + ")");
    delay(500);

  } else { //if we failed to connect just Try again.
    Serial.println("Failed to Connect :/  Going to restart");
    Serial.println(WiFi.status());
    ESP.restart(); // restart the ESP (proper way)
  }

  analogSetAttenuation(ADC_6db); //set to 6dB attenuation for 3.3V full scale reading.

  state = IDLE;
  primary_timer = millis();
  timeout_timer = millis();
  timer = millis();
  in_welcome = false;
  toggle_state = 0;

  // BLE
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value
  lastButtonPress = millis();
  scrollPosition = 0;
  //  pinMode(togglePin, INPUT_PULLUP);
  //  pinMode(refreshOrSelectPin, INPUT_PULLUP);

  pinMode(buzzerPin, OUTPUT);
  ledcSetup(0, 1E5, 12);
  ledcAttachPin(buzzerPin, 0);
  strcpy(manufactureDesc, "MYESP32");

}

void welcome() {
  in_welcome = true;
  tft.fillScreen(TFT_BLACK); //fill background
  tft.drawString("Welcome!", 0, 1, 2);
  tft.drawString("Register new item", 10, 30, 1);
  tft.drawString("Track items", 10, 40, 1);
  tft.drawString("View registered", 10, 50, 1);
  tft.drawString("items", 10, 60, 1);
  tft.drawString(select_char, 2, (toggle_state + 3) * 10, 1);
  tft.drawString("Long press left to", 0, 120, 1);
  tft.drawString("select action!", 0, 130, 1);

  fetch_weather_data();
  // fetch_calendar_data();
}

void register_prompt() {
  tft.fillScreen(TFT_BLACK); //fill background
  //  tft.drawString("Registering: place", 0, 50, 1);
  //  tft.drawString("your item next to me!", 0, 60, 1);
  tft.drawString("REGISTER", 0, 30, 2);
  tft.drawString("Long press left to", 0, 50, 1);
  tft.drawString("scan for tags", 0, 60, 1);
}

void view_registered() {
  tft.fillScreen(TFT_BLACK); //fill background
  char item_response_buffer[OUT_BUFFER_SIZE];
  char item_request_buffer[IN_BUFFER_SIZE];
  sprintf(item_request_buffer, "GET http://608dev.net/sandbox/sc/lyy/new_test.py HTTP/1.1\r\n");
  strcat(item_request_buffer, "Host: 608dev.net\r\n");
  strcat(item_request_buffer, "\r\n"); //new line from header to body
  do_http_request("608dev.net", item_request_buffer, item_response_buffer, 50, RESPONSE_TIMEOUT, true);
  tft.setCursor(0, 10, 1); // set the cursor
  tft.drawString("Registered items:", 0, 0, 2);
  tft.println(item_response_buffer);
  tft.drawString("Short press any", 0, 120, 1);
  tft.drawString("button to go back", 0, 130, 1);
}

void view_recommendations() {
  tft.fillScreen(TFT_BLACK); //fill background
  char item_response_buffer[OUT_BUFFER_SIZE];
  char item_request_buffer[IN_BUFFER_SIZE];
  sprintf(item_request_buffer, "GET http://608dev.net/sandbox/sc/lyy/new_test.py HTTP/1.1\r\n");
  strcat(item_request_buffer, "Host: 608dev.net\r\n");
  strcat(item_request_buffer, "\r\n"); //new line from header to body
  do_http_request("608dev.net", item_request_buffer, item_response_buffer, 50, RESPONSE_TIMEOUT, true);
  tft.setCursor(0, 10, 1); // set the cursor
  tft.println("Recommended items:");
  tft.println(item_response_buffer);
  tft.drawString("Short press any", 0, 120, 1);
  tft.drawString("button to move on", 0, 130, 1);
}

void show_selection_menu() {
  toggle_selection = 0;
  tft.fillScreen(TFT_BLACK); //fill background
  char item_response_buffer[OUT_BUFFER_SIZE];
  char item_request_buffer[IN_BUFFER_SIZE];
  sprintf(item_request_buffer, "GET http://608dev.net/sandbox/sc/lyy/new_test.py HTTP/1.1\r\n");
  strcat(item_request_buffer, "Host: 608dev.net\r\n");
  strcat(item_request_buffer, "\r\n"); //new line from header to body
  do_http_request("608dev.net", item_request_buffer, item_response_buffer, 50, RESPONSE_TIMEOUT, true);
  tft.setCursor(2, 10, 1); // set the cursor
  tft.println("Select item to track:");
  tft.println(item_response_buffer);
}

void load_paired_items() {
  char item_response_buffer[OUT_BUFFER_SIZE];
  char item_request_buffer[IN_BUFFER_SIZE];
  sprintf(item_request_buffer, "GET http://608dev.net/sandbox/sc/lyy/new_test.py?return_name_id=1 HTTP/1.1\r\n");
  strcat(item_request_buffer, "Host: 608dev.net\r\n");
  strcat(item_request_buffer, "\r\n"); //new line from header to body
  do_http_request("608dev.net", item_request_buffer, item_response_buffer, 50, RESPONSE_TIMEOUT, true);

  char * ptr;
  ptr = strtok(item_response_buffer, "\n");
  Serial.println(ptr);
  int index = 0;
  Serial.println("populating prevPaired");
  while (ptr !=  NULL)
  {

    Serial.println("i'm in the loop");
    strcpy(prevPairedName[index], ptr);
    ptr = strtok(NULL, "\n");
    Serial.println(ptr);
    strcpy(prevPairedId[index], ptr);
    ptr = strtok(NULL, "\n");
    Serial.println(ptr);
    if (ptr == NULL) {
      Serial.println("breaking");
      break;
    }
    Serial.println(prevPairedName[index]);
    Serial.println(prevPairedName[index]);
    index++;
  }

  Serial.println("-----");
  for (int i = 0; i < 5; i++) {
    Serial.println(prevPairedName[i]);
    Serial.println(prevPairedId[i]);
  }
}

void loop() {
  button_state = digitalRead(PIN_1);
  button_state2 = digitalRead(PIN_2);

  switch (state) {
    case IDLE: {
        if (!in_welcome) {
          welcome(); // welcome the user
        }
        toggle = refreshOrSelectButton.update1();
        if (toggle == SHORTPRESS) {
          tft.drawString(" ", 2, 10 * (toggle_state + 3), 1);
          toggle_state += 1;
          toggle_state %= 3;
          tft.drawString(select_char, 2, 10 * (toggle_state + 3), 1);
        } else if (toggle == LONGPRESS) {
          in_welcome = false;
          if (toggle_state == 0) {
            register_prompt();
            state = REGISTER;
          } else if (toggle_state == 1) {
            view_recommendations();
            state = RECOMMENDATIONS;
          } else if (toggle_state == 2) {
            view_registered();
            state = VIEW_REGISTERED;
          }
        }
      }
      break;
    case VIEW_REGISTERED:
      {
        toggle = refreshOrSelectButton.update1();
        int right = toggleButton.update1();
        if (toggle == SHORTPRESS || right == SHORTPRESS) {
          state = IDLE;
        }
      }
      break;
    case REGISTER: {
        int refreshOrSelectRes = refreshOrSelectButton.update1();
        int toggleRes = toggleButton.update1();

        //  Serial.print(refreshOrSelectRes);
        //  Serial.println(toggleRes);

        if (refreshOrSelectRes == 2) {//refresh
          Serial.println("REFERESHING");
          tft.fillScreen(TFT_BLACK);
          tft.drawCentreString("Loading items", 3, 50, 2);
          arrayPtr = 0;
          BLEScanResults foundDevices = pBLEScan->start(2, false);
          delay(5000);//wait for scan to terminate
          pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
          rerender();

        } else if (refreshOrSelectRes == 1) {//select the current
          Serial.println("in the connecting block");
          BLEAdvertisedDevice* myDevice = devices[scrollPosition];

          Serial.println(myDevice->getServiceUUID().toString().c_str());

          BLEClient*  pClient  = BLEDevice::createClient();

          pClient->setClientCallbacks(new MyClientCallback());

          Serial.println("ready to connect");
          pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
          Serial.println(" - Connected to server");
          strcpy(address, myDevice->getAddress().toString().c_str());
          paired = true;

          // Obtain a reference to the service we are after in the remote BLE server.
          BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
          if (pRemoteService == nullptr) {
            Serial.print("Failed to find our service UUID: ");
            pClient->disconnect();
          }
          Serial.println(" - Found our service");


          // Obtain a reference to the characteristic in the service of the remote BLE server.
          BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(PAIR_CHARACTERISTIC_UUID);
          if (pRemoteCharacteristic == nullptr) {
            Serial.print("Failed to find our characteristic UUID: ");
            pClient->disconnect();
          }
          Serial.println(" - Found our characteristic");

          // Read the value of the characteristic.
          if (pRemoteCharacteristic->canWrite()) {
            pRemoteCharacteristic->writeValue("true", false);
            Serial.print("Set changed");
          }

          //          int yes = refreshOrSelectButton.update1();
          //          int no = toggleButton.update1();
          //
          //          while (yes == 0 && no == 0) {
          //            yes = refreshOrSelectButton.update1();
          //            no = toggleButton.update1();
          //          }
          //          Serial.println("out of the loop!");
          //          Serial.println(yes);
          //          if (yes != 0) {
          pRemoteCharacteristic->writeValue("false", false);
          strcpy(address, myDevice->getAddress().toString().c_str());
          pClient -> disconnect();
          tft.fillScreen(TFT_BLACK);
          tft.drawString("Success!", 0, 50, 1);
          while (millis() - timeout_timer < TIMEOUT_PERIOD);
          tft.fillScreen(TFT_BLACK);
          tft.drawString("Hold left button to", 0, 50, 1);
          tft.drawString("record item's name", 0, 60, 1);
          state = RECORD_NAME;
          //          } else {
          //            pRemoteCharacteristic->writeValue("false", false);
          //            pClient -> disconnect();
          //          }
          //          rerender();

        } else if (toggleRes == 1 ) {
          scrollPosition = (scrollPosition + 1) % (arrayPtr);
          rerender();
        } else if (toggleRes == 2) {
          state = IDLE;
        }
      }
      break;
    case RECORD_NAME: {
        int refreshOrSelectRes = refreshOrSelectButton.update1();
        //        if (resfreshOrSelectRes != 0) {
        handle_record();
        state = NAME_VERIFY;
        Serial.println("transitioning to NAME_VERIFY");
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Is", 0, 10, 1);
        tft.drawString(temp_transcript, 0, 20, 2);
        tft.drawString("correct?", 0, 30, 1);
        tft.drawString("ACCEPT: short press", 0, 120, 1);
        tft.drawString("right button", 0, 130, 1);
        tft.drawString("RECORD AGAIN: short", 0, 140, 1);
        tft.drawString("press left", 0, 150, 1);
        //        }
      }

      break;

    case NAME_VERIFY:
      {
        int refreshOrSelectRes = refreshOrSelectButton.update1();
        int toggleRes = toggleButton.update1();

        // reject the name
        if (refreshOrSelectRes == SHORTPRESS) {
          memset(name_transcript, 0, strlen(name_transcript));
          tft.fillScreen(TFT_BLACK);
          tft.drawString("Hold left button to", 0, 10, 1);
          tft.drawString("record item's name", 0, 20, 1);
          state = RECORD_NAME;
        }
        // accept the name
        if (toggleRes == SHORTPRESS) {
          tft.fillScreen(TFT_BLACK);
          tft.drawString("Hold left button to", 0, 50, 1);
          tft.drawString("record item's name", 0, 60, 1);
          Serial.println("transitioning to RECORD_DESCRIPTION");
          memset(name_transcript, 0, strlen(name_transcript));
          strcpy(name_transcript, temp_transcript);
          memset(temp_transcript, 0, strlen(temp_transcript));
          state = RECORD_DESCRIPTION;
        }
      }
      break;

    case RECORD_DESCRIPTION:
      {
        int refreshOrSelectRes = refreshOrSelectButton.update1();
        int toggleRes = toggleButton.update1();

        //        if (!button_state && button_state != old_button_state) {
        handle_record();
        state = DESCRIPTION_VERIFY;
        Serial.println("transitioning to DESCRIPTION_VERIFY");
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Is", 0, 10, 1);
        tft.drawString(temp_transcript, 0, 20, 2);
        tft.drawString("correct?", 0, 30, 1);
        tft.drawString("ACCEPT: short press", 0, 120, 1);
        tft.drawString("right button", 0, 130, 1);
        tft.drawString("RECORD AGAIN: short", 0, 140, 1);
        tft.drawString("press left", 0, 150, 1);
        //        }

      }
      break;

    case DESCRIPTION_VERIFY:
      {
        int refreshOrSelectRes = refreshOrSelectButton.update1();
        int toggleRes = toggleButton.update1();

        // reject the description
        if (refreshOrSelectRes == SHORTPRESS) {
          memset(temp_transcript, 0, strlen(temp_transcript));
          tft.fillScreen(TFT_BLACK);
          tft.drawString("Hold left button to", 0, 50, 1);
          tft.drawString("record item's description", 0, 60, 1);
          Serial.println("transitioning to RECORD_DESCRIPTION");
          state = RECORD_DESCRIPTION;
        }

        // accept the description
        if (toggleRes == SHORTPRESS) {
          strcpy(description_transcript, temp_transcript);
          // post request to server
          char body[200]; //for body;
          sprintf(body, "id=%s&name=%s&description=%s", address , name_transcript, description_transcript); //generate body, posting to User, 1 step
          int body_len = strlen(body); //calculate body length (for header reporting)
          sprintf(request_buffer, "POST http://608dev.net/sandbox/sc/lyy/new_test.py HTTP/1.1\r\n");
          strcat(request_buffer, "Host: 608dev.net\r\n");
          strcat(request_buffer, "Content-Type: application/x-www-form-urlencoded\r\n");
          sprintf(request_buffer + strlen(request_buffer), "Content-Length: %d\r\n", body_len); //append string formatted to end of request buffer
          strcat(request_buffer, "\r\n"); //new line from header to body
          strcat(request_buffer, body); //body
          strcat(request_buffer, "\r\n"); //header
          Serial.println(request_buffer);
          do_http_request("608dev.net", request_buffer, response, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, true);
          //tft.println(response); //print the result
          tft.fillScreen(TFT_BLACK);
          tft.drawString(name_transcript, 0, 10, 2);
          tft.drawString(" has been registered", 0, 20, 1);
          memset(name_transcript, 0, strlen(name_transcript));
          memset(description_transcript, 0, strlen(description_transcript));
          Serial.println("SUCCESSFUL REGISTRATION");
          state = IDLE;
        }
      }
      break;
    case RECOMMENDATIONS:
      {
        toggle = refreshOrSelectButton.update1();
        int right = toggleButton.update1();
        Serial.println(toggle);
        if (toggle == SHORTPRESS || right == SHORTPRESS) {
          tft.fillScreen(TFT_BLACK);
          tft.drawString("Hold left button to", 0, 50, 1);
          tft.drawString("load items", 0, 60, 1);
          state = TRACK;
        }
      }
      break;
    case TRACK:
      {
        tracking = true;
        int refreshOrSelectRes = refreshOrSelectButton.update1();
        int toggleRes = toggleButton.update1();
        if (beep && toggleRes == 2) {
          tracking = false;
          beep = false;
          ledcWrite(0, 0);
          state = IDLE;
          return;
        }

        //        for (int i = 0; i < 5; i++) {
        //          memset(prevPairedId[i], 0, strlen(prevPairedId[i]));
        //          memset(prevPairedName[i], 0, strlen(prevPairedName[i]));
        //        }
        //  Serial.print(refreshOrSelectRes);
        //  Serial.println(toggleRes);

        if (refreshOrSelectRes == 2) {//refresh
          tft.fillScreen(TFT_BLACK);
          tft.drawCentreString("Loading items", 3, 50, 2);
          load_paired_items();
          Serial.println("REFRESHING");
          arrayPtr = 0;
          BLEScanResults foundDevices = pBLEScan->start(2, false);
          delay(5000);//wait for scan to terminate
          pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory

          rerender();
//          tft.drawString("TRACK", 0, 10, 1);

        } else if (refreshOrSelectRes == 1) {//select the current
          Serial.println("in the connecting block");
          BLEAdvertisedDevice* myDevice = devices[scrollPosition];

          Serial.println(myDevice->getServiceUUID().toString().c_str());

          BLEClient*  pClient  = BLEDevice::createClient();

          pClient->setClientCallbacks(new MyClientCallback());

          Serial.println("ready to connect");
          connectSuccessful = pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
          Serial.println(" - Connected to server");
          strcpy(address, myDevice->getAddress().toString().c_str());
          paired = true;

          // Obtain a reference to the service we are after in the remote BLE server.
          BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
          if (pRemoteService == nullptr) {
            Serial.print("Failed to find our service UUID: ");
            pClient->disconnect();
          }
          Serial.println(" - Found our service");


          // Obtain a reference to the characteristic in the service of the remote BLE server.
          BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(TRACK_CHARACTERISTIC_UUID);
          if (pRemoteCharacteristic == nullptr) {
            Serial.print("Failed to find our characteristic UUID: ");
            pClient->disconnect();
          }
          Serial.println(" - Found our characteristic");

          // Read the value of the characteristic.
          if (pRemoteCharacteristic->canWrite()) {
            pRemoteCharacteristic->writeValue("true", false);
            Serial.print("Set changed");
          }

        } else if (toggleRes == 1 ) {
          scrollPosition = (scrollPosition + 1) % (arrayPtr);
          selected[selectPtr] = scrollPosition;
          rerender();
        }
        else if (toggleRes == 2) {
          state = IDLE;
        }

        if (beep) {
          Serial.println("BEEEEEP");
          ledcWriteTone(0, 1200);
          ledcWriteNote(0, NOTE_C, 1);
          delay(500);

        }
      }
      break;
  }

  while (millis() - timer < 10);
  timer = millis();
  old_button_state = button_state;
  old_button_state2 = button_state2;
}
