#include "M5HatMiniJoyC.h"

void M5HatMiniJoyC::writeBytes(uint8_t addr, uint8_t reg, uint8_t *buffer,
                               uint8_t length) {
    _wire->beginTransmission(addr);
    _wire->write(reg);
    for (int i = 0; i < length; i++) {
        _wire->write(*(buffer + i));
    }
    _wire->endTransmission();
}

bool M5HatMiniJoyC::readBytes(uint8_t addr, uint8_t reg, uint8_t *buffer,
                              uint8_t length) {
    uint8_t index = 0;
    for (int i = 0; i < length; i++) {
        buffer[i] = 0;
    }
    _wire->beginTransmission(addr);
    _wire->write(reg);
    if (_wire->endTransmission() != 0) {
        _lastReadOk = false;
        _readErrorCount++;
        return false;
    }

    uint8_t received = _wire->requestFrom(addr, length);
    if (received != length) {
        while (_wire->available()) {
            (void)_wire->read();
        }
        _lastReadOk = false;
        _readErrorCount++;
        return false;
    }

    while (_wire->available() && index < length) {
        buffer[index++] = _wire->read();
    }

    if (index != length) {
        _lastReadOk = false;
        _readErrorCount++;
        return false;
    }

    _lastReadOk = true;
    return true;
}

bool M5HatMiniJoyC::begin(TwoWire *wire, uint8_t addr, uint8_t sda, uint8_t scl,
                          uint32_t speed) {
    _wire  = wire;
    _addr  = addr;
    _sda   = sda;
    _scl   = scl;
    _speed = speed;
    _lastReadOk = true;
    _readErrorCount = 0;
    _wire->begin(_sda, _scl, _speed);
    delay(10);
    _wire->beginTransmission(_addr);
    uint8_t error = _wire->endTransmission();
    if (error == 0) {
        return true;
    } else {
        return false;
    }
}

uint16_t M5HatMiniJoyC::getADCValue(minijoyc_adc_index_t index) {
    uint8_t data[4];
    if (index > 2) return 0;
    uint8_t reg = index * 2 + ADC_VALUE_REG;
    if (!readBytes(_addr, reg, data, 2)) return 0;
    uint32_t value = data[0] | (data[1] << 8);
    return value;
}

uint16_t M5HatMiniJoyC::getPOSValue(minijoyc_pos_index_t index,
                                    minijoyc_pos_read_mode_t bit) {
    uint8_t data[4];
    uint32_t value = 0;

    if (index > 2) return 0;
    if (bit == _10bit) {
        uint8_t reg = index * 2 + POS_VALUE_REG_10_BIT;
        if (!readBytes(_addr, reg, data, 2)) return 0;
        value = data[0] | (data[1] << 8);
    } else if (bit == _8bit) {
        uint8_t reg = index + POS_VALUE_REG_8_BIT;
        if (!readBytes(_addr, reg, data, 1)) return 0;
        value = data[0];
    }

    return value;
}

bool M5HatMiniJoyC::getButtonStatus() {
    uint8_t data;
    if (!readBytes(_addr, BUTTON_REG, &data, 1)) return false;
    return data;
}

void M5HatMiniJoyC::setLEDColor(uint32_t rgb888color) {
    uint8_t data[4];
    data[2] = rgb888color & 0xff;
    data[1] = (rgb888color >> 8) & 0xff;
    data[0] = (rgb888color >> 16) & 0xff;
    writeBytes(_addr, RGB_LED_REG, data, 3);
}

void M5HatMiniJoyC::setOneCalValue(minijoyc_cal_index_t index, uint16_t data) {
    uint8_t reg;
    uint8_t buf[4];

    reg    = index * 2 + CAL_REG;
    buf[0] = data & 0xff;
    buf[1] = (data >> 8) & 0xff;
    writeBytes(_addr, reg, buf, 2);
    delay(1000);
}

void M5HatMiniJoyC::setAllCalValue(uint16_t *data) {
    writeBytes(_addr, CAL_REG, (uint8_t *)data, 12);
    delay(1000);
}

uint16_t M5HatMiniJoyC::getCalValue(minijoyc_cal_index_t index) {
    if (index > 5) return 0;
    uint8_t data[4];
    uint8_t reg = index * 2 + CAL_REG;
    if (!readBytes(_addr, reg, data, 2)) return 0;
    uint16_t value = data[0] | (data[1] << 8);
    return value;
}

uint8_t M5HatMiniJoyC::setI2CAddress(uint8_t addr) {
    _wire->beginTransmission(_addr);
    _wire->write(I2C_ADDRESS_REG);
    _wire->write(addr);
    _wire->endTransmission();
    _addr = addr;
    return _addr;
}

uint8_t M5HatMiniJoyC::getI2CAddress(void) {
    _wire->beginTransmission(_addr);
    _wire->write(I2C_ADDRESS_REG);
    _wire->endTransmission();

    uint8_t RegValue;

    Wire.requestFrom(_addr, (uint8_t)1);
    RegValue = Wire.read();
    return RegValue;
}

uint8_t M5HatMiniJoyC::getFirmwareVersion(void) {
    _wire->beginTransmission(_addr);
    _wire->write(FIRMWARE_VERSION_REG);
    _wire->endTransmission();

    uint8_t RegValue;

    Wire.requestFrom(_addr, (uint8_t)1);
    RegValue = Wire.read();
    return RegValue;
}

bool M5HatMiniJoyC::wasLastReadSuccessful(void) const {
    return _lastReadOk;
}

uint32_t M5HatMiniJoyC::getReadErrorCount(void) const {
    return _readErrorCount;
}
