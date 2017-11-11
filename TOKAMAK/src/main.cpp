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
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }

        if (currentLine.startsWith("GET /?=")) {
          const String msg = currentLine.substring(7,15);
          display.clear();
          display.drawString(centerX, centerY, msg);
          display.display();
        }
      }
    }
    client.stop();
  }
}
