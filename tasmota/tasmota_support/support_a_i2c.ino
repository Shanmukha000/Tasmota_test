/*
  support_a_i2c.ino - I2C support for Tasmota

  SPDX-FileCopyrightText: 2022 Theo Arends

  SPDX-License-Identifier: GPL-3.0-only
*/

#ifdef USE_I2C
/*********************************************************************************************\
 * Basic I2C routines supporting two busses
\*********************************************************************************************/

const uint8_t I2C_RETRY_COUNTER = 3;

#ifdef ESP8266
uint32_t i2c_active[1][4] = { 0 };
#else
uint32_t i2c_active[2][4] = { 0 };
#endif
uint32_t i2c_buffer = 0;

bool I2cBegin(int sda, int scl, uint32_t frequency = 100000);
bool I2cBegin(int sda, int scl, uint32_t frequency) {
  bool result = true;
#ifdef ESP8266
  Wire.begin(sda, scl);
#endif
#ifdef ESP32
#if ESP_IDF_VERSION_MAJOR > 3  // Core 2.x uses a different I2C library
  static bool reinit = false;
  if (reinit) { Wire.end(); }
#endif  // ESP_IDF_VERSION_MAJOR > 3
  result = Wire.begin(sda, scl, frequency);
#if ESP_IDF_VERSION_MAJOR > 3  // Core 2.x uses a different I2C library
  reinit = result;
#endif  // ESP_IDF_VERSION_MAJOR > 3
#endif
//  AddLog(LOG_LEVEL_DEBUG, PSTR("I2C: Bus1 %d"), result);
  return result;
}

#ifdef ESP32
bool I2c2Begin(int sda, int scl, uint32_t frequency = 100000);
bool I2c2Begin(int sda, int scl, uint32_t frequency) {
  bool result = Wire1.begin(sda, scl, frequency);
//  AddLog(LOG_LEVEL_DEBUG, PSTR("I2C: Bus2 %d"), result);
  return result;
}
#endif

bool I2cValidRead(uint8_t addr, uint8_t reg, uint8_t size, uint8_t bus = 0) {
  uint8_t retry = I2C_RETRY_COUNTER;
  bool status = false;
#ifdef ESP32
  if (!TasmotaGlobal.i2c_enabled_2) { bus = 0; }
  TwoWire & myWire = (bus == 0) ? Wire : Wire1;
#else
  TwoWire & myWire = Wire;
#endif
  i2c_buffer = 0;
  while (!status && retry) {
    myWire.beginTransmission(addr);                       // start transmission to device
    myWire.write(reg);                                    // sends register address to read from
    if (0 == myWire.endTransmission(false)) {             // Try to become I2C Master, send data and collect bytes, keep master status for next request...
      myWire.requestFrom((int)addr, (int)size);           // send data n-bytes read
      if (myWire.available() == size) {
        for (uint32_t i = 0; i < size; i++) {
          i2c_buffer = i2c_buffer << 8 | myWire.read();   // receive DATA
        }
        status = true;
      }
    }
    retry--;
  }
  if (!retry) myWire.endTransmission();
  return status;
}

bool I2cValidRead8(uint8_t *data, uint8_t addr, uint8_t reg, uint8_t bus = 0) {
  bool status = I2cValidRead(addr, reg, 1, bus);
  *data = (uint8_t)i2c_buffer;
  return status;
}

bool I2cValidRead16(uint16_t *data, uint8_t addr, uint8_t reg, uint8_t bus = 0) {
  bool status = I2cValidRead(addr, reg, 2, bus);
  *data = (uint16_t)i2c_buffer;
  return status;
}

bool I2cValidReadS16(int16_t *data, uint8_t addr, uint8_t reg, uint8_t bus = 0) {
  bool status = I2cValidRead(addr, reg, 2, bus);
  *data = (int16_t)i2c_buffer;
  return status;
}

bool I2cValidRead16LE(uint16_t *data, uint8_t addr, uint8_t reg, uint8_t bus = 0) {
  uint16_t ldata;
  bool status = I2cValidRead16(&ldata, addr, reg, bus);
  *data = (ldata >> 8) | (ldata << 8);
  return status;
}

bool I2cValidReadS16_LE(int16_t *data, uint8_t addr, uint8_t reg, uint8_t bus = 0) {
  uint16_t ldata;
  bool status = I2cValidRead16LE(&ldata, addr, reg, bus);
  *data = (int16_t)ldata;
  return status;
}

bool I2cValidRead24(int32_t *data, uint8_t addr, uint8_t reg, uint8_t bus = 0) {
  bool status = I2cValidRead(addr, reg, 3, bus);
  *data = i2c_buffer;
  return status;
}

uint8_t I2cRead8(uint8_t addr, uint8_t reg, uint8_t bus = 0) {
  I2cValidRead(addr, reg, 1, bus);
  return (uint8_t)i2c_buffer;
}

uint16_t I2cRead16(uint8_t addr, uint8_t reg, uint8_t bus = 0) {
  I2cValidRead(addr, reg, 2, bus);
  return (uint16_t)i2c_buffer;
}

int16_t I2cReadS16(uint8_t addr, uint8_t reg, uint8_t bus = 0) {
  I2cValidRead(addr, reg, 2, bus);
  return (int16_t)i2c_buffer;
}

uint16_t I2cRead16LE(uint8_t addr, uint8_t reg, uint8_t bus = 0) {
  I2cValidRead(addr, reg, 2, bus);
  uint16_t temp = (uint16_t)i2c_buffer;
  return (temp >> 8) | (temp << 8);
}

int16_t I2cReadS16_LE(uint8_t addr, uint8_t reg, uint8_t bus = 0) {
  return (int16_t)I2cRead16LE(addr, reg, bus);
}

int32_t I2cRead24(uint8_t addr, uint8_t reg, uint8_t bus = 0) {
  I2cValidRead(addr, reg, 3, bus);
  return i2c_buffer;
}

bool I2cWrite(uint8_t addr, uint8_t reg, uint32_t val, uint8_t size, uint8_t bus = 0) {
  uint8_t x = I2C_RETRY_COUNTER;
#ifdef ESP32
  if (!TasmotaGlobal.i2c_enabled_2) { bus = 0; }
  TwoWire & myWire = (bus == 0) ? Wire : Wire1;
#else
  if (bus != 0) { return false; }  // Second I2c bus ESP32 only
  TwoWire & myWire = Wire;
#endif
  do {
    myWire.beginTransmission((uint8_t)addr);              // start transmission to device
    myWire.write(reg);                                    // sends register address to write to
    uint8_t bytes = size;
    while (bytes--) {
      myWire.write((val >> (8 * bytes)) & 0xFF);          // write data
    }
    x--;
  } while (myWire.endTransmission(true) != 0 && x != 0);  // end transmission
  return (x);
}

bool I2cWrite8(uint8_t addr, uint8_t reg, uint32_t val, uint8_t bus = 0) {
   return I2cWrite(addr, reg, val, 1, bus);
}

bool I2cWrite16(uint8_t addr, uint8_t reg, uint32_t val, uint8_t bus = 0) {
   return I2cWrite(addr, reg, val, 2, bus);
}

bool I2cReadBuffer(uint8_t addr, uint8_t reg, uint8_t *reg_data, uint16_t len, uint8_t bus = 0) {
#ifdef ESP32
  if (!TasmotaGlobal.i2c_enabled_2) { bus = 0; }
  TwoWire & myWire = (bus == 0) ? Wire : Wire1;
#else
  if (bus != 0) { return false; }  // Second I2c bus ESP32 only
  TwoWire & myWire = Wire;
#endif
  myWire.beginTransmission((uint8_t)addr);
  myWire.write((uint8_t)reg);
  myWire.endTransmission();
  if (len != myWire.requestFrom((uint8_t)addr, (uint8_t)len)) {
    return 1;
  }
  while (len--) {
    *reg_data = (uint8_t)myWire.read();
    reg_data++;
  }
  return 0;
}

int8_t I2cWriteBuffer(uint8_t addr, uint8_t reg, uint8_t *reg_data, uint16_t len, uint8_t bus = 0) {
#ifdef ESP32
  if (!TasmotaGlobal.i2c_enabled_2) { bus = 0; }
  TwoWire & myWire = (bus == 0) ? Wire : Wire1;
#else
  if (bus != 0) { return false; }  // Second I2c bus ESP32 only
  TwoWire & myWire = Wire;
#endif
  myWire.beginTransmission((uint8_t)addr);
  myWire.write((uint8_t)reg);
  while (len--) {
    myWire.write(*reg_data);
    reg_data++;
  }
  myWire.endTransmission();
  return 0;
}

void I2cScan(uint8_t bus = 0) {
  // Return error codes defined in twi.h and core_esp8266_si2c.c
  // I2C_OK                      0
  // I2C_SCL_HELD_LOW            1 = SCL held low by another device, no procedure available to recover
  // I2C_SCL_HELD_LOW_AFTER_READ 2 = I2C bus error. SCL held low beyond client clock stretch time
  // I2C_SDA_HELD_LOW            3 = I2C bus error. SDA line held low by client/another_master after n bits
  // I2C_SDA_HELD_LOW_AFTER_INIT 4 = line busy. SDA again held low by another device. 2nd master?
  //                             5 = bus busy. Timeout
  // https://www.arduino.cc/reference/en/language/functions/communication/wire/endtransmission/
  // 0: success
  // 1: data too long to fit in transmit buffer
  // 2: received NACK on transmit of address
  // 3: received NACK on transmit of data
  // 4: other error
  // 5: timeout

  uint8_t error = 0;
  uint8_t address = 0;
  uint8_t any = 0;

  Response_P(PSTR("{\"" D_CMND_I2CSCAN "\":\"" D_JSON_I2CSCAN_DEVICES_FOUND_AT));
  for (address = 1; address <= 127; address++) {
#ifdef ESP32
    if (!TasmotaGlobal.i2c_enabled_2) { bus = 0; }
    TwoWire & myWire = (bus == 0) ? Wire : Wire1;
#else
    if (bus != 0) { return; }  // Second I2c bus ESP32 only
    TwoWire & myWire = Wire;
#endif
    myWire.beginTransmission(address);
    error = myWire.endTransmission();
    if (0 == error) {
      any = 1;
#ifdef ESP32
      ResponseAppend_P(PSTR(" 0x%02x:%d"), address, bus);
#else
      ResponseAppend_P(PSTR(" 0x%02x"), address);
#endif
    }
    else if (error != 2) {  // Seems to happen anyway using this scan
      any = 2;
      Response_P(PSTR("{\"" D_CMND_I2CSCAN "\":\"Error %d at 0x%02x bus %d"), error, address, bus);
      break;
    }
  }
  if (any) {
    ResponseAppend_P(PSTR("\"}"));
  } else {
    Response_P(PSTR("{\"" D_CMND_I2CSCAN "\":\"" D_JSON_I2CSCAN_NO_DEVICES_FOUND "\"}"));
  }
}

void I2cResetActive(uint32_t addr, uint32_t count = 1, uint8_t bus = 0) {
#ifdef ESP8266
  bus = 0;
#endif
  addr &= 0x7F;         // Max I2C address is 127
  count &= 0x7F;        // Max 4 x 32 bits available
  while (count-- && (addr < 128)) {
    i2c_active[bus][addr / 32] &= ~(1 << (addr % 32));
    addr++;
  }
//  AddLog(LOG_LEVEL_DEBUG, PSTR("I2C: Active %08X,%08X,%08X,%08X"), i2c_active[bus][0], i2c_active[bus][1], i2c_active[bus][2], i2c_active[bus][3]);
}

void I2cSetActive(uint32_t addr, uint32_t count = 1, uint8_t bus = 0) {
#ifdef ESP8266
  bus = 0;
#endif
  addr &= 0x7F;         // Max I2C address is 127
  count &= 0x7F;        // Max 4 x 32 bits available
  while (count-- && (addr < 128)) {
    i2c_active[bus][addr / 32] |= (1 << (addr % 32));
    addr++;
  }
//  AddLog(LOG_LEVEL_DEBUG, PSTR("I2C: Active %08X,%08X,%08X,%08X"), i2c_active[bus][0], i2c_active[bus][1], i2c_active[bus][2], i2c_active[bus][3]);
}

void I2cSetActiveFound(uint32_t addr, const char *types, uint8_t bus = 0) {
  I2cSetActive(addr, bus);
#ifdef ESP32
  if (1 == bus) {
    AddLog(LOG_LEVEL_INFO, S_LOG_I2C_FOUND_AT_PORT, types, addr, bus);
  } else
#endif // ESP32
  AddLog(LOG_LEVEL_INFO, S_LOG_I2C_FOUND_AT, types, addr);
}

bool I2cActive(uint32_t addr, uint8_t bus = 0) {
#ifdef ESP8266
  bus = 0;
#endif
  addr &= 0x7F;         // Max I2C address is 127
  return (i2c_active[bus][addr / 32] & (1 << (addr % 32)));
}

bool I2cSetDevice(uint32_t addr, uint8_t bus = 0) {
#ifdef ESP32
  if (!TasmotaGlobal.i2c_enabled_2) { bus = 0; }
  TwoWire & myWire = (bus == 0) ? Wire : Wire1;
#else
  TwoWire & myWire = Wire;
#endif
  addr &= 0x7F;         // Max I2C address is 127
  if (I2cActive(addr, bus)) {
    return false;       // If already active report as not present;
  }
  myWire.beginTransmission((uint8_t)addr);
//  return (0 == myWire.endTransmission());
  uint32_t err = myWire.endTransmission();
  if (err && (err != 2)) {
#ifdef ESP32
    AddLog(LOG_LEVEL_DEBUG, PSTR("I2C: Error %d at 0x%02x bus %d"), err, addr, bus);
#else
    AddLog(LOG_LEVEL_DEBUG, PSTR("I2C: Error %d at 0x%02x"), err, addr);
#endif
  }
  return (0 == err);
}
#endif  // USE_I2C