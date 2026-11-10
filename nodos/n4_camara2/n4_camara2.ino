// Nodo 4 - Camara 2: DHT22 + puerta. Alarma a 60 s, cierre por servo a 120 s (edge, sin gateway).
#define NODE_ID "n4"
#define NODE_KEY KEY_N4
#include "g2_common.h"
#include <DHT.h>
#include <ESP32Servo.h>
#define PIN_DHT 4
#define PIN_DOOR 14
#define PIN_SERVO 13
#ifndef T_ALARMA_S
#define T_ALARMA_S 60
#endif
#ifndef T_CIERRE_S
#define T_CIERRE_S 120
#endif
DHT dht(PIN_DHT, DHT22); Servo servo;
float temp = NAN, hum = NAN; uint32_t abierta = 0; bool cerradoForzado = false; String lote = "L-251103-01";
#define PIN_FAN 26
#define PIN_LED 2
bool fan = false, fanGw = false, alarma = false; uint32_t ultimoGw = 0;   // fanGw: fan_on del gateway mantiene el ventilador
void salidaAlarma(bool on) { digitalWrite(PIN_LED, on); }
void setFan(bool on) { fan = on; digitalWrite(PIN_FAN, on); }
void cerrarPuerta() { servo.write(90); cerradoForzado = true; }
void abrirPuerta()  { servo.write(0);  cerradoForzado = false; }
void onMsg(char* t, uint8_t* p, unsigned int n) {
  if (!strcmp(t, "upc/g2/gw/estado")) { if (n == 6 && !memcmp(p, "online", 6)) ultimoGw = millis(); return; }   // heartbeat del gateway
  JsonDocument d; if (deserializeJson(d, p, n)) return; String m;
  if (!verifyCmd(d, m)) { g2Alert("orden_rechazada", NODE_ID, "descartada", m.c_str()); return; }   // firma + contador
  ultimoGw = millis();
  String c = d["cmd"] | "";
  // Defensa: nunca se apaga el ventilador si la camara esta fuera de rango
  if (c == "fan_off") { if (!isnan(temp) && temp > 2.0) g2Alert("orden_peligrosa", NODE_ID, "rechazada", "ventilador no se apaga con T>2C"); else { fanGw = false; setFan(false); } }
  else if (c == "fan_on") { fanGw = true; setFan(true); }
  else if (c == "cerrar_puerta") cerrarPuerta(); else if (c == "abrir_puerta") { if (temp > 2.0) g2Alert("orden_peligrosa", NODE_ID, "rechazada", "no se abre puerta con T>2C"); else abrirPuerta(); }
  else if (c == "set_lote") { String a = d["arg"] | ""; if (g2LoteOk(a)) lote = a; }   // lote actual asignado por el gateway
}
void setup() { Serial.begin(115200); pinMode(PIN_DOOR, INPUT_PULLUP); pinMode(PIN_FAN, OUTPUT); pinMode(PIN_LED, OUTPUT); dht.begin(); servo.attach(PIN_SERVO); abrirPuerta(); g2ExtraSub = "upc/g2/gw/estado"; g2Wifi(); ultimoGw = millis(); }
void loop() {
  delay(10);  // cede CPU entre lecturas al simular varios ESP32
  g2Reconnect(onMsg); mqtt.loop(); g2Heartbeat();
  bool open = digitalRead(PIN_DOOR) == HIGH;           // interruptor abierto = puerta abierta
  // Episodio de apertura: alarma a T_ALARMA_S y cierre por servo a T_CIERRE_S (una vez por episodio, sin depender
  // de ordenes previas del gateway); al cerrarse la puerta se rearma todo
  static bool cierreHecho = false;
  if (open && !abierta) abierta = millis() | 1; if (!open) { abierta = 0; alarma = false; cierreHecho = false; }
  uint32_t s = abierta ? (millis() - abierta) / 1000 : 0;
  if (open && s >= T_ALARMA_S && !alarma) { alarma = true; g2Alert("puerta_abierta", NODE_ID, "alarma", "puerta abierta >60 s"); }
  if (open && s >= T_CIERRE_S && !cierreHecho) { cierreHecho = true; cerrarPuerta(); g2Alert("puerta_abierta", NODE_ID, "servo_cierra", "puerta abierta >120 s, cierre local sin gateway"); }
  // Modo seguro: sin noticias del GATEWAY >60 s (heartbeat upc/g2/gw/estado u orden valida; no basta el broker)
  // -> ventilador ON + alarma local. Fuera de modo seguro: termostato 1/2 C, salvo fan_on del gateway.
  bool seguro = millis() - ultimoGw > 60000;
  if (seguro) { setFan(true); salidaAlarma(true); }
  else { salidaAlarma(alarma); if (fanGw || (!isnan(temp) && temp > 2.0)) setFan(true); else if (!isnan(temp) && temp < 1.0) setFan(false); }
  static uint32_t tr = 0, tp = 0;
  if (millis() - tr > 2000) { tr = millis(); float t = dht.readTemperature(), h = dht.readHumidity(); if (!isnan(t) && t > -40 && t < 60) temp = t; if (!isnan(h)) hum = h; }
  if (millis() - tp > 2000) { tp = millis(); pubSigned("datos", lote.c_str(), temp, "C", isnan(temp) ? "bad" : "ok"); pubSigned("datos", lote.c_str(), open ? 1 : 0, "puerta", "ok"); }
}
