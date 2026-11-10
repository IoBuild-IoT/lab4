// Nodo 1 - Recepcion: registra lote + temperatura de pulpa. Semaforo RGB.
#define NODE_ID "n1"
#define NODE_KEY KEY_N1
#include "g2_common.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#define PIN_DS 4
#define PIN_BTN 5
#define PIN_R 25
#define PIN_G 26
#define PIN_B 27
OneWire ow(PIN_DS); DallasTemperature ds(&ow);
String lotes[24]; int nLotes = 0; String loteActual = "";
int seq = 0; float tPulpa = NAN, tPrev = NAN;

// Defensa: formato L-AAMMDD-NN y unicidad
bool formatoOk(const String& s) {
  if (s.length() != 11 || s[0] != 'L' || s[1] != '-' || s[8] != '-') return false;
  for (int i : {2,3,4,5,6,7,9,10}) if (!isDigit(s[i])) return false;
  return true;
}
bool registrar(const String& id, const char* origen) {
  if (!formatoOk(id)) { g2Alert("lote_falsificado", NODE_ID, "rechazado", "formato invalido"); Serial.printf("RECHAZO %s (formato)\n", id.c_str()); return false; }
  for (int i = 0; i < nLotes && i < 24; i++) if (lotes[i] == id) { g2Alert("lote_falsificado", NODE_ID, "rechazado", "identificador duplicado"); Serial.printf("RECHAZO %s (duplicado)\n", id.c_str()); return false; }
  lotes[nLotes++ % 24] = id;                       // anillo: conserva los 24 ultimos
  loteActual = id;
  pubSigned("datos", id.c_str(), isnan(tPulpa) ? 0 : tPulpa, "C", "reg");   // calidad=reg: registro de lote
  Serial.printf("LOTE registrado %s (%s)\n", id.c_str(), origen); return true;
}
void onMsg(char* t, uint8_t* p, unsigned int n) {
  JsonDocument d; if (deserializeJson(d, p, n)) return;
  if (d["reg"].is<const char*>()) { registrar(String((const char*)d["reg"]), "mqtt"); return; }  // intento de registro (panel de ataques)
  String m; if (!verifyCmd(d, m)) g2Alert("orden_rechazada", NODE_ID, "descartada", m.c_str());
}
void rgb(bool r, bool g, bool b) { digitalWrite(PIN_R, r); digitalWrite(PIN_G, g); digitalWrite(PIN_B, b); }
void setup() {
  Serial.begin(115200); pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_R, OUTPUT); pinMode(PIN_G, OUTPUT); pinMode(PIN_B, OUTPUT); ds.begin(); g2Wifi();
}
void loop() {
  delay(10);  // cede CPU entre lecturas al simular varios ESP32
  g2Reconnect(onMsg); mqtt.loop(); g2Heartbeat();
  static uint32_t tb = 0, tr = 0, tp = 0; static bool btnPrev = HIGH;
  bool btn = digitalRead(PIN_BTN); bool flanco = btn == LOW && btnPrev == HIGH; btnPrev = btn;   // un lote por pulsacion
  if (flanco && millis() - tb > 400) { tb = millis(); char id[16]; snprintf(id, 16, "L-251103-%02d", ++seq); registrar(id, "boton"); }
  if (millis() - tr > 1000) {            // edge: lectura validada (rango y salto brusco)
    tr = millis(); ds.requestTemperatures(); float t = ds.getTempCByIndex(0);
    bool ok = t > -40 && t < 80 && !(t == DEVICE_DISCONNECTED_C) && (isnan(tPrev) || fabs(t - tPrev) < 15);
    if (ok) { tPulpa = t; tPrev = t; }
    // Semaforo: rojo = enfriar ya (>10C), amarillo 2-10, verde <=2
    if (!ok) rgb(1, 0, 1); else if (tPulpa > 10) rgb(1, 0, 0); else if (tPulpa > 2) rgb(1, 1, 0); else rgb(0, 1, 0);
  }
  if (millis() - tp > 2000 && loteActual.length()) { tp = millis(); pubSigned("datos", loteActual.c_str(), tPulpa, "C", isnan(tPulpa) ? "bad" : "ok"); }
}
