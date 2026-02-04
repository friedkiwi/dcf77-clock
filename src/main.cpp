#include <DCF77FreeRTOS.h>


// Create DCF77 receiver instance
// Parameters: input pin, debug flag (DCF_DEBUG / DCF_NODEBUG)
DCF77FreeRTOS receiver(17, DCF_NODEBUG);
int uptimeSeconds = 0;
int syncedShown = 0;

void setup() {
  Serial.begin(9600);
  delay(100);  // give serial time to initialize
  Serial.println("DCF77FreeRTOS Basic Demo starting...");

  receiver.begin(-1);  // optional LED pin
}

void loop() {
  DCFtime t;
  if (receiver.getTime(&t)) {
    Serial.printf(
      "DCF time: %02u-%02u-%02u %02u:%02u (dow=%u)\n",
      t.year, t.month, t.day, t.hour, t.minute, t.dow
    );
  }

  int status = receiver.getStatus();
  switch(status) {
    case 0:
      Serial.printf("[DCF77] %05d Status: Receiver disconnected\n", uptimeSeconds);
      syncedShown = 0;
      break;
    case 1:
      Serial.printf("[DCF77] %05d Status: No signal\n", uptimeSeconds);
      syncedShown = 0;
      break;
    case 2:
      Serial.printf("[DCF77] %05d Status: Signal detected, not yet synced\n", uptimeSeconds);
      syncedShown = 0;
      break;
    case 3:
      if (syncedShown == 0)
        Serial.printf("[DCF77] %05d Status: Signal synced\n", uptimeSeconds);
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