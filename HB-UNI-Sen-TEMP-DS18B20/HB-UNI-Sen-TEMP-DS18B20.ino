//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER

#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>
#include <AskSinPP.h>
#include <LowPower.h>

#include <Register.h>
#include <MultiChannelDevice.h>

#include <OneWire.h>
#include <sensors/Ds18b20.h>
#define MAX_SENSORS       8

// Arduino Pro mini 8 Mhz
// Arduino pin for the config button
#define CONFIG_BUTTON_PIN 8

// number of available peers per channel
#define PEERS_PER_CHANNEL 6

//DS18B20 Sensors connected to pin
OneWire oneWire(3);

// all library classes are placed in the namespace 'as'
using namespace as;

// define all device properties
const struct DeviceInfo PROGMEM devinfo = {
  {0x11, 0x12, 0x13},       	 // Device ID
  "UNITEMP001",           	  // Device Serial
  {0xF3, 0x01},            	 // Device Model
  0x10,                   	  // Firmware Version
  as::DeviceType::THSensor, 	// Device Type
  {0x01, 0x01}             	 // Info Bytes
};

/**
   Configure the used hardware
*/
typedef AvrSPI<10, 11, 12, 13> SPIType;
typedef Radio<SPIType, 2> RadioType;
typedef StatusLed<4> LedType;
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


DEFREGISTER(UReg0, MASTERID_REGS, DREG_BURSTRX, DREG_LOWBATLIMIT)
class UList0 : public RegList0<UReg0> {
  public:
    UList0 (uint16_t addr) : RegList0<UReg0>(addr) {}
    void defaults () {
      clear();
      burstRx(false);
      lowBatLimit(22);
    }
};

DEFREGISTER(UReg1, CREG_TX_MINDELAY)
class UList1 : public RegList1<UReg1> {
  public:
    UList1 (uint16_t addr) : RegList1<UReg1>(addr) {}
    void defaults () {
      clear();
      txMindelay(18);
    }
};

class WeatherEventMsg : public Message {
  public:
    void init(uint8_t msgcnt, Ds18b20 *sensors, bool batlow) {

      DPRINT("batlow = ");
      DDECLN(batlow);
      DPRINT("Temperatures: ");
      for (int i = 0; i < MAX_SENSORS; i++) {
        DDEC(sensors[i].temperature());
        DPRINT(";");
      } DPRINTLN("");


      uint8_t t1 = (sensors[0].temperature() >> 8) & 0x7f;
      uint8_t t2 = sensors[0].temperature() & 0xff;
      if ( batlow == true ) {
        t1 |= 0x80; // set bat low bit
      }
      Message::init(0x19, msgcnt, 0x70, BCAST, t1, t2);

      for (int i = 0; i < ((MAX_SENSORS * 2) - 2); i++) {
        pload[i] = (sensors[(i / 2) + 1].temperature() >> 8) & 0x7f;
        pload[i + 1] = sensors[(i / 2) + 1].temperature() & 0xff;
        i++;
      }
    }
};

class WeatherChannel : public Channel<Hal, UList1, EmptyList, List4, PEERS_PER_CHANNEL, UList0>, public Alarm {

    WeatherEventMsg msg;

    Ds18b20       sensors[MAX_SENSORS];
    uint8_t       sensorcount;
    uint16_t      millis;

  public:
    WeatherChannel () : Channel(), Alarm(5), millis(0) {}
    virtual ~WeatherChannel () {}

    virtual void trigger (__attribute__ ((unused)) AlarmClock& clock) {
      uint8_t msgcnt = device().nextcount();
      // reactivate for next measure
      tick = delay();
      sysclock.add(*this);

      Ds18b20::measure(sensors, sensorcount);

      msg.init(msgcnt, sensors, device().battery().low());

      device().sendPeerEvent(msg, *this);
    }

    uint32_t delay () {
      uint8_t _txMindelay = 18; //tenth of seconds (18 = 180)
      _txMindelay = this->getList1().txMindelay();
      if (_txMindelay == 0) _txMindelay = 18;
      return seconds2ticks(_txMindelay * 10);
    }

    void configChanged() {
      DPRINTLN("Config changed List1");
      DPRINT("TX Delay = ");
      DDECLN(this->getList1().txMindelay());
    }

    void setup(Device<Hal, UList0>* dev, uint8_t number, uint16_t addr) {
      Channel::setup(dev, number, addr);
      sysclock.add(*this);
      sensorcount = Ds18b20::init(oneWire, sensors, 10);
      DPRINT("Found "); DDEC(sensorcount); DPRINTLN(" DS18B20 Sensors");
    }

    uint8_t status () const {
      return 0;
    }

    uint8_t flags () const {
      return 0;
    }
};

class UType : public MultiChannelDevice<Hal, WeatherChannel, 1, UList0> {
  public:
    typedef MultiChannelDevice<Hal, WeatherChannel, 1, UList0> TSDevice;
    UType(const DeviceInfo& info, uint16_t addr) : TSDevice(info, addr) {}
    virtual ~UType () {}

    virtual void configChanged () {
      TSDevice::configChanged();
      DPRINTLN("Config Changed List0");
      DPRINT("LOW BAT Limit: ");
      DDECLN(this->getList0().lowBatLimit());
      DPRINT("Wake-On-Radio: ");
      DDECLN(this->getList0().burstRx());
      this->battery().low(this->getList0().lowBatLimit());
    }
};

UType sdev(devinfo, 0x20);
ConfigButton<UType> cfgBtn(sdev);

void setup () {
  DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);
  sdev.init(hal);
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
  sdev.initDone();
}

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  if ( worked == false && poll == false ) {
    hal.activity.savePower<Sleep<>>(hal);
  }
}
