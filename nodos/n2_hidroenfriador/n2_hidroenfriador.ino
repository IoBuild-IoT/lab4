// Nodo 2 - Hidroenfriador: bomba de agua fria hasta pulpa <=2C en <2h (1 s real = 1 min simulado)
#define NODE_ID "n2"
#define NODE_KEY KEY_N2
#include "g2_common.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#define PIN_DS 4                 // DS18B20 agua
#define PIN_DS_P 16              // DS18B20 pulpa (bus propio: no depende del orden de busqueda 1-Wire)
#define PIN_RELE 26
#define SIM_X 60                 // 1 s real = 1 min simulado
OneWire ow(PIN_DS), owP(PIN_DS_P); DallasTemperature ds(&ow), dsP(&owP);
float tAgua = NAN, tPulpa = NAN; bool bomba = false; uint32_t t0 = 0; String lote = "L-251103-01";
bool avLento = false, avAgua = false;   // una alerta por episodio (evita inundar al gateway)
uint32_t minSim() { return t0 ? (millis() - t0) / 1000 : 0; }   // minutos simulados desde inicio del enfriado
void setBomba(bool on) { if (on != bomba) { bomba = on; digitalWrite(PIN_RELE, on); if (on && !t0) t0 = millis(); Serial.printf("BOMBA %s\n", on ? "ON" : "OFF"); } }
void onMsg(char* t, uint8_t* p, unsigned int n) {
  JsonDocument d; if (deserializeJson(d, p, n)) return; String m;
  // Defensa: "lote enfriado" falso -> firma + coherencia (pulpa <=2.5C y tiempo >= 20 min simulados)
  if (String((const char*)(d["cmd"] | "")) == "lote_enfriado") {
    bool sigOk = verifyCmd(d, m);
    bool coh = !isnan(tPulpa) && tPulpa <= 2.5 && minSim() >= 20;
    if (!sigOk || !coh) { g2Alert("falso_lote_enfriado", NODE_ID, "rechazado", !sigOk ? m.c_str() : "incoherente con temperatura/tiempo"); return; }
    setBomba(false); return;
  }
  if (!verifyCmd(d, m)) { g2Alert("orden_rechazada", NODE_ID, "descartada", m.c_str()); return; }
  String a = d["arg"] | "";
  if (String((const char*)(d["cmd"] | "")) == "set_lote" && g2LoteOk(a) && a != lote) { lote = a; t0 = bomba ? millis() : 0; avLento = false; }   // nuevo lote: reinicia el tiempo de enfriado
}
void setup() { Serial.begin(115200); pinMode(PIN_RELE, OUTPUT); ds.begin(); dsP.begin(); g2Wifi(); }
void loop() {
  delay(10);  // cede CPU entre lecturas al simular varios ESP32
  g2Reconnect(onMsg); mqtt.loop(); g2Heartbeat();
  static uint32_t tr = 0, tp = 0;
  if (millis() - tr > 1000) {
    tr = millis(); ds.requestTemperatures(); dsP.requestTemperatures();
    float a = ds.getTempCByIndex(0), p = dsP.getTempCByIndex(0);
    if (a > -40 && a < 80) tAgua = a; if (p > -40 && p < 80) tPulpa = p;
    // Edge: bomba ON si pulpa >2C; OFF con histeresis a <=1C. Alerta a los 90 min simulados si no baja.
    if (!isnan(tPulpa)) { if (tPulpa > 2.0) setBomba(true); else if (tPulpa <= 1.0) setBomba(false); }
    if (t0 && minSim() > 90 && tPulpa > 2.0 && !avLento) avLento = true, g2Alert("enfriado_lento", NODE_ID, "bomba_on", "90 min sin llegar a 2C (limite 120)");
    if (!isnan(tAgua) && tAgua > 4 && !avAgua) avAgua = true, g2Alert("agua_tibia", NODE_ID, "revisar_chiller", "agua >4C");
    if (!isnan(tAgua) && tAgua <= 3.5) avAgua = false;
  }
  if (millis() - tp > 2000) { tp = millis(); pubSigned("datos", lote.c_str(), tPulpa, "C", isnan(tPulpa) ? "bad" : "ok"); pubSigned("datos", lote.c_str(), tAgua, "C_agua", isnan(tAgua) ? "bad" : "ok"); }
}
