#pragma once

#include "const.h"
#include "wled.h"

/*
 * Usermods allow you to add own functionality to WLED more easily
 * See: https://github.com/Aircoookie/WLED/wiki/Add-own-functionality
 *
 * This is an example for a v2 usermod.
 * v2 usermods are class inheritance based and can (but don't have to) implement more functions, each of them is shown in this example.
 * Multiple v2 usermods can be added to one compilation easily.
 *
 * Creating a usermod:
 * This file serves as an example. If you want to create a usermod, it is recommended to use usermod_v2_empty.h from the usermods folder as a template.
 * Please remember to rename the class and file to a descriptive name.
 * You may also use multiple .h and .cpp files.
 *
 * Using a usermod:
 * 1. Copy the usermod into the sketch folder (same folder as wled00.ino)
 * 2. Register the usermod by adding #include "usermod_filename.h" in the top and registerUsermod(new MyUsermodClass()) in the bottom of usermods_list.cpp
 */

#ifndef USERMOD_MST_EN_GPIO
#define USERMOD_MST_EN_GPIO 2 // Q3_GPIO_PIN
#endif
#ifndef USERMOD_MST_CH1_GPIO
#define USERMOD_MST_CH1_GPIO 15 // Q1_GPIO_PIN
#endif
#ifndef USERMOD_MST_CH2_GPIO
#define USERMOD_MST_CH2_GPIO 12 // Q2_GPIO_PIN
#endif
#ifndef USERMOD_MST_CH3_GPIO
#define USERMOD_MST_CH3_GPIO -1 // disabled
#endif
#define USERMOD_MST_MAX_CH 3

class UsermodMultiStripType : public Usermod
{
private:
  bool enabled = true;
  uint8_t chToBusMap[USERMOD_MST_MAX_CH] = {0, 1, 2};
  uint8_t stripTypes[USERMOD_MST_MAX_CH][2] = {
      {TYPE_SK6812_RGBW, TYPE_WS2812_RGB},
      {TYPE_SK6812_RGBW, TYPE_WS2812_RGB},
      {TYPE_SK6812_RGBW, TYPE_WS2812_RGB},
  };
  uint8_t stripColorOrder[USERMOD_MST_MAX_CH][2] = {
      {COL_ORDER_GRB, COL_ORDER_BRG},
      {COL_ORDER_GRB, COL_ORDER_BRG},
      {COL_ORDER_GRB, COL_ORDER_BRG},
  };
  uint8_t stripLength[USERMOD_MST_MAX_CH][2] = {
      {1, 1},
      {1, 1},
      {1, 1},
  };
  int8_t gpioPins[USERMOD_MST_MAX_CH] = {
      USERMOD_MST_CH1_GPIO,
      USERMOD_MST_CH2_GPIO,
      USERMOD_MST_CH3_GPIO,
  };
  int8_t enablePin = USERMOD_MST_EN_GPIO;

  ulong lastTime = 0;
  bool initDone = false;
  bool hasEnPin = false;
  bool knownChannelState[USERMOD_MST_MAX_CH];
  uint16_t sampleRate = 250;

  static const char _name[];
  static const char _pin[];
  static const char _type[];
  static const char _len[];
  static const char _color[];
  static const char _ch[];
  static const char _chMap[];
  static const char _enabled[];
  static const char _sampleRate[];

public:
  /*
   * setup() is called once at boot. WiFi is not yet connected at this point.
   * You can use it to initialize variables, sensors or similar.
   */
  void setup()
  {
    PinOwner po = PinOwner::UM_MST;
    PinManagerPinType pins[USERMOD_MST_MAX_CH];
    uint8_t nCh = 0;
    for (uint8_t ch = 0; ch < USERMOD_MST_MAX_CH; ch++)
    {
      if (gpioPins[ch] < 0)
        continue;

      pins[ch] = {gpioPins[ch], false};
      nCh++;
    }
    if (!pinManager.allocateMultiplePins(pins, nCh, po))
    {
      Serial.printf_P(PSTR("Multi-strip type: Failed to allocate pins\n"));
      return;
    }
    if (enablePin > -1)
    {
      if (!pinManager.allocatePin(enablePin, true, po))
      {
        Serial.printf_P(PSTR("Multi-strip type: Failed to allocate enable pin. External power required\n"));
      }
      else
      {
        digitalWrite(enablePin, HIGH);
        hasEnPin = true;
      }
    }
    else
    {
      Serial.printf_P(PSTR("Multi-strip type: External power required\n"));
    }

    for (uint8_t ch = 0; ch < USERMOD_MST_MAX_CH; ch++)
    {
      if (gpioPins[ch] < 0)
        continue;

      uint8_t busId = chToBusMap[ch];
      configureBus(ch, busId, knownChannelState[ch]);
    }
    initDone = true;
  }

  void shutdown()
  {
    if (hasEnPin)
    {
      digitalWrite(enablePin, LOW);
    }
  }

  void
  loop()
  {
    if (!enabled || millis() - lastTime <= sampleRate)
    {
      return;
    }

    lastTime = millis();

    for (uint8_t ch = 0; ch < USERMOD_MST_MAX_CH; ch++)
    {
      if (gpioPins[ch] <= 0)
        continue;

      bool state = digitalRead(gpioPins[ch]);
#ifdef USERMOD_MST_ACTIVE_LOW
      state ^= HIGH;
#endif

      int8_t busId = chToBusMap[ch];
      if (knownChannelState[busId] == state)
      {
        continue;
      }

      knownChannelState[busId] = state;
      configureBus(ch, busId, state);
    }
  }

  void configureBus(uint8_t ch, int8_t busId, bool state)
  {
    Bus *bus = busses.getBus(busId);
    if (bus == nullptr)
    {
      return;
    }

    savePrevBusConfig(ch, bus, !state);

    uint8_t pins[5];
    uint16_t start = bus->getStart();
    bus->getPins(pins);
    BusConfig bc = BusConfig(
        stripTypes[ch][state],
        pins,
        start,
        stripLength[ch][state],
        stripColorOrder[ch][state],
        bus->reversed,
        bus->skippedLeds());
    busses.replace(bc, busId);
    recalcNextBus(busId, start, stripLength[ch][state]);
  }

  void recalcNextBus(int8_t prevBusId, uint16_t prevStart, uint16_t prevLength)
  {
    int8_t busId = prevBusId + 1;
    Bus *bus = busses.getBus(busId);
    if (bus == nullptr)
    {
      return;
    }

    uint8_t pins[5];
    uint16_t start = prevStart + prevLength;
    uint16_t len = bus->getLength();
    bus->getPins(pins);
    BusConfig bc = BusConfig(
        bus->getType(),
        pins,
        start,
        len,
        bus->getColorOrder(),
        bus->reversed,
        bus->skippedLeds());
    busses.replace(bc, busId);
    recalcNextBus(busId, start, len);
  }

  void initSegments()
  {
    strip.makeAutoSegments();
    if (currentPreset > 0)
    {
      applyPreset(currentPreset, CALL_MODE_INIT);
    }
  }

  void savePrevBusConfig(uint8_t ch, Bus *bus, bool state)
  {
    if (!initDone) {
      return;
    }
    stripTypes[ch][state] = bus->getType();
    stripColorOrder[ch][state] = bus->getColorOrder();
    stripLength[ch][state] = bus->getLength();
  }

  void addToConfig(JsonObject &root)
  {
    JsonObject top = root.createNestedObject(FPSTR(_name));
    JsonArray stripTypeArray = top.createNestedArray(FPSTR(_type));
    JsonArray stripLenArray = top.createNestedArray(FPSTR(_len));
    JsonArray colorOrderArray = top.createNestedArray(FPSTR(_color));
    JsonArray chArray = top.createNestedArray(FPSTR(_ch));
    JsonArray chMapArray = top.createNestedArray(FPSTR(_chMap));
    JsonArray pinArray = top.createNestedArray(FPSTR(_pin));
    for (int i = 0; i < USERMOD_MST_MAX_CH; i++)
    {
      stripTypeArray.add(stripTypes[i][0]);
      stripTypeArray.add(stripTypes[i][1]);
      stripLenArray.add(stripLength[i][0]);
      stripLenArray.add(stripLength[i][1]);
      colorOrderArray.add(stripColorOrder[i][0]);
      colorOrderArray.add(stripColorOrder[i][1]);
      pinArray.add(gpioPins[i]);
      chArray.add(knownChannelState[i]);
      chMapArray.add(chToBusMap[i]);
    }
    pinArray.add(enablePin);
    top[FPSTR(_enabled)] = enabled;
    top[FPSTR(_sampleRate)] = sampleRate;
  }

  bool readFromConfig(JsonObject &root)
  {
    // Read current bus data before making changes
    if (!initDone)
    {
      for (uint8_t ch = 0; ch < min(USERMOD_MST_MAX_CH, (int)busses.getNumBusses()); ch++)
      {
        Bus *bus = busses.getBus(ch);
        if (bus == nullptr)
        {
          continue;
        }

        savePrevBusConfig(ch, bus, false);
        savePrevBusConfig(ch, bus, true);
      }
    }

    // Read config
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull())
    {
      DEBUG_PRINTLN(F("Multi-strip type: no configuration found, using defaults."));
      return false;
    }

    getJsonValue(top[FPSTR(_enabled)], enabled);
    getJsonValue(top[FPSTR(_sampleRate)], sampleRate);

    JsonArray stripTypeArray = top[FPSTR(_type)];
    JsonArray colorOrderArray = top[FPSTR(_color)];
    JsonArray lenArray = top[FPSTR(_len)];
    bool readType = !stripTypeArray.isNull();
    bool readColor = !colorOrderArray.isNull();
    bool readLen = !lenArray.isNull();
    for (uint8_t i = 0; i < USERMOD_MST_MAX_CH; i++)
    {
      // Why is it setting the same value for both states? It uses the first value :@
      if (readType)
      {
        stripTypes[i][0] = stripTypeArray[i * 2];
        stripTypes[i][1] = stripTypeArray[i * 2 + 1];
      }
      if (readColor)
      {
        stripColorOrder[i][0] = colorOrderArray[i * 2];
        stripColorOrder[i][1] = colorOrderArray[i * 2 + 1];
      }
      if (readLen)
      {
        stripLength[i][0] = lenArray[i * 2];
        stripLength[i][1] = lenArray[i * 2 + 1];
      }
    }

    PinOwner po = PinOwner::UM_MST;
    bool pinsChanged = false;

    int8_t curPins[USERMOD_MST_MAX_CH];
    int8_t curEnPin = enablePin;
    memcpy(curPins, gpioPins, sizeof(gpioPins));

    JsonArray pinArray = top[FPSTR(_pin)];
    if (!pinArray.isNull())
    {
      for (uint8_t i = 0; i < USERMOD_MST_MAX_CH; i++)
      {
        gpioPins[i] = pinArray[i];
        pinsChanged |= gpioPins[i] != curPins[i];
      }
    }
    uint8_t enablePinIdx = USERMOD_MST_MAX_CH; // This is the last item in the array or n+1 of gpio pins
    if (pinArray.size() - 1 == enablePinIdx)
    {
      enablePin = pinArray[enablePinIdx];
      pinsChanged |= enablePin != curEnPin;
    }

    JsonArray chMapArray = top[FPSTR(_chMap)];
    if (!chMapArray.isNull())
    {
      for (uint8_t i = 0; i < USERMOD_MST_MAX_CH; i++)
      {
        chToBusMap[i] = chMapArray[i];
      }
    }

    JsonArray chArray = top[FPSTR(_ch)];
    if (!chArray.isNull())
    {
      for (uint8_t i = 0; i < USERMOD_MST_MAX_CH; i++)
      {
        knownChannelState[i] = chArray[i];
      }
    }

    if (pinsChanged && initDone)
    {
      digitalWrite(curEnPin, LOW);
      pinManager.deallocateMultiplePins((uint8_t *)curPins, USERMOD_MST_MAX_CH, po);
      pinManager.deallocatePin(curEnPin, po);
      setup();
    }

    return true;
  }

  uint16_t getId()
  {
    return USERMOD_ID_MST;
  }
};

const char UsermodMultiStripType::_name[] PROGMEM = "MultiStripType";
const char UsermodMultiStripType::_pin[] PROGMEM = "pin";
const char UsermodMultiStripType::_type[] PROGMEM = "type";
const char UsermodMultiStripType::_len[] PROGMEM = "len";
const char UsermodMultiStripType::_color[] PROGMEM = "color";
const char UsermodMultiStripType::_ch[] PROGMEM = "ch";
const char UsermodMultiStripType::_chMap[] PROGMEM = "map";
const char UsermodMultiStripType::_enabled[] PROGMEM = "enabled";
const char UsermodMultiStripType::_sampleRate[] PROGMEM = "sample";
