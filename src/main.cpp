#include <DCF77FreeRTOS.h>
#include <TFT_eSPI.h>


// Create DCF77 receiver instance
// Parameters: input pin, debug flag (DCF_DEBUG / DCF_NODEBUG)
DCF77FreeRTOS receiver(DCF_DATA, DCF_NODEBUG);

// Create Adafruit OLED driver instance
TFT_eSPI display = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT);


// Global variables:
int uptimeSeconds = 0;
int syncedShown = 0;

// day of week string array:
constexpr const char* kDayNames[] = {
  "", "Monday", "Tuesday", "Wednesday",
  "Thursday", "Friday", "Saturday", "Sunday"
};


void setup() {
  Serial.begin(115200);
  delay(100);  // give serial time to initialize

  Serial.println("[FW] DCF77-Clock V0.1 --- https://github.com/friedkiwi/dcf77-clock");
  
  // enable display backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // init display
  display.init();
  display.setRotation(1);
  display.fillScreen(TFT_BLACK);

  

  Serial.println("[DISP] Display initialised.");

  receiver.begin(-1);  // optional LED pin
  Serial.println("[DCF77] DCF77 receiver initialised.");
}


void loop() {
  DCFtime t;
  
  if (receiver.getTime(&t)) {
    Serial.printf(
      "[DCF77] DCF time: %02u-%02u-%02u %02u:%02u (dow=%u)\n",
      t.year, t.month, t.day, t.hour, t.minute, t.dow
    );

    // draw clock
    display.setTextColor(TFT_WHITE);
    display.setTextSize(4);
    display.setCursor(40,40);
    display.printf("%02u:%02u", t.hour, t.minute);

    // draw date
    display.setTextSize(2);
    display.setCursor(125,0);
    display.printf("%s 20%02u-%02u-%02u", kDayNames[t.dow], t.year, t.month, t.day);
  }

  int status = receiver.getStatus();

  // draw DCF status:
  display.setTextSize(1);
  display.fillRect(0,0,150,10,TFT_BLACK);
  display.setCursor(0,0);
  display.setTextColor(TFT_WHITE);
  display.setTextWrap(false);
  display.print("DCF:");
  display.setCursor(30,0);
  switch(status) {
    case 0:
      Serial.printf("[DCF77] %05d Status: Receiver disconnected\n", uptimeSeconds);
      display.setTextColor(TFT_ORANGE);
      display.print("Disconnected");
      syncedShown = 0;
      break;
    case 1:
      Serial.printf("[DCF77] %05d Status: No signal\n", uptimeSeconds);
      display.setTextColor(TFT_RED);
      display.print("No signal");
      syncedShown = 0;
      break;
    case 2:
      Serial.printf("[DCF77] %05d Status: Signal detected, not yet synced\n", uptimeSeconds);
      display.setTextColor(TFT_MAGENTA);
      display.print("Synchronising");
      syncedShown = 0;
      break;
    case 3:
      if (syncedShown == 0)
        Serial.printf("[DCF77] %05d Status: Signal synced\n", uptimeSeconds);
      display.setTextColor(TFT_GREEN);
      display.print("OK");
      syncedShown = 1;
      break;
    default:
      Serial.printf("[DCF77] %05d Status: Unknown status\n", uptimeSeconds);
      syncedShown = 0;
      break;
  }
  if (uptimeSeconds > 32000) uptimeSeconds = 0;

  uptimeSeconds++;

  
  delay(1000);
}