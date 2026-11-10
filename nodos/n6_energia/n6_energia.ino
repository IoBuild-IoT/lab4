// Nodo 6 - Energia: detecta apagon, arranca grupo electrogeno y confirma.
#define NODE_ID "n6"
#define NODE_KEY KEY_N6
#include "g2_common.h"
#define PIN_RED 14       // slide switch: HIGH = hay red
#define PIN_GRUPO 12     // slide switch: confirmacion grupo en marcha
#define PIN_RELE 26
#define PIN_LED 27
#define T_LOCAL_S 10     // si el gateway no ordena en 10 s, arranque local (sin internet/gateway)
bool red = true, grupoCmd = false; uint32_t apagon0 = 0;
void setGrupo(bool on, const char* origen) { if (on != grupoCmd) { grupoCmd = on; digitalWrite(PIN_RELE, on); Serial.printf("GRUPO %s (%s)\n", on ? "START" : "STOP", origen); } }
void onMsg(char* t, uint8_t* p, unsigned int n) {
  JsonDocument d; if (deserializeJson(d, p, n)) return; String m;
  if (!verifyCmd(d, m)) { g2Alert("orden_falsa_grupo", NODE_ID, "rechazada", m.c_str()); return; }
  String c = d["cmd"] | "";
  if (c == "gen_start") setGrupo(true, "gateway");
  else if (c == "gen_stop") {
    if (!red) g2Alert("orden_peligrosa", NODE_ID, "rechazada", "no se apaga el grupo si no hay red");   // regla de seguridad
    else setGrupo(false, "gateway");
  }
}
void setup() { Serial.begin(115200); pinMode(PIN_RED, INPUT_PULLUP); pinMode(PIN_GRUPO, INPUT_PULLUP); pinMode(PIN_RELE, OUTPUT); pinMode(PIN_LED, OUTPUT); g2Wifi(); }
void loop() {
  delay(10);  // cede CPU entre lecturas al simular varios ESP32
  g2Reconnect(onMsg); mqtt.loop(); g2Heartbeat();
  bool r = digitalRead(PIN_RED) == LOW;                    // interruptor cerrado a GND = red OK
  bool grupoOn = digitalRead(PIN_GRUPO) == LOW;            // confirmacion fisica
  if (!r && red) { apagon0 = millis(); g2Alert("apagon", NODE_ID, "solicita_grupo", "perdida de red electrica"); }
  if (r && !red) { apagon0 = 0; setGrupo(false, "red restablecida"); }
  red = r;
  if (!red && !grupoCmd && millis() - apagon0 > T_LOCAL_S * 1000UL) setGrupo(true, "respaldo local");   // edge: no depende del gateway
  static uint32_t tl = 0, tp = 0;
  if (millis() - tl > 50) { tl = millis(); digitalWrite(PIN_LED, red ? HIGH : (grupoOn ? (millis() / 400) % 2 : (millis() / 100) % 2)); }  // fijo=red, lento=grupo, rapido=sin respaldo
  if (millis() - tp > 2000) { tp = millis(); pubSigned("datos", "-", red, "red", "ok"); pubSigned("datos", "-", grupoOn, "grupo", "ok"); }
}
