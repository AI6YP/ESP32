#include <GxEPD2_3C.h>
#include "malevich1.h" // 2.9"  b/w/r
#include "vascale.h" // 2.9"  b/w/r

//pins

#define MENU_BTN_PIN 26   // pin15 / BTN_1 / SW
#define BACK_BTN_PIN 25   // pin14 / BTN_2 / NW
// #define UP_BTN_PIN 35  // pin11 / BTN_3 / NE
#define DOWN_BTN_PIN 4    // pin24 / BTN_4 / SE
#define DISPLAY_CS 5      // pin34
#define DISPLAY_RES 9     // pin28
#define DISPLAY_DC 10     // pin29
#define DISPLAY_BUSY 19   // pin38
#define ACC_INT_1_PIN 14  // pin17
#define ACC_INT_2_PIN 12  // pin18
#define VIB_MOTOR_PIN 13  // pin20
#define RTC_INT_PIN 27    // pin16

GxEPD2_3C<GxEPD2_290_Z13c, GxEPD2_290_Z13c::HEIGHT> display(
  GxEPD2_290_Z13c(DISPLAY_CS, DISPLAY_DC, DISPLAY_RES, DISPLAY_BUSY));

struct bitmap_pair {
  const unsigned char* black;
  const unsigned char* red;
};

bitmap_pair bitmap_pairs[] = {
  {vascale_black, vascale_red}
};

void setup() {
  // init Watchy board
  Wire.begin(SDA, SCL); // init i2c

  display.init(115200); // default 10ms reset pulse, e.g. for bare panels with DESPI-C02
  for (uint16_t i = 0; i < sizeof(bitmap_pairs) / sizeof(bitmap_pair); i++) {
    if (i) {
      delay(2000);
    }
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      display.drawBitmap(0, 0, bitmap_pairs[i].black, 128, 296, GxEPD_BLACK);
      display.drawBitmap(0, 0, bitmap_pairs[i].red,   128, 296, GxEPD_RED);
    }
    while (display.nextPage());
  }
}

void loop(){}
