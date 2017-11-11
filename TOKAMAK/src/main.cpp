#include <TimeLib.h>
#include "SSD1306.h"
#include <WiFi.h>
#include "secret.h"

const int screenW = 128;
const int screenH = 64;
const int centerX = screenW / 2;
const int centerY = screenH / 2;

SSD1306 display(0x3c, 5, 4); // SDA, SCL
WiFiServer server(80);

void relays (int code) {
  // 16?
  const int p = 25;
  const int n = 26;

  pinMode(p, OUTPUT);
  pinMode(n, OUTPUT);

  if (code & 1) {
    digitalWrite(p, 1);
    digitalWrite(n, 0);
  } else {
    digitalWrite(p, 0);
    digitalWrite(n, 1);
  }
  delay(5);

  pinMode(p, INPUT);
  pinMode(n, INPUT);

  delay(5);
}

void blinkAll (int code) {
  const int myPins[9] = {
    // 39, z
    16,
    // 5, H
    // 4, H
    0,
    2,
    14,
    12,
    13,
    15,
    // 3,
    // 1
    25,
    26
  };
  for (int j = 0; j < 9; j++) {
    pinMode(myPins[j], OUTPUT);
  }
  for (int i = 0; i < 0xffff; i++) {
    for (int j = 0; j < 9; j++) {
      digitalWrite(myPins[j], ((i >> j) & 1));
    }
    delay(5);
  }
  for (int j = 0; j < 9; j++) {
    pinMode(myPins[j], INPUT);
  }
}

void setup() {

  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_24);

  display.clear();
  display.drawString(centerX, centerY, "?");
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  const int ip = WiFi.localIP();
  const String ipString = String((ip >> 16) & 255) + ":" + String((ip >> 24) & 255);
  display.clear();
  display.drawString(centerX, centerY, ipString);
  display.display();

  server.begin();
}

void loop() {

  WiFiClient client = server.available();

  if (client) {
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.print("<title>TOKAMAK</title>");
            client.print("TOKAMAK online<br>");
            client.println();
            break;
          } else {

            if (currentLine.startsWith("GET /?=")) {
              const String msg = currentLine.substring(7,15);
              const int inMsg = msg.toInt();
              relays(inMsg);

              const String msg2 = String(inMsg);
              display.clear();
              display.drawString(centerX, centerY, msg2);
              display.display();
            }

            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
  }
}
