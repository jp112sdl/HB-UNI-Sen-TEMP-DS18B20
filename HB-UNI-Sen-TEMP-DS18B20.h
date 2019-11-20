//---------------------------------------------------------
// HB-UNI-Sens-TEMP-DS18B20.h
//---------------------------------------------------------
// 2016-10-31 papa Creative Commons
// 2018-03-24 jp112sdl Creative Commons
// 2019-11-19 Wolfram Winter Creative Commons
// http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// You are free to Share & Adapt under the following terms:
// Give Credit, NonCommercial, ShareAlike
//---------------------------------------------------------

#ifndef _HB-UNI-Sen-TEMP-DS18B20_H_
#define _HB-UNI-Sen-TEMP-DS18B20_H_

//---------------------------------------------------------
// !! NDEBUG should be defined when the development and testing is done
//
// #define NDEBUG

//---------------------------------------------------------
// Device Angaben
#define cDEVICE_ID      { 0xF3, 0x01, 0x01 }
#define cDEVICE_SERIAL  "UNITEMP8-1"

// Device Model  HB-UNI-Sen-TEMP-DS18B20
// 1..8fach DS18B20 Temperatursensor
// https://github.com/jp112sdl/JP-HB-Devices-addon
#define cDEVICE_MODEL   {0xF3, 0x01}    

//---------------------------------------------------------
// Batterie-Limit [V*10]
#define cBAT_LOW_LIMIT  22
#define cBAT_CRT_LIMIT  18

//---------------------------------------------------------
// Sende-Intervall [sec]
#define cSEND_INTERVAL  180      // 180 = 3 Minuten

// not defined (default) - es wird immer der zuletzt gesetzte Sende-Intervallwert benutzt
// defined               - cSEND_INTERVAL wird immer bei Modulstart neu gesetzt
// #define cSEND_INTERVAL_ONSTART

//---------------------------------------------------------
// Pin-Anschluss der DS18B20 Sensoren
#define cPORT_ONEWIRE   A5

//---------------------------------------------------------
// Anzahl der angeschlossenen Sensoren
// und
// Reihenfolge der Ausgabe der Sensor-Temperatur-Werte
//---------------------------------------------------------
//
// defined 'cSENS_ID_ORDER'
// ------------------------
// - mit der Übergabe der vorher ermittelten Sensor-IDs (siehe unten) kann die 
//   Reihenfolge bzw. die Zuordnung der Temperatur-Ausgabe auf die 8 Ports 
//   festgelegt werden
//   - es werden die Temperaturen in der vorgegebenen Reihenfolge ausgegeben
//   - fällt ein Sensor aus (oder ist nicht vorhanden), dann wird -999 als 
//     Temperatur ausgegeben
//
// not defined 'cSENS_ID_ORDER'
// ----------------------------
// - die Reihenfolge bzw. die Zuordnung der Temperatur-Ausgabe auf die 8 Ports 
//   wird durch die Reihenfolge bei der Initialisierung der Sensoren festgelegt
//   - es werden die (ansprechbaren) Sensoren nacheinander abgefragt
//   - in der Regel findet die Ausgabe der Temperatur absteigend nach Sensor-ID
//     statt (i.d.R.: Sensor mit höchster ID zuerst)
//   - dabei werden evtl. defekte Sensoren übersprungen - dadurch kann die 
//     Reihung der Temperatur-Ausgabe durcheinander kommen
//   - es findet keine Fehlerabfrage statt
// - mit dieser Einstellung können im Debug-Modus die Sensor-IDs ermittelten
//   werden (siehe oben)
// - siehe auch: https://homematic-forum.de/forum/viewtopic.php?t=45004#p451960
// 

// Anzahl der abzufragenden DS18B20 Sensoren (max. 8)
#define cANZ_SENSORS    2

// Flag für die Vorgabe / Sortierung der DS18B20 Temperaturwerte
// auskommentieren, wenn keine Sortierung
#define cSENS_ID_ORDER

// Vorgabe / Sortierung der DS18B20 Sensoren
//  !!! cSENS_ID_x  mit 0 oder 16 Zeichen !!!
#define cSENS_ID_1      "28AA9933191302C8"
#define cSENS_ID_2      "28AA9CFF1813029A"
#define cSENS_ID_3      ""
#define cSENS_ID_4      ""
#define cSENS_ID_5      ""
#define cSENS_ID_6      ""
#define cSENS_ID_7      ""
#define cSENS_ID_8      ""

#endif
