//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2018-03-24 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2023-10-11 SchmidHo extended: - Configured Channel temperature offsets used also for values in LCD display now
//                               - Additional Button BTN_BACKLIGHT_PIN (if define WITH_BACKLIGHT) to
//                                 - switch off/on LCD backlight (short press)
//                                 - display OneWire serial Numbers (Addresses) of the sensors (long press)
//                               - if less than 4 sensors and LCD: Display additional to actual also two elder values from each channel
//                               - OneWire-Serialnumbers of sensors can be compiled in for defines order of Sensors
//                               - for not available Sensors -150.0 instead of 0.0 is send to CCU
//                               - trimmed for less program memory usage
//                               - Many comments (in German language) added
//- -----------------------------------------------------------------------------------------------------------------------

// Kommentare übernommen von https://github.com/netprog2019/Homematic-HomeBrew-Komponenten/tree/main/HB-MR-MOIST-SENS-SW2

// Der Sketch verwendet 28850 ...  Bytes (93% ... 94%) des Programmspeicherplatzes. Das Maximum sind 30720 Bytes.
// Globale Variablen verwenden 1488 Bytes (72%) des dynamischen Speichers. Das Maximum sind 2048 Bytes.


const uint64_t SENSOR_ORDER[] = {}; // Leer: Alle Sensoren in der gefundenen Reihenfolge den Kanälen zuordnen
// const uint64_t SENSOR_ORDER[] = {0xABCf52443b75128, 0x1c4ed678434a8b28}; //Option: Sensoren mit diesen Seriennummern als Kanal 1, 2, ... verwenden!
// bis zu 8 Addressen (aus seriellen Monitor oder LCD-Display übernehmen und eintragen)  
// geg. gefundene, weitere Sensoren werden in der gefundenen Reihenfolge den nächsten Kanälen zugeordnet

#define FMVERS 1.1 // up to 25.5, multiplied with 10 inserted to struct DeviceInfo PROGMEM devinfo
#define SKETCH_NAME "UNI-Sen-TEMP-DS18B20"
// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER

#define SENSOR_ONLY //about 600 Bytes less code
#define NORTC // about 60 Bytes less code

#define OFFSET_PARAMETER_SIZE 4 // gemäß hb-uni-sen-temp.xml und JP-HB-Devices-addon, für -5.0 .. + 5.0 (als -50 ... +50 übertragen) würde 1 Byte reichen!

// Wake On Radio (WoR):
// #define USE_WOR // activate if operated from battery to save power (without always receiving packets)
#define HIDE_IGNORE_MSG // without USE_WOR the serial monitor shows messages for other devices, which are suppressed by this

#define USE_LCD // Operation with an 20x4 LCD display
#define WITH_BACKLIGHT // use an additional button to switch off/on LCD backlight and to display OneWire serial numbers
#define LCD_ADDRESS 0x27 //0x27 = alle 3 Brücken offen

#define DEFAULT_SENDE_INTERVALL_s 60
#define ELDER_N 10 // bei <=3 Sensoren, älter Werte von vor 1 .. 2 * (ELDER_N *  dev.getList0().Sendeintervall()) Sekunden im LCD anzeigen

//Korrekturfaktor der Clock-Ungenauigkeit, wenn keine RTC verwendet wird
#define SYSCLOCK_FACTOR    0.98 // Ausmessen und anpassen
// #define SYSCLOCK_FACTOR    1.00 //ohne Korrektur

#define EI_NOTEXTERNAL
// in Message.h wird MaxDataLen (für uint8_t pload[MaxDataLen]) zum Senden auf 17 gesetzt, falls noch nicht definiert. 
// für empfangene Meldungen gilt eine Maximallänge von 60!
#include <EnableInterrupt.h>
#include <AskSinPP.h>
#include <LowPower.h>
#include <Register.h>
#include <MultiChannelDevice.h>

#ifdef USE_LCD
  //use this LCD lib: https://github.com/marcoschwartz/LiquidCrystal_I2C
  // Mit Poti Kontrast einstellen!
  #include <LiquidCrystal_I2C.h>
  LiquidCrystal_I2C lcd(LCD_ADDRESS, 20, 4); // 4 Zeilen a 20 Zeichen
#endif

#include <OneWire.h>
#include <sensors/Ds18b20.h>
#define MAX_SENSORS       8

// Arduino Pro mini 8 Mhz
#define ONE_WIRE1_PIN 3
#define CONFIG_BUTTON_PIN 8 // Arduino pin for the System config button
#define LED_PIN           4
#ifdef WITH_BACKLIGHT
  #define BTN_BACKLIGHT_PIN 7 // Button für "Display on/off" und Anzeige der OneWire-SerialNumbern
#endif

// Arduino pin für das c1101 Funkmodul
#define CC1101_GDO0_PIN        2
#define CC1101_CS_PIN          10
#define CC1101_MOSI_PIN        11
#define CC1101_MISO_PIN        12
#define CC1101_SCK_PIN         13

// number of available peers per channel
#define PEERS_PER_CHANNEL 6

//DS18B20 Sensors connected to pin
OneWire oneWire(ONE_WIRE1_PIN);

// all library classes are placed in the namespace 'as'
using namespace as;

// define all device properties:
const struct DeviceInfo PROGMEM devinfo = {
  {0xf3, 0x01, 0x01},         // Device ID   (=Adress, unique! 0xF30101 = 15925505)
  "UNITEMP001",               // Device Serial (unique! 10 Characters!)
  {0xF3, 0x01},               // Device Model
  (uint8_t)(10*FMVERS),       // Firmware Version, one Byte!, needs to be defined before including this!
  as::DeviceType::THSensor,   // Device Type
  {0x01, 0x01}                // Info Bytes
};

/* Configure the used hardware */
typedef AvrSPI<CC1101_CS_PIN, CC1101_MOSI_PIN, CC1101_MISO_PIN, CC1101_SCK_PIN> SPIType;
typedef Radio<SPIType, CC1101_GDO0_PIN> RadioType;
typedef StatusLed<LED_PIN> LedType;
typedef AskSin<LedType, BatterySensor, RadioType> BaseHal;
//                      NoBattery

class Hal : public BaseHal {
  public:
    void init (const HMID& id) {
      BaseHal::init(id);
      battery.init(seconds2ticks(60UL * 60), sysclock); //battery measure once an hour
      battery.low(22); //2.2V  Spannungsteiler an A0 erforderlich???? 
      battery.critical(18); //1.8V
      }

    bool runready () {
      return sysclock.runready() || BaseHal::runready();
      }
  } hal;


// =====================================================================
// ==      Gerätebezogene Register  MASTERID_REGS                     ==
// ==  Einmal je HomeMatic-Gerät, in List0 gespeichert                ==
// == Optional z.B:                                                   ==
// == - DREG_LOWBATLIMIT                                              ==
// == - DREG_SABOTAGEMSG (Sabotagemeldung ja/nein)                    ==
// == - DREG_TRANSMITTRYMAX (max. Sendeversuche 1-10)                 ==
// == - die "freien" Register 0x21/22 werden hier als 16bit memory    ==
// ==   für das Sende-Intervall in 1-3600 Sek. verwendet              ==
// ==   *siehe <parameter id="Sendeintervall">                        ==
// =====================================================================
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
      
    void defaults () { // wird nicht aufgerufen
      clear();
      lowBatLimit(22);
      Sendeintervall(DEFAULT_SENDE_INTERVALL_s);
      // sabotageMsg(false);      
      // transmitDevTryMax(6);       
      // DPRINTLN("MasterDefaults setup!");
      }
  }; // class UList0 : public RegList0<UReg0>

// =====================================================================
// ==                    Kanalbezogene Register                       ==
// ==  Für z.B. jeden weiteren Kanal n, in List1 gespeichert          ==
// == Read/Write als 8bit-Wert, d.h. 32bit ist in 4 Werte zu spalten  ==
// == Es wird abweichend von Intel Prozessoren BigEndian verwendet!   ==
// == - CREG_EVENTFILTER 0x01                                         ==
// == - CREG_INTERVAL 0x02                                            ==
// == - 0x03, 0x04 Temperatur-Offsetwerte für Fehler jeden Sensorss   ==
// == Optional weitere Register, z.B.                                 ==
// == - Register 0x23/24 und 0x25/26 jeweils als 16bit für Bodenfeute ==
// ==   *siehe <parameter id="CAP_MOIST_HIGH_VALUE">                  ==
// ==   *siehe <parameter id="CAP_MOIST_LOW_VALUE">                   ==
// =====================================================================
#if OFFSET_PARAMETER_SIZE == 4 // Kompatibel zum JP-HB-Devices-addon !
DEFREGISTER(UReg1, 0x01, 0x02, 0x03, 0x04)  // Je Kanal ein 32-bit-Register für einen 32bit-Offset-Wert -5.0 ... +5.0 als -50 ... +50)
#else // vorbereitet für anderes Add-On
DEFREGISTER(UReg1, 0x01)  // Je Kanal ein 8-bit-Register für einen 8bit-Offset-Wert -5.0 ... +5.0 als -50 ... +50)
#endif
class UList1 : public RegList1<UReg1> {
  public:
    UList1 (uint16_t addr) : RegList1<UReg1>(addr) {}

    int8_t Offset () const {
      // Solange gemäß hb-uni-sen-temp.xml der Parameter mit 4 Byte (d.h. als int32_t) übertragen wird
      // und BigEndian-Speicherung verwendet wird benötigen wir das 4. Byte (OFFSET_PARAMETER_SIZE = 4)! 
      return (this->readRegister(OFFSET_PARAMETER_SIZE, /*default=*/0));
      }

   /* nicht benötigt
    bool Offset (int8_t value) const { //set Offsets
      DPRINT("Offset set to ");DDECLN(value);
      return this->writeRegister(OFFSET_PARAMETER_SIZE, value);
      }
   */
      
    void defaults () { // wird nicht aufgerufen, spart aber seltsamer Weise Programmspeicherplatz
      clear();
      // Offset(0); // Defaultwert des Offsets je Sensor setzen
      // DPRINTLN("RegDefaults set to zero!");
      }

  }; //class UList1 : public RegList1<UReg1>

int8_t Offsets[MAX_SENSORS]; // Temperatur-Korrektur-Werte gemäß Kanalparamter in CCU
int8_t sensCnt; // actual connected sensor count, ev. höher falls beim sortieren Lücken entstehen
int16_t dispIntervall_s; // bei 1 .. 3 Sensoren werden zusätzlich zwei ältere Werte in diesem Zeitabstand angezeigt
int8_t senSeq[MAX_SENSORS]; // Umsortierung gemäß Vorgabe durch SENSOR_ORDER[]

class WeatherEventMsg : public Message {
  public:
    
    void init(uint8_t msgcnt, Ds18b20* sensors, bool batlow, uint8_t channelFieldOffset) {

      Message::init(/*length=*/0x16, msgcnt, /*typ=*/AS_MESSAGE_SENSOR_DATA, /*flags=*/(msgcnt % 20 == 1) ? (BIDI | WKMEUP) : BCAST, batlow ? 0x80 : 0x00, 0x41 + channelFieldOffset);
      // 0x53 = AS_MESSAGE_SENSOR_DATA
      // als Standard wird BCAST gesendet um Energie zu sparen.
      // Bei jeder 20. Nachricht senden wir stattdessen BIDI|WKMEUP, um eventuell anstehende
      // Konfigurationsänderungen auch ohne Betätigung des Anlerntasters übernehmen zu können
      // (mit Verzögerung, worst-case 10x Sendeintervall).
      
      // für 4 Sensoren die Meldungsdaten zusammenbauen:
      // für ersten der 4 Kanäle wurde der 1. Wert schon mit ..init(...)-Parameter gesetzt
      for (int j = 0; j < 4; j++) {
        int8_t k = j + channelFieldOffset;
        int16_t t = -1500; // -150.0°C für fehlenden Sensor als klare Unterscheidung zu Messwert 0.0 °C
        if (senSeq[k] > -1) {
          t = sensors[senSeq[k]].temperature() + Offsets[k];
          }
        if (j>0) {
          pload[-1 + 3*j] = 0x41 + j + channelFieldOffset;
          }
        pload[0 + 3*j] = (t >> 8) & 0xff;
        pload[1 + 3*j] = (t) & 0xff;
        }
      } //init(...)
  }; //class WeatherEventMsg : public Message

#ifdef USE_LCD
  int8_t page = -1; // switched by LongPress of BTN_BACKLIGHT: -1: Temperaturen, 0: SerialNummern 1..4, 1: SerialNummern 5..8

  void headline() { // bei 1 .. 3 Sensoren das Alter der Werte in der 2. und 3. Spalte als Überschrift anzeigen
    // z.B. "°C akt  -10  -20min"
    if ((sensCnt>0) && (sensCnt < 4)) {
      lcd.setCursor(0, 0);
      String unit="s";
      int16_t dispIntervall=dispIntervall_s;
      if (dispIntervall > 3600*7) {
        dispIntervall = dispIntervall / 3600;
        unit = "h";
        }
      else if (dispIntervall_s > 240) {
        dispIntervall = dispIntervall / 60;
        unit = "min";
        }
      lcd.print((  String((char)223) + "C akt  -" + String(dispIntervall) + "  -" + String(2 * dispIntervall) + unit + "                  ").substring(0,20));
      }
    else {
      ; //DPRINT(F("headline(): sensCnt=")); DDECLN(sensCnt);
      }  
    }  
#endif // USE_LCD

class WeatherChannel : public Channel<Hal, UList1, EmptyList, List4, PEERS_PER_CHANNEL, UList0> {
  public:
    WeatherChannel () : Channel() {}
    virtual ~WeatherChannel () {}

    void configChanged() {
      //DDEC(number());DPRINT(F(" OFFSET new ")); DDECLN(this->getList1().Offset());
      Offsets[number() - 1] = this->getList1().Offset();
      // headline();
      }

    uint8_t status () const {
      return 0;
      }

    uint8_t flags () const {
      return 0;
      }
  }; // class WeatherChannel


uint64_t addrSN[MAX_SENSORS]; // OneWireAddress = Serial Number


String formatSN(uint64_t val) { // SerienNummer (incl. CRC) formatieren
  // String(...) mit uint64_t ist nicht definiert! Geg. führende 0 wird nicht ausgeg.
  uint32_t x[2];
  memcpy(x, &val, 8);
  String s="";
  for (int j = 1; j >= 0; j--) {
    if (x[j] < 0x10000000) s = s + String("0"); // geg. führende Null ergänzen!
    s = s + String(x[j], HEX);
    }
  return s;
  }


class UType : public MultiChannelDevice<Hal, WeatherChannel, MAX_SENSORS, UList0> {

  class SensorArray : public Alarm {
    UType& dev;
#ifdef USE_LCD
    private:
      int16_t oldTem[3][3] = {{-32767,-32767,-32767},{-32767,-32767,-32767},{-32767,-32767,-32767}};
      // z.B. dispIntervall_s = 60 und ELDER_N = 10
      // oldTem[0] aus aktuellen Wert gesetzt, 0 ... 9 min alt, nicht angezeigt
      // oldTem[1] 10 ... 19 min alt
      // oldTem[2] 20 ... 29 min alt
      int8_t oldCnt = 0;

      String formatTemp(int16_t val) { // Einzelne Temperatur formatieren
        if (val <= -1500) { // -150.0 °C
          return " --.-";  // bisher noch kein älterer Messwert verfügbar
          }
        String s_temp = (String)((float)val / 10.0);
        s_temp = s_temp.substring(0, s_temp.length() - 1); //auf eine Nachkommastelle begrenzen
        if ((val < 1000) && (val >= 0)) {
          s_temp = " " + s_temp; // < 100°C und positiv: Leerzeichen voranstellen
          }
        return s_temp;
        }

      String formatTempLine(int8_t i, int8_t upd) { // Temperatur eines Sensors (aus sensors[i].temperature()) und geg. die 2 alten Werte  formatieren
        // bei upd==true (und <= 3 Sensoren): Shift der (alten) Werte
        int16_t tempWithOffset = -1500;
        if  (senSeq[i] > -1) { // Umsortierung war o.k für diese OneWire-SerienNummer
          tempWithOffset = sensors[senSeq[i]].temperature() + Offsets[i];
          }
        String s_temp = formatTemp( tempWithOffset );
        String disp_temp = String(i + 1) + ":" + s_temp;
        if (sensCnt < 4) { // nur <= 3 Sensoren: alte Werte hinzufügen und zwischenspeichern:
          if (upd) { // falls außer der Reihe aufgerufen Shift geg. überspringen!
            if (oldCnt == 0) { // z.B. bei update alle 60s mit z.B. ELDER_N=10: Alte Werte von vor 10 und vor 20 Minuten
              oldTem[2][i]=oldTem[1][i];
              oldTem[1][i]=oldTem[0][i];
              oldTem[0][i]=tempWithOffset;            
              }
            if (i == sensCnt-1) { // beim letzten Sensor den counter für die im Display auszulassenden aktualisieren bzw reseten:
              oldCnt +=1;
              if (oldCnt >= ELDER_N) {
                oldCnt = 0;
                // DPRINTLN("OldValShift done");
                }
              }
            // DPRINT("OldCnt = ");DDEC(oldCnt);DPRINT(", i = ");DDEC(i);DPRINT(", sensCnt = ");DDECLN(sensCnt);
            } // if upd
          disp_temp = disp_temp + " " + formatTemp(oldTem[1][i]) + " " + formatTemp(oldTem[2][i]);                          
          }
        else {
          disp_temp = disp_temp + (char)223 + "C ";
          }
        return disp_temp;
        } // formatTempLine(int8_t i)
#endif // USE_LCD

    public:
      uint8_t       sensorcount; // physical Count of found sensors
      Ds18b20       sensors[MAX_SENSORS];
      SensorArray (UType& d) : Alarm(0), dev(d), sensorcount(0) {}

#ifdef USE_LCD
      // Temperaturen ind LCD-Display übertragen:
      void dispTemps(int8_t upd) {
        // falls außer der Reihe (zum Display-Refresh) aufgerufen sollte upd==false sein
        //DPRINT("dispTemps oldTem[0][0]=");DDEC(oldTem[0][0]);DPRINT(", Page=");DDECLN(page);
        if (oldTem[0][0] == -32767) { // allererster Aufruf: Display löschen
          lcd.clear();
          headline();
          oldTem[0][0] = -32766;
          }
        for (int i = 0; i < sensCnt; i++) {
          uint8_t x = (i % 2 == 0 ? 0 : 10);  // ungerade: Spalte 0, gerade: Spalte 10
          uint8_t y = i / 2; // Zeile bei zweispaltig
          if (sensCnt < 5) {  // einspaltig
            x = 0;
            y = i;
            if (sensCnt < 4) {
              y = i+1; // wegen Überschrift eine Zeile weiter unten
              }
            } 
          if (page == -1) { // geg. solange OneWire-SerialNumbern angezeigt werden Ausgabe überspringen
            lcd.setCursor(x, y);
            lcd.print(formatTempLine(i, upd));
            }            
          } // for i
        } // dispTemps()
#endif


      virtual void trigger (__attribute__ ((unused)) AlarmClock& clock) {
        sysclock.add(*this, getMeasDelay_ms()); // nächsten Trigger initialisieren

        Ds18b20::measure(sensors, sensorcount);
        DPRINT(F("Temperaturen *10 °C: "));
        for (int i = 0; i < MAX_SENSORS; i++) {
          if (senSeq[i]>=0) {
            DDEC(sensors[senSeq[i]].temperature()); // mit Sortierung und ohne Offset-Korrektur
            }
          DPRINT(" | ");  
          } // for i
        DPRINTLN("");
#ifdef USE_LCD
        dispTemps(/*upd=*/true); // alte Temperatur-Werte ausgeben und bei TRUE: Bei 1..3 Sensoren die alte Temperatur-Werte schieben
#endif
        // Werte an CCU senden:
        WeatherEventMsg& msg = (WeatherEventMsg&)dev.message();
        // Aufteilung in 2 Messages, da sonst die max. BidCos Message Size (0x1a)? überschritten wird
        // Seit CCU-Version ??? liegt das Message-Size-Limit aqber höher!
        msg.init(dev.nextcount(), sensors, dev.battery().low(), /*offset=*/0);
        dev.send(msg, dev.getMasterID());
#if MAX_SENSORS > 4
        _delay_ms(250);
        msg.init(dev.nextcount(), sensors, dev.battery().low(), 4);
        dev.send(msg, dev.getMasterID());
#endif
        } // trigger(...) 

      uint32_t getMeasDelay_ms () {
        uint16_t txMindelay_s = DEFAULT_SENDE_INTERVALL_s;
        txMindelay_s = max(dev.getList0().Sendeintervall(), 10);        
        return (uint32_t)(SYSCLOCK_FACTOR * 1000UL * txMindelay_s);
        }

    }; // class SensorArray

  public:
  SensorArray sensarray;

  public:
    typedef MultiChannelDevice<Hal, WeatherChannel, /*channelCount=*/MAX_SENSORS, UList0> TSDevice;
    UType(const DeviceInfo& info, uint16_t addr) : TSDevice(info, addr), sensarray(*this) {}

    virtual ~UType () {}

    virtual void configChanged () {
      TSDevice::configChanged();
      // Empfangene Daten holen:
      uint16_t lb = this->getList0().lowBatLimit();
      uint16_t si = this->getList0().Sendeintervall();
      DPRINT("Cfg Chg, LOW BAT Limit: "); DDEC(lb);
      // und speichern ...
      this->battery().low(lb);
      dispIntervall_s = ELDER_N * si;      
      DPRINT(" Intervall: "); DDECLN(si);
      } //configChanged


    // Wenn OneWire-Seriennummern vorgegeben sind, dann die Sensorreihenfolge umsortieren
    void sensorSequence(void) {
      uint8_t done[MAX_SENSORS];
      memset (done, 0, sizeof(done)); // Initialisierung
      memset (senSeq, 255, sizeof(senSeq)); // (unsigned char)255 ist -1 bei int8_t ! 
      int8_t remainingCnt = sensCnt;

      // Sensoren mit eingetragener Seriennummer bearbeiten:
      uint8_t j = 0;
      while (j < sizeof(SENSOR_ORDER)/sizeof(SENSOR_ORDER[0])) {
        for (uint8_t i = 0; i < sensCnt; i++) {
          if (SENSOR_ORDER[j] == addrSN[i]) {
            //DPRINT("Sensor i=");DDEC(i+1);DPRINT(" found with j=");DDEC(j+1);DPRINTLN(" as " + formatSN(addrSN[i]));
            senSeq[j]=i;
            done[i]=1;
            remainingCnt--;
            break;
            } // if
          } // for i
        j++;  
        } // while j
      
      // noch nicht festgelegt Sensoren verteilen:
      uint8_t i=0;
      uint8_t sc=0;
      while ((i<sensCnt) && (j<MAX_SENSORS)) {
        while (done[i] != 0) {
          i++;
          if (i >= sensCnt) break;
          }
        while (senSeq[j] != -1) {
          j++;
          if (j >= MAX_SENSORS) break;
          }
        if ((j < MAX_SENSORS) && (i < sensCnt)) {
          senSeq[j]=i;
          sc = j+1;
          done[i]=1;
          //DPRINT("Sensor " + formatSN(addrSN[i])+ " ");DDEC(i+1);DPRINT(" added with j=");DDECLN(j+1);
          remainingCnt--;
          i++;
          }
        }
      sensCnt=sc; // falls Lücken vorhanden ev höher als physikalische Sensor-Anzahl
      DPRINT("New Order ");  
      for (uint8_t j = 0; j < MAX_SENSORS; j++) {
        DDEC(j+1);DPRINT(":");DDEC(1+senSeq[j]);DPRINT("; ");        
        }
      DPRINTLN("");
      if (remainingCnt>0) {
#ifdef USE_LCD
        lcd.setCursor(0, 0);
        lcd.print("Sensor Ordering Err!");
        lcd.setCursor(0, 1);
        lcd.print("SerialNo missmatch! ");
#endif
        DPRINTLN("Found Sensor Serial Numbers are not matching to requested SNs in SENSOR_ORDER[]");
        }
      };


    void init (Hal& hal) {
      TSDevice::init(hal);
      sensarray.sensorcount = Ds18b20::init(oneWire, sensarray.sensors, MAX_SENSORS);
      sensCnt = sensarray.sensorcount;
      //DPRINT("Found Sens#="); DDECLN(sensCnt);
      for (int i = 0; i < sensCnt; i++) {
        DPRINT("Sensor ");DDEC(i+1);
        byte a[8];
        if (oneWire.search(a))  {
          memcpy(&(addrSN[i]), a, 8);
          DPRINTLN(":0x" + formatSN(addrSN[i]));
          }
        else {
          DPRINT(": n.a.");
          }
        }
      sensorSequence();
#ifdef USE_LCD
      lcd.setCursor(0, 3);
      lcd.print(("Sens#: " + String(sensarray.sensorcount) + ", Upd: " + String(this->getList0().Sendeintervall()) + "s ").substring(0,20));
#endif
      sysclock.add(sensarray, 1000UL * 7); // nach 7000 MilliSekunden Init-Text durch Messwerte ersetzen
      } // init
  }; // class UType

UType sdev(devinfo, 0x20);
ConfigButton<UType> cfgBtn(sdev);  // System-Button

#ifdef WITH_BACKLIGHT
  StateButton<> btn1();

// Vorlage für den folgenden Teil war AskSinPP\examples\custom\HB-LC-Bl1-Velux
 
  // Service-Routine für Knopf, Methoden werden später implementiert
  class PushPin : public Alarm {
    public:
    PushPin () { async(true); }
    virtual ~PushPin () {}
    virtual void trigger (__attribute__((unused)) AlarmClock& clock) {
      ;
    }

  };

class LocalBtn : public StateButton<> {
  public:
    enum Mode {BCK};

  private:
    Mode m_Mode;
    uint8_t toggle = 1; // BackLight 1/0, default 1 da Backlight defaultmäßig EIN ist.

  public:
    LocalBtn (Mode m) : m_Mode(m) {}
    virtual ~LocalBtn () {}
    virtual void state(uint8_t s) {
      uint8_t last = StateButton<>::state();
      StateButton<>::state(s);
      if( (s == StateButton<>::released) && (page == -1)) { //normale Temperaturanzeige und Ende kurzer Tastendruck
        toggle = (toggle + 1) % 2;
        lcd.setBacklight(toggle); // AUS / EIN
        // DPRINT("Button Pin ");DDEC(getPin());DPRINT(", toggle=");DDECLN(toggle);
        }
      else if(s == StateButton<>::longpressed && last != StateButton<>::longpressed ) {
        // Sensor-Seriennummern anzeigen oder zurück zur normalen Anzeige
        lcd.backlight();
        toggle=1;
        page++;
        if ((4*page) >= sensCnt) { // 1..4 Sensoren: reset 1 to -1; 5..8 Sensoren: reset 2 to -1 
          page=-1; // normal display of temperatures
          }
        // DPRINT("Button LongPress Page atfer incr=");DDECLN(page);
        if ((page>-1) && ((4*page) < sensCnt)) { // Anzeige der SerienNummern
          for (int r = 0; r < 4; r++) {
            int8_t i = 4*page + r;
            lcd.setCursor(0, /*row=*/r);
            if ((i < sensCnt) && (senSeq[i] > -1)) {
              lcd.print((String(i + 1) + ":" + formatSN(addrSN[senSeq[i]]) + "   ").substring(0,20));
              }
            else {
              lcd.print((String(i + 1) + ": n.a                              ").substring(0,20));
              }
            } // for
          } // if ((4*page) < sensCnt)
        else {
          lcd.clear();
          headline();
          //DPRINT("Button LongPress clear/headline due to Page=");DDECLN(page);
          sdev.sensarray.dispTemps(/*upd=*/false); // sensarray is private!!?? 
          }
        }
      } // state 

/*
    void irq () {
      // irq is only handled if the button is not pressed by software / PushPin
      if( PushPin::active() == false ) {
        StateButton<>::irq();
        }
      }
 */
    }; //class LocalBtn

  LocalBtn bckbtn(LocalBtn::BCK);
#endif


void setup () {
  DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);
  memset(Offsets, 0, sizeof(Offsets));
  HMID my_devid;
  sdev.getDeviceID(my_devid);
  DPRINT(F("DEVINFO ID: "));my_devid.dump();
  uint8_t my_serial[11];
  sdev.getDeviceSerial(my_serial);
  my_serial[10]=0;
  DPRINT(F("  Serial: "));DPRINTLN((char*)my_serial);
#ifdef USE_LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(SKETCH_NAME);
  lcd.setCursor(5, 1);
  lcd.print((char*)my_serial);
  lcd.setCursor(4, 2);
  lcd.print(my_devid, HEX); // no leading zeros!
  lcd.print("  V"); lcd.print(FMVERS, 1); //mit einer Kommastelle
#endif

  sdev.init(hal);
  // geg. für andere Kanäle:
  // sdev.switch1Channel().init(SWITCH_PIN1);
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
#ifdef WITH_BACKLIGHT
  buttonISR(bckbtn, BTN_BACKLIGHT_PIN);  // Interrupt für Button aktivieren
#endif
  // Optional hier: 
  // #include "FrequenzAbgleich.h" // wenn dort die Werte vom Funkmodul eingetragen wurden
  // Alternativer Abbgleich: Funkmodul-Sketch auf dem Arduino laufen lassen bevor dieser Sketch geladen wird.

  sdev.initDone();
  } // setup()

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  if ( worked == false && poll == false ) {
    if ( hal.battery.critical() ) {
      hal.activity.sleepForever(hal);
      }
#ifdef USE_WOR
    hal.activity.savePower<Sleep<> >(hal); // nur nach Senden empfangsbereit
#else
    hal.activity.savePower<Idle<> >(hal); // immer empfangsbereit
    // Idle: gemäß https://homematic-forum.de/forum/viewtopic.php?f=76&t=46017&start=20#p464616 bleibt nur Time 2 für Pin 3 aktiv.
#endif
    }
  }
