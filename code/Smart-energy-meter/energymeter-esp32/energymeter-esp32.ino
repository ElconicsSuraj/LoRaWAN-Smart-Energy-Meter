#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
#define PZEM_ADDR   0x01   // use 0xF8 if address is unknown (single module only)

uint16_t crc16(const uint8_t *buf, uint8_t len) {
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= buf[i];
    for (uint8_t b = 0; b < 8; b++)
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

bool readPZEM(float &v, float &i, float &p, float &e, float &f, float &pf, uint16_t &alarm) {
  uint8_t req[8] = {PZEM_ADDR, 0x04, 0x00, 0x00, 0x00, 0x0A, 0, 0};
  uint16_t c = crc16(req, 6);
  req[6] = c & 0xFF; req[7] = c >> 8;

  while (Serial2.available()) Serial2.read();      // flush old bytes
  Serial2.write(req, 8);

  uint8_t resp[25];
  if (Serial2.readBytes(resp, 25) != 25) return false;          // timeout
  if (resp[0] != PZEM_ADDR || resp[1] != 0x04 || resp[2] != 20) return false;
  c = crc16(resp, 23);
  if (resp[23] != (c & 0xFF) || resp[24] != (c >> 8)) return false;  // CRC fail

  auto reg = [&](int n) -> uint16_t { return (resp[3 + 2*n] << 8) | resp[4 + 2*n]; };

  v     = reg(0) * 0.1f;
  i     = (((uint32_t)reg(2) << 16) | reg(1)) * 0.001f;
  p     = (((uint32_t)reg(4) << 16) | reg(3)) * 0.1f;
  e     = (((uint32_t)reg(6) << 16) | reg(5));        // Wh
  f     = reg(7) * 0.1f;
  pf    = reg(8) * 0.01f;
  alarm = reg(9);
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
  Serial2.setTimeout(500);
}

void loop() {
  float v, i, p, e, f, pf; uint16_t alarm;
  if (readPZEM(v, i, p, e, f, pf, alarm)) {
    Serial.printf("V: %.1f V | I: %.3f A | P: %.1f W | E: %.0f Wh | F: %.1f Hz | PF: %.2f | Alarm: %s\n",
                  v, i, p, e, f, pf, alarm ? "YES" : "no");
  } else {
    Serial.println("PZEM read failed");
  }
  delay(2000);
}