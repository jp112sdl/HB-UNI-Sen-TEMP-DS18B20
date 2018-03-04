#ifndef __SENSORS_DS18B20_h__
#define __SENSORS_DS18B20_h__

#include <Sensors.h>
#include <OneWire.h>
//https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <DallasTemperature.h>

#define PRECISION 9 //DS18B20 Resolution 9...12 bit

namespace as {

template <int DATAPIN>
class Ds18b20 : public Temperature {
    OneWire oneWire = OneWire(DATAPIN);
    DallasTemperature _ds18b20 = DallasTemperature(&oneWire);
    DeviceAddress _sensors[8];
    uint8_t _sensor_cnt = 0;
    int _i;

  public:
    Ds18b20 () : _ds18b20(&oneWire) {}

    int sencount() {
      return _sensor_cnt;
    }

    void init () {
      _ds18b20.begin();
      _sensor_cnt = _ds18b20.getDeviceCount();
      for (int i = 0; i < _sensor_cnt; i++) {
        _ds18b20.getAddress(_sensors[i], i);
        _ds18b20.setResolution(_sensors[i], PRECISION);
      }
      _present = true;
    }
    void measure (uint8_t ds_num, __attribute__((unused)) bool async = false) {
      if ( present() == true ) {
        _i = 0;
        do {
          if ( _i != 0 ) {
            delay(200);
            DPRINT("DS18B20 Try # "); DDECLN(_i);
          }
          _ds18b20.requestTemperatures();
          _temperature = _ds18b20.getTempC(_sensors[ds_num]) * 10;
          _i = _i + 1;
        } while ((_temperature == 0) && _i <= 10);
      }
    }
};

}

#endif
