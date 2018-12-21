#include <PN5180.h>
#include <PN5180ISO15693.h>

#define PN5180_RST  2
#define PN5180_NSS  1
#define PN5180_BUSY 0

#define PLAYER_N 1

// Actions
#define INSERT_CARD_ACTION 0x01
#define REMOVE_CARD_ACTION 0x02

PN5180ISO15693 nfc(PN5180_NSS, PN5180_BUSY, PN5180_RST);

bool errorFlag = false;
uint8_t previousUid[8];
uint8_t nullUid[8] = {0, 0, 0, 0, 0, 0, 0, 0};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("=================================="));
  Serial.println(F("Uploaded: " __DATE__ " " __TIME__));
  Serial.println(F("PN5180 ISO15693 Demo Sketch"));

  nfc.begin();

  Serial.println(F("----------------------------------"));
  Serial.println(F("PN5180 Hard-Reset..."));
  nfc.reset();

  Serial.println(F("----------------------------------"));
  Serial.println(F("Reading product version..."));
  uint8_t productVersion[2];
  nfc.readEEprom(PRODUCT_VERSION, productVersion, sizeof(productVersion));
  Serial.print(F("Product version="));
  Serial.print(productVersion[1]);
  Serial.print(".");
  Serial.println(productVersion[0]);

  if (0xff == productVersion[1]) { // if product version 255, the initialization failed
    Serial.println(F("Initialization failed!?"));
    Serial.println(F("Press reset to restart..."));
    Serial.flush();
    exit(-1); // halt
  }

  Serial.println(F("----------------------------------"));
  Serial.println(F("Reading firmware version..."));
  uint8_t firmwareVersion[2];
  nfc.readEEprom(FIRMWARE_VERSION, firmwareVersion, sizeof(firmwareVersion));
  Serial.print(F("Firmware version="));
  Serial.print(firmwareVersion[1]);
  Serial.print(".");
  Serial.println(firmwareVersion[0]);

  Serial.println(F("----------------------------------"));
  Serial.println(F("Reading EEPROM version..."));
  uint8_t eepromVersion[2];
  nfc.readEEprom(EEPROM_VERSION, eepromVersion, sizeof(eepromVersion));
  Serial.print(F("EEPROM version="));
  Serial.print(eepromVersion[1]);
  Serial.print(".");
  Serial.println(eepromVersion[0]);

  Serial.println(F("----------------------------------"));
  Serial.println(F("Enable RF field..."));
  nfc.setupRF();

  for (int i=0; i<8; i++) {
    previousUid[i] = nullUid[i];
  }
}



void loop() {
  int n;
  byte buffer[64];
  n = RawHID.recv(buffer, 0); // 0 timeout = do not wait
  if (n > 0) {
    Keyboard.press(KEYPAD_PLUS);
    delay(20);
    Keyboard.release(KEYPAD_PLUS);
  }

  if (errorFlag) {
    uint32_t irqStatus = nfc.getIRQStatus();
    showIRQStatus(irqStatus);

    nfc.reset();
    nfc.setupRF();

    errorFlag = false;
  }

  uint8_t uid[8];
  ISO15693ErrorCode rc = nfc.getInventory(uid);
  if (ISO15693_EC_OK != rc) {
    Serial.print(F("Error in getInventory: "));
    Serial.println(nfc.strerror(rc));
    errorFlag = true;

    if (!compareUid(previousUid, nullUid)) {
      removeCardAction();
    }

    return;
  }

  uint8_t blockSize, numBlocks;
  rc = nfc.getSystemInfo(uid, &blockSize, &numBlocks);
  if (ISO15693_EC_OK != rc) {
    Serial.print(F("Error in getSystemInfo: "));
    Serial.println(nfc.strerror(rc));
    errorFlag = true;
    return;
  }

  if (compareUid(previousUid, nullUid) || !compareUid(previousUid, uid)) {
    if (!compareUid(previousUid, nullUid)) {
        removeCardAction();
    }
    insertCardAction(uid);
  }

  delay(100);
}

bool compareUid(uint8_t a[8], uint8_t b[8]) {
  for (int i=0; i<8; i++) {
    if (a[i] != b[i]) {
      return false;
    }
  }

  return true;
}

void sendAction(uint8_t action, uint8_t data[61]) {
  byte buffer[64];

  // first 2 bytes are a signature
  buffer[0] = 0xEA; // EAmusement
  buffer[1] = 0xCA; // CArd Reader
  // byte 3 is the action
  buffer[2] = action;

  // Insert payload
  for (int i=0; i<61; i++) {
    // byte 4 signifies which player it belongs to (1 == Player 1, 2 == Player 2 etc)
    buffer[3+i] = data[i];
  }

  Serial.println("Sending data:");
  for (int i=0; i<64; i++) {
    if ((i+1) % 16 == 1) {
      Serial.print("\n");
    }
    Serial.print(String(toHex(buffer[i], 2)) + " ");
  }
  Serial.println();

  RawHID.send(buffer, 100);
}

void insertCardAction(uint8_t uid[8]) {
  uint8_t data[61];

  data[0] = PLAYER_N; // Player number

  Serial.println("Card detected, ID: ");
  // Insert card id
  for (int i=0; i<8; i++) {
    previousUid[7-i] = uid[7-i];
    data[1+i] = uid[7-i];
    String hex = toHex(uid[7-i], 2);
    Serial.print(hex);
  }
  Serial.println();

  // rest of the bytes are zeroed
  for (int i=9; i<63; i++) {
    data[i] = 0;
  }

  sendAction(INSERT_CARD_ACTION, data);
}

void removeCardAction() {
  for (int i=0; i<8; i++) {
    previousUid[i] = 0;
  }

  uint8_t data[61];
  data[0] = PLAYER_N; // Player number
  // rest of the bytes are zeroed
  for (int i=1; i<61; i++) {
    data[i] = 0;
  }
  sendAction(REMOVE_CARD_ACTION, data);
}

String toHex(int num, int precision) {
   char tmp[16];
   char format[128];

   sprintf(format, "%%.%dX", precision);

   sprintf(tmp, format, num);

   return tmp;
}

void dumpUid(String label, uint8_t uid[8]) {
  Serial.println();
  Serial.print(label + ": ");
  for (int i=0; i<8; i++) {
    Serial.print(String(toHex(uid[7-i], 2)) + " ");
  }
}

void showIRQStatus(uint32_t irqStatus) {
  Serial.print(F("IRQ-Status 0x"));
  Serial.print(irqStatus, HEX);
  Serial.print(": [ ");
  if (irqStatus & (1<< 0)) Serial.print(F("RQ "));
  if (irqStatus & (1<< 1)) Serial.print(F("TX "));
  if (irqStatus & (1<< 2)) Serial.print(F("IDLE "));
  if (irqStatus & (1<< 3)) Serial.print(F("MODE_DETECTED "));
  if (irqStatus & (1<< 4)) Serial.print(F("CARD_ACTIVATED "));
  if (irqStatus & (1<< 5)) Serial.print(F("STATE_CHANGE "));
  if (irqStatus & (1<< 6)) Serial.print(F("RFOFF_DET "));
  if (irqStatus & (1<< 7)) Serial.print(F("RFON_DET "));
  if (irqStatus & (1<< 8)) Serial.print(F("TX_RFOFF "));
  if (irqStatus & (1<< 9)) Serial.print(F("TX_RFON "));
  if (irqStatus & (1<<10)) Serial.print(F("RF_ACTIVE_ERROR "));
  if (irqStatus & (1<<11)) Serial.print(F("TIMER0 "));
  if (irqStatus & (1<<12)) Serial.print(F("TIMER1 "));
  if (irqStatus & (1<<13)) Serial.print(F("TIMER2 "));
  if (irqStatus & (1<<14)) Serial.print(F("RX_SOF_DET "));
  if (irqStatus & (1<<15)) Serial.print(F("RX_SC_DET "));
  if (irqStatus & (1<<16)) Serial.print(F("TEMPSENS_ERROR "));
  if (irqStatus & (1<<17)) Serial.print(F("GENERAL_ERROR "));
  if (irqStatus & (1<<18)) Serial.print(F("HV_ERROR "));
  if (irqStatus & (1<<19)) Serial.print(F("LPCD "));
  Serial.println("]");
}
