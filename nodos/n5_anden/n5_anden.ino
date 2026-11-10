// Nodo 5 - Anden: controla tiempo de espera fuera del frio. PIR = pallet presente. Semaforo RGB.
#define NODE_ID "n5"
#define NODE_KEY KEY_N5
#include "g2_common.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#define PIN_DS 4
#define PIN_PIR 27
#define PIN_R 25
#define PIN_G 26
#define PIN_B 33
#define SIM_X 60                 // 1 s real = 1 min simulado
#define MAX_MIN 20               // maximo en anden (min simulados): amarillo al 50 %, rojo al 100 %
OneWire ow(PIN_DS); DallasTemperature ds(&ow);
float tAmb = NAN; uint32_t pallet0 = 0; bool suspendido = false; String lote = "L-251103-01";
// Defensa contra inundacion: max 5 mensajes entrantes por segundo; el exceso se descarta
uint32_t ventana = 0; int cuenta = 0, descartados = 0, avisoFlood = 0;
void rgb(bool r, bool g, bool b) { digitalWrite(PIN_R, r); digitalWrite(PIN_G, g); digitalWrite(PIN_B, b); }
void onMsg(char* t, uint8_t* p, unsigned int n) {
  if (millis() - ventana > 1000) { if (descartados) avisoFlood = descartados; ventana = millis(); cuenta = 0; descartados = 0; }
  if (++cuenta > 5) { descartados++; return; }
  JsonDocument d; if (deserializeJson(d, p, n)) return; String m;
  if (!verifyCmd(d, m)) { g2Alert("orden_rechazada", NODE_ID, "descartada", m.c_str()); return; }
  String c = d["cmd"] | ""; if (c == "suspender_carga") suspendido = true; else if (c == "reanudar_carga") suspendido = false; else if (c == "set_lote") { String a = d["arg"] | ""; if (g2LoteOk(a)) lote = a; }   // lote actual asignado por el gateway
}
void setup() { Serial.begin(115200); pinMode(PIN_PIR, INPUT); pinMode(PIN_R, OUTPUT); pinMode(PIN_G, OUTPUT); pinMode(PIN_B, OUTPUT); ds.begin(); g2Wifi(); }
void loop() {
  delay(10);  // cede CPU entre lecturas al simular varios ESP32
  g2Reconnect(onMsg); mqtt.loop(); g2Heartbeat();
  // La alerta de inundacion se publica aqui y no en onMsg: publicar dentro del callback pisa el buffer de PubSubClient
  if (avisoFlood) { char b[48]; snprintf(b, 48, "descartados %d msg/s", avisoFlood); avisoFlood = 0; g2Alert("inundacion", NODE_ID, "descarte_exceso", b); }
  bool pres = digitalRead(PIN_PIR);
  if (pres && !pallet0) pallet0 = millis(); if (!pres) pallet0 = 0;
  uint32_t min = pallet0 ? (millis() - pallet0) / 1000 : 0;       // minutos simulados en anden
  static uint32_t tr = 0, tp = 0; static int estado = -1;
  if (millis() - tr > 1000) { tr = millis(); ds.requestTemperatures(); float t = ds.getTempCByIndex(0); if (t > -40 && t < 80) tAmb = t; }
  int e = !pres ? 0 : (min >= MAX_MIN ? 3 : (min >= MAX_MIN / 2 ? 2 : 1));   // 0 libre,1 verde,2 amarillo,3 rojo
  if (suspendido) e = 4;
  if (e != estado) {
    estado = e;
    if (e == 3) g2Alert("anden_tiempo_max", NODE_ID, "semaforo_rojo", "pallet excede tiempo maximo en anden");
    if (e == 4) rgb(1, 0, 1); else if (e == 3) rgb(1, 0, 0); else if (e == 2) rgb(1, 1, 0); else if (e == 1) rgb(0, 1, 0); else rgb(0, 0, 0);
  }
  if (millis() - tp > 2000) { tp = millis(); pubSigned("datos", lote.c_str(), tAmb, "C", isnan(tAmb) ? "bad" : "ok"); pubSigned("datos", lote.c_str(), min, "min_anden", "ok"); }
}
