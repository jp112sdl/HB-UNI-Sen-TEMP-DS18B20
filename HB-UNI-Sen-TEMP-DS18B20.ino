//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2018-03-24 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER

//#define USE_LCD
//#define LCD_ADDRESS 0x3f

#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>
#include <AskSinPP.h>
#include <LowPower.h>

#include <Register.h>
#include <MultiChannelDevice.h>

#ifdef USE_LCD
//use this LCD lib: https://github.com/marcoschwartz/LiquidCrystal_I2C
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(LCD_ADDRESS, 20, 4);
#endif

#include <OneWire.h>
#include <sensors/Ds18b20.h>
#define MAX_SENSORS       8

// Arduino Pro mini 8 Mhz
// Arduino pin for the config button
#define CONFIG_BUTTON_PIN 8
#define LED_PIN           4

// number of available peers per channel
#define PEERS_PER_CHANNEL 6

//DS18B20 Sensors connected to pin
OneWire oneWire(3);

// all library classes are placed in the namespace 'as'
using namespace as;

// define all device properties
const struct DeviceInfo PROGMEM devinfo = {
  {0xf3, 0x01, 0x01},          // Device ID
  "UNITEMP001",               // Device Serial
  {0xF3, 0x01},              // Device Model
  0x10,                       // Firmware Version
  as::DeviceType::THSensor,   // Device Type
  {0x01, 0x01}               // Info Bytes
};

/**
   Configure the used hardware
*/
typedef AvrSPI<10, 11, 12, 13> SPIType;
typedef Radio<SPIType, 2> RadioType;
typedef StatusLed<LED_PIN> LedType;
typedef AskSin<LedType, BatterySensor, RadioType> BaseHal;
class Hal : public BaseHal {
  public:
    void init (const HMID& id) {
      BaseHal::init(id);

      battery.init(seconds2ticks(60UL * 60), sysclock); //battery measure once an hour
      battery.low(22);
      battery.critical(18);
    }

    bool runready () {
      return sysclock.runready() || BaseHal::runready();
    }
} hal;


DEFREGISTER(UReg0, MASTERID_REGS, DREG_LOWBATLIMIT, 0x21, 0x22)
class UList0 : public RegList0<UReg0> {
  public:
    UList0 (uint16_t addr) : RegList0<UReg0>(addr) {}
    bool Sendeintervall (uint16_t value) const {
      return this->writeRegister(0x21, (value >> 8) & 0xff) && this->writeRegister(0x22, value & 0xff);
    }
    uint16_t Sendeintervall () const {
      return (this->readRegister(0x21, 0) << 8) + this->readRegister(0x22, 0);
    }
    void defaults () {
      clear();
      lowBatLimit(22);
      Sendeintervall(180);
    }
};

DEFREGISTER(UReg1, 0x01, 0x02, 0x03, 0x04)
class UList1 : public RegList1<UReg1> {
  public:
    UList1 (uint16_t addr) : RegList1<UReg1>(addr) {}

    bool Offset (int32_t value) const {
      return
          this->writeRegister(0x01, (value >> 24) & 0xff) &&
          this->writeRegister(0x02, (value >> 16) & 0xff) &&
          this->writeRegister(0x03, (value >> 8) & 0xff) &&
          this->writeRegister(0x04, (value) & 0xff)
          ;
    }

    int32_t Offset () const {
      return
          ((int32_t)(this->readRegister(0x01, 0)) << 24) +
          ((int32_t)(this->readRegister(0x02, 0)) << 16) +
          ((int32_t)(this->readRegister(0x03, 0)) << 8) +
          ((int32_t)(this->readRegister(0x04, 0)))
          ;
    }
    void defaults () {
      clear();
      Offset(0);
    }
};

int32_t Offsets[MAX_SENSORS];


class WeatherEventMsg : public Message {
  public:
    void init(uint8_t msgcnt, Ds18b20* sensors, bool batlow, uint8_t channelFieldOffset) {
      Message::init(0x16, msgcnt, 0x53, (msgcnt % 20 == 1) ? (BIDI | WKMEUP) : BCAST, batlow ? 0x80 : 0x00, 0x41 + channelFieldOffset);
      int16_t t0 = sensors[0 + channelFieldOffset].temperature() + Offsets[0 + channelFieldOffset];
      int16_t t1 = sensors[1 + channelFieldOffset].temperature() + Offsets[1 + channelFieldOffset];
      int16_t t2 = sensors[2 + channelFieldOffset].temperature() + Offsets[2 + channelFieldOffset];
      int16_t t3 = sensors[3 + channelFieldOffset].temperature() + Offsets[3 + channelFieldOffset];

      pload[0] = (t0 >> 8) & 0xff;
      pload[1] = (t0) & 0xff;
      pload[2] = 0x42 + channelFieldOffset;
      pload[3] = (t1 >> 8) & 0xff;
      pload[4] = (t1) & 0xff;
      pload[5] = 0x43 + channelFieldOffset;
      pload[6] = (t2 >> 8) & 0xff;
      pload[7] = (t2) & 0xff;
      pload[8] = 0x44 + channelFieldOffset;
      pload[9] = (t3 >> 8) & 0xff;
      pload[10] = (t3) & 0xff;
    }
};

class WeatherChannel : public Channel<Hal, UList1, EmptyList, List4, PEERS_PER_CHANNEL, UList0> {
  public:
    WeatherChannel () : Channel() {}
    virtual ~WeatherChannel () {}

    void configChanged() {
      //DPRINT(F("(")); DDEC(number()); DPRINTLN(F(") Config changed List1"));
      DPRINT(F("OFFSET: ")); DDECLN(this->getList1().Offset());
      Offsets[number() - 1] = this->getList1().Offset();
    }

    uint8_t status () const {
      return 0;
    }

    uint8_t flags () const {
      return 0;
    }
};

class UType : public MultiChannelDevice<Hal, WeatherChannel, MAX_SENSORS, UList0> {

    class SensorArray : public Alarm {
        UType& dev;

      public:
        uint8_t       sensorcount;
        Ds18b20       sensors[MAX_SENSORS];
        SensorArray (UType& d) : Alarm(0), dev(d), sensorcount(0) {}

        virtual void trigger (__attribute__ ((unused)) AlarmClock& clock) {
          tick = delay();
          sysclock.add(*this);

          Ds18b20::measure(sensors, sensorcount);
          DPRINT(F("Temperaturen: | "));
          for (int i = 0; i < MAX_SENSORS; i++) {
            DDEC(sensors[i].temperature()); DPRINT(" | ");
#ifdef USE_LCD
            uint8_t x = (i % 2 == 0 ? 0 : 10);
            uint8_t y = i / 2;
            lcd.setCursor(x, y);

            String s_temp = " --.-";
            if ((i + 1) <= sensorcount) {
              s_temp = (String)((float)sensors[i].temperature() / 10.0);
              s_temp = s_temp.substring(0, s_temp.length() - 1);
              if (sensors[i].temperature() < 1000 && sensors[i].temperature() >= 0) s_temp = " " + s_temp;
            }
            String disp_temp = String(i + 1) + ":" + s_temp + (char)223 + "C ";

            lcd.print(disp_temp);
#endif
          }
          DPRINTLN("");
          WeatherEventMsg& msg = (WeatherEventMsg&)dev.message();
          //Aufteilung in 2 Messages, da sonst die max. BidCos Message Size (0x1a)? überschritten wird
          msg.init(dev.nextcount(), sensors, dev.battery().low(), 0);
          dev.send(msg, dev.getMasterID());
#if MAX_SENSORS > 4
          _delay_ms(250);
          msg.init(dev.nextcount(), sensors, dev.battery().low(), 4);
          dev.send(msg, dev.getMasterID());
#endif
        }

        uint32_t delay () {
          uint16_t _txMindelay = 180;
          _txMindelay = dev.getList0().Sendeintervall();
          if (_txMindelay == 0) _txMindelay = 180;
          return seconds2ticks(_txMindelay);
        }

    } sensarray;

  public:
    typedef MultiChannelDevice<Hal, WeatherChannel, MAX_SENSORS, UList0> TSDevice;
    UType(const DeviceInfo& info, uint16_t addr) : TSDevice(info, addr), sensarray(*this) {}
    virtual ~UType () {}

    virtual void configChanged () {
      TSDevice::configChanged();
      DPRINTLN("Config Changed List0");
      DPRINT("LOW BAT Limit: ");
      DDECLN(this->getList0().lowBatLimit());
      this->battery().low(this->getList0().lowBatLimit());
      DPRINT("Sendeintervall: "); DDECLN(this->getList0().Sendeintervall());
    }

    void init (Hal& hal) {
      TSDevice::init(hal);
      sensarray.sensorcount = Ds18b20::init(oneWire, sensarray.sensors, MAX_SENSORS);
      DPRINT("Found "); DDEC(sensarray.sensorcount); DPRINTLN(" DS18B20 Sensors");
#ifdef USE_LCD
      lcd.setCursor(2, 3);
      lcd.print("Found Sensors: " + String(sensarray.sensorcount));
#endif
      sensarray.set(seconds2ticks(5));
      sysclock.add(sensarray);
    }
};

UType sdev(devinfo, 0x20);
ConfigButton<UType> cfgBtn(sdev);

void setup () {
  DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);
  memset(Offsets, 0, MAX_SENSORS);
  DDEVINFO(sdev);

#ifdef USE_LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("UNI-Sen-TEMP-DS18B20");
  lcd.setCursor(5, 1);
  lcd.print((char*)serial);
  HMID temp;
  sdev.getDeviceID(temp);
  lcd.setCursor(7, 2);
  lcd.print(temp, HEX);
#endif

  sdev.init(hal);
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
  sdev.initDone();
}

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  if ( worked == false && poll == false ) {
    if ( hal.battery.critical() ) {
      hal.activity.sleepForever(hal);
    }
    hal.activity.savePower<Sleep<>>(hal);
  }
}

