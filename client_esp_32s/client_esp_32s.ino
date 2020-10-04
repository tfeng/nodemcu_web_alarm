#include <Adafruit_SSD1306.h>
#include <WebSocketsClient.h>
#include <WiFi.h>

const char*  SSID          = "ssid";
const char*  PASSWORD      = "password";

const String SERVER_HOST   = "cpe.tfeng.me";
const int    SERVER_PORT   = 37813;

const String CLIENT_NAME   = "Jason";

const int    BUTTON_PIN    = 4;
const int    BUZZER_PIN    = 19;
const int    LED_PIN       = 23;
const int    STATUS_PIN    = 2;

const int    SCREEN_WIDTH  = 128;
const int    SCREEN_HEIGHT = 64;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT);
WebSocketsClient webSocket;
bool has_alarm = false;

void show(String s) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(s);
  display.display();
}

void alarm(String message) {
  show(message);
  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(STATUS_PIN, HIGH);
}

void normal() {
  show("Normal");
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(STATUS_PIN, HIGH);
}

void disconnected() {
  show("Disconnected");
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(STATUS_PIN, LOW);
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      disconnected();
      if (WiFi.status() != WL_CONNECTED) {
        init_wifi();
      }
      break;
    case WStype_CONNECTED:
      normal();
      has_alarm = false;
      break;
    case WStype_TEXT:
      if (length == 0) {
        normal();
      } else {
        alarm((char*) payload);
      }
      break;
  }
}

void init_wifi() {
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(SSID, PASSWORD);
    WiFi.waitForConnectResult();
  }
}

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextSize(1);
  display.setTextColor(WHITE);

  pinMode(BUTTON_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(STATUS_PIN, OUTPUT);

  disconnected();

  init_wifi();

  webSocket.begin(SERVER_HOST, SERVER_PORT);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(3000, 3000, 2);
}

void loop() {
  if (!has_alarm && digitalRead(BUTTON_PIN) == HIGH) {
    has_alarm = true;
    webSocket.sendTXT("start:" + CLIENT_NAME);
  } else if (has_alarm && digitalRead(BUTTON_PIN) == LOW) {
    has_alarm = false;
    webSocket.sendTXT("stop:" + CLIENT_NAME);
  }
  webSocket.loop();
}
