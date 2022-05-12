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
  int8_t gpioPins[USERMOD_MST_MAX_CH] = {
      USERMOD_MST_CH1_GPIO,
      USERMOD_MST_CH2_GPIO,
      USERMOD_MST_CH3_GPIO,
  };

  ulong lastTime = 0;
  bool initDone = false;
  bool knownChannelState[USERMOD_MST_MAX_CH];

  static const char _name[];
  static const char _pin[];
  static const char _type[];
  static const char _color[];
  static const char _ch[];
  static const char _chMap[];
  static const char _enabled[];

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
      if (gpioPins[ch] < 0) continue;
      
      pins[ch] = {gpioPins[ch], false};
      nCh++;
    }
    if (!pinManager.allocateMultiplePins(pins, nCh, po))
    {
      DEBUG_PRINTLN(F("Multi-strip type: Failed to allocate pins"));
      return;
    }

    for (uint8_t ch = 0; ch < USERMOD_MST_MAX_CH; ch++)
    {
      if (gpioPins[ch] < 0) continue;
      
      uint8_t busId = chToBusMap[ch];
      Bus *bus = busses.getBus(busId);
      if (bus == nullptr)
      {
        continue;
      }
      uint8_t pins[5];
      bus->getPins(pins);
      BusConfig bc = BusConfig(
          stripTypes[ch][knownChannelState[ch]],
          pins, bus->getStart(),
          bus->getLength(),
          stripColorOrder[ch][knownChannelState[ch]],
          bus->reversed,
          bus->skippedLeds());
      busses.replace(bc, busId);
    }
    initDone = true;
  }

  void
  loop()
  {
    if (!enabled || millis() - lastTime <= 250)
    {
      return;
    }

    lastTime = millis();

    for (uint8_t ch = 0; ch < USERMOD_MST_MAX_CH; ch++)
    {
      if (gpioPins[ch] <= 0) continue;

      bool state = digitalRead(gpioPins[ch]);
#ifdef USERMOD_MST_ACTIVE_LOW
      state ^= HIGH;
#endif

      int8_t busId = chToBusMap[ch];
      if (knownChannelState[busId] == state)
      {
        continue;
      }

      Serial.printf_P(PSTR("MultiStrip: ch: %i, bus: %i state: %s\n"), ch, busId, state ? "on" : "off");

      knownChannelState[busId] = state;
      Bus *bus = busses.getBus(busId);
      if (bus == nullptr)
      {
        continue;
      }
      uint8_t pins[5];
      bus->getPins(pins);
      BusConfig bc = BusConfig(
          stripTypes[ch][state],
          pins, bus->getStart(),
          bus->getLength(),
          stripColorOrder[ch][state],
          bus->reversed,
          bus->skippedLeds());
      busses.replace(bc, busId);
    }
  }

  void addToConfig(JsonObject &root)
  {
    JsonObject top = root.createNestedObject(FPSTR(_name));
    JsonArray stripTypeArray = top.createNestedArray(FPSTR(_type));
    JsonArray colorOrderArray = top.createNestedArray(FPSTR(_color));
    JsonArray chArray = top.createNestedArray(FPSTR(_ch));
    JsonArray chMapArray = top.createNestedArray(FPSTR(_chMap));
    JsonArray pinArray = top.createNestedArray(FPSTR(_pin));
    for (int i = 0; i < USERMOD_MST_MAX_CH; i++)
    {
      stripTypeArray.add(stripTypes[i][0]);
      stripTypeArray.add(stripTypes[i][1]);
      colorOrderArray.add(stripColorOrder[i][0]);
      colorOrderArray.add(stripColorOrder[i][1]);
      pinArray.add(gpioPins[i]);
      chArray.add(knownChannelState[i]);
      chMapArray.add(chToBusMap[i]);
    }
    top[FPSTR(_enabled)] = enabled;
  }

  bool readFromConfig(JsonObject &root)
  {
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull())
    {
      DEBUG_PRINTLN(F("Multi-strip type: no configuration found, using defaults."));
      return false;
    }

    getJsonValue(top[FPSTR(_enabled)], enabled);

    JsonArray stripTypeArray = top[FPSTR(_type)];
    JsonArray colorOrderArray = top[FPSTR(_color)];
    if (!stripTypeArray.isNull() && !colorOrderArray.isNull())
    {
      for (uint8_t i = 0; i < USERMOD_MST_MAX_CH; i++)
      {
        stripTypes[i][0] = stripTypeArray[i * 2];
        stripTypes[i][1] = stripTypeArray[i * 2 + 1];
        stripColorOrder[i][0] = colorOrderArray[i * 2];
        stripColorOrder[i][1] = colorOrderArray[i * 2 + 1];
      }
    }

    JsonArray pinArray = top[FPSTR(_pin)];
    if (!pinArray.isNull())
    {
      int8_t newPins[USERMOD_MST_MAX_CH];
      bool pinsChanged = false;
      for (uint8_t i = 0; i < USERMOD_MST_MAX_CH; i++)
      {
        // Check if pins changed?
        newPins[i] = pinArray[i];
        pinsChanged |= gpioPins[i] != newPins[i];
      }
      if (pinsChanged && initDone)
      {
        PinOwner po = PinOwner::UM_MST;
        pinManager.deallocateMultiplePins((uint8_t *) gpioPins, USERMOD_MST_MAX_CH, po);
        memcpy(gpioPins, newPins, sizeof(gpioPins));
        setup();
      }
      else
      {
        memcpy(gpioPins, newPins, sizeof(gpioPins));
      }
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
const char UsermodMultiStripType::_color[] PROGMEM = "color";
const char UsermodMultiStripType::_ch[] PROGMEM = "ch";
const char UsermodMultiStripType::_chMap[] PROGMEM = "map";
const char UsermodMultiStripType::_enabled[] PROGMEM = "enabled";
