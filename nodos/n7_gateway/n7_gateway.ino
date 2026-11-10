// Nodo 7 - Gateway / fog. Decide localmente (sin internet): verifica HMAC por nodo,
// rate-limit, trazabilidad por lote (minutos fuera de 0-2 C por etapa), lazo cerrado
// de energia, modo seguro por heartbeat, panel TFT + NeoPixel + RTC. ThingSpeak opcional.
#define NODE_ID "gw"
#define NODE_KEY GW_KEY            // el gateway firma con GW_KEY
#include "g2_common.h"
#include <HTTPClient.h>
#include <Preferences.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_NeoPixel.h>

// ===================== CONFIG SOLO GATEWAY (claves desde secrets.h) =====================
#ifndef KEY_N1
#define KEY_N1 "key-n1"
#endif
#ifndef KEY_N2
#define KEY_N2 "key-n2"
#endif
#ifndef KEY_N3
#define KEY_N3 "key-n3"
#endif
#ifndef KEY_N4
#define KEY_N4 "key-n4"
#endif
#ifndef KEY_N5
#define KEY_N5 "key-n5"
#endif
#ifndef KEY_N6
#define KEY_N6 "key-n6"
#endif
#ifndef TS_WRITE_KEY
#define TS_WRITE_KEY ""
#endif
#ifndef SIM_X
#define SIM_X 60                  // segundos simulados por segundo real (60: 1 s = 1 min)
#endif
#ifndef DETECT_MODE
#define DETECT_MODE 'B'           // 'A' umbral instantaneo (edge) | 'B' regla acumulada
#endif
#define N_NODOS 6
static const char* NODOS[N_NODOS] = {"n1", "n2", "n3", "n4", "n5", "n6"};
static const char* NKEYS[N_NODOS] = {KEY_N1, KEY_N2, KEY_N3, KEY_N4, KEY_N5, KEY_N6};
static const int8_t ETAPA_DE[N_NODOS] = {0, 1, 2, 2, 3, -1};   // n6 energia: sin etapa
static const char* ETAPAS[4] = {"recepcion", "hidro", "camara", "anden"};
#define MAX_MSG_S       10        // flood: >10 msg/s por nodo -> descarte
#define OFFLINE_MS      60000UL   // sin noticias >60 s -> offline
#define GEN_TIMEOUT_MS  30000UL   // confirmacion grupo electrogeno
#define CONFIRM_MS      15000UL   // doble confirmacion de cambios de config
#define TS_PERIOD_MS    20000UL   // ThingSpeak >= 15 s
#define MAX_LOTES       8

// Pines
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST 4
#define PIN_PIX 13
#define N_PIX 8
#define PIN_ACK 14

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
Adafruit_NeoPixel pix(N_PIX, PIN_PIX, NEO_GRB + NEO_KHZ800);
RTC_DS1307 rtc; bool rtcOk = false;
Preferences prefs;

// Parametros configurables (cambio solo con firma + doble confirmacion)
float tMin = 0.0, tMax = 2.0, budgetMin = 120.0, prePct = 0.80; char modo = DETECT_MODE;

// ===================== ESTADO =====================
struct Nodo { uint32_t lastSeen = 0; bool online = false, everSeen = false; float temp = NAN, tAux = NAN; int puerta = -1;
              uint32_t win = 0; uint16_t cnt = 0, drop = 0; uint32_t lastFlood = 0; long lastTs = -1; char lastSig[33] = ""; };
Nodo nodos[N_NODOS];
struct Lote { bool used = false; char id[16]; uint8_t etapa = 0; float minEt[4] = {0, 0, 0, 0}; float minTot = 0; float t[N_NODOS];
              uint32_t tT[N_NODOS]; bool pre = false, rup = false, aFlag = false, enfriadoTx = false; uint32_t lastSeen = 0; };
Lote lotes[MAX_LOTES];
Lote* loteActivo = nullptr;
uint32_t ultimoSetLote[N_NODOS] = {};
bool redOk = true, grupoOn = false, genWait = false, respaldoFalla = false; uint32_t genT0 = 0, apagonUs = 0;
uint32_t gwCtr = 0, ctrReserved = 0;
struct Pend { bool on = false; String kv; uint32_t t = 0; } pend;
#define NLOG 5
String logL[NLOG]; uint16_t logC[NLOG]; int nAlert = 0, unacked = 0, nAlertA = 0, nAlertB = 0; uint32_t lastSec = 0;
uint32_t lastSecAlert[16];

int nodeIdx(const char* n) { for (int i = 0; i < N_NODOS; i++) if (!strcmp(n, NODOS[i])) return i; return -1; }

String hora() {
  char b[12];
  if (rtcOk) { DateTime n = rtc.now(); snprintf(b, sizeof b, "%02d:%02d:%02d", n.hour(), n.minute(), n.second()); }
  else { uint32_t s = millis() / 1000; snprintf(b, sizeof b, "+%02lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60), (unsigned long)(s % 60)); }
  return String(b);
}
long tsNow() { return rtcOk ? (long)rtc.now().unixtime() : (long)(millis() / 1000); }

void addLog(const String& s, uint16_t col) {
  for (int i = NLOG - 1; i > 0; i--) { logL[i] = logL[i - 1]; logC[i] = logC[i - 1]; }
  logL[0] = hora().substring(0, 8) + " " + s; logC[0] = col; if (logL[0].length() > 52) logL[0] = logL[0].substring(0, 52);
}

// Alerta del gateway (firmada): firma = HMAC(GW_KEY,"gw|cat|tipo|nodo|ts")
void gwAlert(const char* cat, const char* tipo, const char* nodo, const char* accion, const char* motivo,
             const char* lote = nullptr, const char* etapa = nullptr, float minF = -1) {
  long ts = tsNow(); String h = hora();
  JsonDocument d; d["id"] = "gw"; d["cat"] = cat; d["tipo"] = tipo; d["hora"] = h; d["nodo"] = nodo;
  d["accion"] = accion; d["motivo"] = motivo; d["ts"] = ts;
  if (lote) { d["lote"] = lote; d["etapa"] = etapa; d["min_fuera"] = serialized(fmtV(minF)); d["presupuesto"] = budgetMin; d["modo"] = String(modo); }
  d["firma"] = hmacHex(GW_KEY, String("gw|") + cat + "|" + tipo + "|" + nodo + "|" + ts);
  char buf[400]; size_t n = serializeJson(d, buf, sizeof buf);
  if (mqtt.connected()) mqtt.publish(tpc("alerta").c_str(), (const uint8_t*)buf, n, false);
  Serial.printf("ALERTA %s\n", buf);
  nAlert++; unacked++;
  bool sec = !strcmp(cat, "seguridad"); if (sec) lastSec = millis();
  addLog(String(sec ? "SEG " : "") + tipo + " " + nodo + (lote ? String(" ") + lote : String("")), sec ? ILI9341_MAGENTA : ILI9341_ORANGE);
}
// Alerta de seguridad con anti-rebote por (nodo,tipo) para no generar flood propio
void secAlert(int slot, const char* tipo, const char* nodo, const char* accion, const char* motivo) {
  slot &= 15; if (lastSecAlert[slot] && millis() - lastSecAlert[slot] < 3000) return; lastSecAlert[slot] = millis();
  gwAlert("seguridad", tipo, nodo, accion, motivo);
}

// ===================== ORDENES FIRMADAS =====================
uint32_t nextCtr() {   // estrictamente creciente incluso tras reinicio (NVS, reserva por bloques)
  gwCtr++;
  if (gwCtr >= ctrReserved) { ctrReserved = gwCtr + 64; prefs.putUInt("ctr", ctrReserved); }
  return gwCtr;
}
void sendOrder(const char* dest, const char* cmd, const char* arg) {
  if (!strcmp(cmd, "gen_stop") && !redOk) {   // regla de seguridad: nunca apagar el grupo sin red
    secAlert(15, "orden_insegura", dest, "bloqueada", "gen_stop sin red electrica"); return;
  }
  uint32_t ctr = nextCtr(); long ts = tsNow();
  String sig = hmacHex(GW_KEY, String("gw|") + dest + "|" + cmd + "|" + arg + "|" + String(ctr) + "|" + String(ts));
  JsonDocument d; d["id"] = "gw"; d["dest"] = dest; d["cmd"] = cmd; d["arg"] = arg; d["ctr"] = ctr; d["ts"] = ts; d["firma"] = sig;
  char buf[256]; size_t n = serializeJson(d, buf, sizeof buf);
  bool ok = mqtt.connected() && mqtt.publish(tpc("cmd", dest).c_str(), (const uint8_t*)buf, n, false);
  Serial.printf("ORDEN %s %s\n", ok ? "tx" : "SIN_RED", buf);
  addLog(String("> ") + dest + " " + cmd, ILI9341_CYAN);
}

// ===================== LOTES =====================
Lote* getLote(const char* id, bool crear) {
  if (!id || !*id || !strcmp(id, "-")) return nullptr;
  for (auto& l : lotes) if (l.used && !strcmp(l.id, id)) return &l;
  if (!crear) return nullptr;
  Lote* s = nullptr; for (auto& l : lotes) if (!l.used) { s = &l; break; }
  if (!s) { s = &lotes[0]; for (auto& l : lotes) if (l.lastSeen < s->lastSeen) s = &l; }   // recicla el mas antiguo
  *s = Lote(); s->used = true; strlcpy(s->id, id, sizeof s->id); s->lastSeen = millis();
  for (int i = 0; i < N_NODOS; i++) { s->t[i] = NAN; s->tT[i] = 0; }
  return s;
}

void publicarLote() {
  if (!mqtt.connected()) return;
  JsonDocument d;
  d["id"] = "gw";
  d["lote"] = loteActivo ? loteActivo->id : "";
  d["presupuesto"] = budgetMin;
  if (loteActivo) {
    const Lote& l = *loteActivo;
    d["etapa"] = ETAPAS[l.etapa];
    d["min_fuera"] = serialized(fmtV(l.minTot));
    JsonArray porEtapa = d["min_etapa"].to<JsonArray>();
    for (float m : l.minEt) porEtapa.add(serialized(fmtV(m)));
    d["prealerta"] = l.pre;
    d["ruptura"] = l.rup;
  }
  char buf[256];
  size_t n = serializeJson(d, buf, sizeof buf);
  mqtt.publish(tpc("lote").c_str(), (const uint8_t*)buf, n, true);
}
// Temperatura de la etapa actual del lote (camara: peor de n3/n4)
float loteTemp(Lote& l, int& nIdx) {
  float best = NAN; nIdx = -1;
  for (int i = 0; i < N_NODOS; i++) if (ETAPA_DE[i] == l.etapa && l.tT[i] && millis() - l.tT[i] < 15000 && !isnan(l.t[i]))
    if (isnan(best) || fabsf(l.t[i] - 1.0f) > fabsf(best - 1.0f)) { best = l.t[i]; nIdx = i; }
  return best;
}
bool fuera(float t) { return !isnan(t) && (t < tMin || t > tMax); }

void accion(Lote& l, int nIdx, const char* tipo, uint32_t detUs) {
  const char* nn = nIdx >= 0 ? NODOS[nIdx] : "gw";
  int peor = 0; for (int e = 1; e < 4; e++) if (l.minEt[e] > l.minEt[peor]) peor = e;
  char mot[96]; snprintf(mot, sizeof mot, "%.0f/%.0f min fuera de %.1f-%.1fC; ruptura en %s (%.0f min)", l.minTot, budgetMin, tMin, tMax, ETAPAS[peor], l.minEt[peor]);
  const char* acc = "avisar_operador";
  if (!strcmp(tipo, "ruptura_frio") || !strcmp(tipo, "prealerta_frio")) {
    if (l.etapa == 2) { sendOrder("n3", "cerrar_puerta", l.id); sendOrder("n4", "cerrar_puerta", l.id); sendOrder("n4", "fan_on", l.id); acc = "cerrar_puertas_fan_on"; }
    if (l.etapa == 3) { sendOrder("n5", "suspender_carga", l.id); acc = "suspender_carga"; }
  }
  gwAlert("proceso", tipo, nn, acc, mot, l.id, ETAPAS[peor], l.minTot);
  uint32_t actUs = micros();
  if (modo == 'A') nAlertA++; else nAlertB++;
  Serial.printf("LAT,%c,%s,%s,%s,det_ms=%lu,lat_us=%lu,min_fuera=%.1f\n", modo, tipo, l.id, ETAPAS[l.etapa], (unsigned long)(detUs / 1000), (unsigned long)(actUs - detUs), l.minTot);
}

void tickLotes(uint32_t dtMs) {
  float dSim = dtMs / 1000.0f * SIM_X / 60.0f;   // minutos simulados
  for (auto& l : lotes) {
    if (!l.used) continue;
    if (millis() - l.lastSeen > 600000UL) { // 10 min sin datos: lote cerrado
      if (loteActivo == &l) loteActivo = nullptr;
      l.used = false;
      continue;
    }
    int ni; float t = loteTemp(l, ni);
    bool out = fuera(t);
    if (out) { l.minEt[l.etapa] += dSim; l.minTot += dSim; }
    if (modo == 'A') {             // Alternativa A: umbral instantaneo por episodio
      if (out && !l.aFlag) { l.aFlag = true; accion(l, ni, "umbral_A", micros()); }
      if (!out && !isnan(t)) l.aFlag = false;
    } else {                       // Alternativa B: presupuesto acumulado, aviso al 80 %
      if (!l.pre && l.minTot >= prePct * budgetMin) { l.pre = true; accion(l, ni, "prealerta_frio", micros()); }
      if (!l.rup && l.minTot >= budgetMin) { l.rup = true; accion(l, ni, "ruptura_frio", micros()); }
    }
  }
}

// ===================== MENSAJES DE NODOS =====================
bool verifyNode(int i, JsonDocument& d, String& motivo) {
  String v = d["valor"].is<const char*>() ? String((const char*)d["valor"]) : fmtV(d["valor"].as<float>());
  long ts = d["ts"].as<long>(); String sig = d["firma"] | "";
  String m = String(NODOS[i]) + "|" + (const char*)(d["lote"] | "") + "|" + String(ts) + "|" + v + "|" + (const char*)(d["unidad"] | "") + "|" + (const char*)(d["calidad"] | "");
  if (!ctEq(hmacHex(NKEYS[i], m), sig)) { motivo = "firma HMAC invalida"; return false; }
  Nodo& n = nodos[i];
  if (!strcmp(n.lastSig, sig.c_str()) || (ts < n.lastTs - 5 && ts > 60)) { motivo = "mensaje repetido/antiguo (replay)"; return false; }
  n.lastTs = ts; strlcpy(n.lastSig, sig.c_str(), sizeof n.lastSig); return true;
}

void marcarVivo(int i) { Nodo& n = nodos[i]; n.lastSeen = millis(); n.everSeen = true;
  if (!n.online) { n.online = true; addLog(String(NODOS[i]) + " online", ILI9341_GREEN); } }

void onDatos(int i, JsonDocument& d) {
  const char* u = d["unidad"] | ""; const char* cal = d["calidad"] | ""; const char* lid = d["lote"] | "";
  float v = d["valor"].is<const char*>() ? atof((const char*)d["valor"]) : d["valor"].as<float>();
  bool bad = !strcmp(cal, "bad");
  Nodo& n = nodos[i];
  if (!strcmp(u, "C")) {
    if (!bad) n.temp = v;
    Lote* l = getLote(lid, false);
    if (i == 0 && g2LoteOk(lid) && (!l || !strcmp(cal, "reg") || !loteActivo)) {
      // N1 anuncia continuamente su lote, también cuando el gateway se reinicia.
      l = getLote(lid, true);
      loteActivo = l;
      addLog(String("lote reg ") + lid, ILI9341_GREEN);
      Serial.printf("LOTE reg %s\n", lid);
      for (int j = 1; j <= 4; j++) {
        sendOrder(NODOS[j], "set_lote", lid);
        ultimoSetLote[j] = millis();
      }
    }
    if (i >= 1 && i <= 4 && loteActivo && strcmp(lid, loteActivo->id) && millis() - ultimoSetLote[i] >= 10000) {
      sendOrder(NODOS[i], "set_lote", loteActivo->id);
      ultimoSetLote[i] = millis();
    }
    if (!l) return;
    l->lastSeen = millis();
    if (!bad) { l->t[i] = v; l->tT[i] = millis(); }
    int e = ETAPA_DE[i];
    // Maquina de etapas: recepcion -> hidro (n2 reporta) -> camara (pulpa <=2C) -> anden (min_anden>0)
    if (e == 1 && l->etapa == 0) l->etapa = 1;
    if (e == 1 && l->etapa == 1 && !bad && v <= tMax && v >= tMin && !l->enfriadoTx) {
      l->enfriadoTx = true; l->etapa = 2; sendOrder("n2", "lote_enfriado", l->id);
    }
    if (modo == 'A' && e == l->etapa && fuera(v) && !l->aFlag) { l->aFlag = true; accion(*l, i, "umbral_A", micros()); }
  } else if (!strcmp(u, "C_agua")) { if (!bad) n.tAux = v; }
  else if (!strcmp(u, "puerta")) n.puerta = (int)v;
  else if (!strcmp(u, "min_anden")) { n.tAux = v; Lote* l = getLote(lid, false); if (i == 4 && !bad && l && v > 0 && l->etapa == 2) { l->etapa = 3; l->lastSeen = millis(); } }
  else if (!strcmp(u, "red")) {
    bool r = v > 0.5f;
    if (!r && redOk) {           // perdida de red -> lazo cerrado
      apagonUs = micros(); redOk = false; genWait = true; genT0 = millis(); respaldoFalla = false;
      sendOrder("n6", "gen_start", "apagon");
      gwAlert("proceso", "apagon", "n6", "gen_start", "perdida de red electrica");
      Serial.printf("LAT,energia,gen_start,lat_us=%lu\n", (unsigned long)(micros() - apagonUs));
    } else if (r && !redOk) {
      redOk = true; genWait = false;
      gwAlert("proceso", "red_restablecida", "n6", respaldoFalla ? "reanudar_carga" : "ninguna", "red electrica OK");
      if (respaldoFalla) { sendOrder("n5", "reanudar_carga", "red_ok"); respaldoFalla = false; }
    }
  } else if (!strcmp(u, "grupo")) {
    bool g = v > 0.5f;
    if (g && genWait) { genWait = false; addLog(String("grupo OK ") + (millis() - genT0) / 1000 + "s", ILI9341_GREEN); }
    grupoOn = g;
  }
}

void onMsg(char* topic, uint8_t* p, unsigned int len) {
  char buf[512]; if (len >= sizeof buf) len = sizeof buf - 1;
  memcpy(buf, p, len); buf[len] = 0;                       // copia: publish() reutiliza el buffer
  char tn[16] = "", sub[12] = "";
  if (sscanf(topic, "upc/g2/%15[^/]/%11s", tn, sub) != 2) return;
  if (!strcmp(tn, "gw")) { if (!strcmp(sub, "cmd")) onGwCmd(buf); return; }
  int i = nodeIdx(tn);
  if (i < 0) { secAlert(14, "nodo_desconocido", tn, "descartado", "topico de nodo no registrado"); return; }
  Nodo& n = nodos[i];
  // Flood: ventana de 1 s por nodo
  if (millis() - n.win >= 1000) {
    if (n.drop) { char m[48]; snprintf(m, sizeof m, "descartados %u msg (>%d msg/s)", n.drop, MAX_MSG_S); secAlert(i, "flood", tn, "descarte_exceso", m); }
    n.win = millis(); n.cnt = 0; n.drop = 0;
  }
  if (++n.cnt > MAX_MSG_S) { n.drop++; return; }
  if (!strcmp(sub, "estado")) {                            // LWT/heartbeat (texto plano)
    if (!strcmp(buf, "offline")) { if (n.online) { n.online = false; gwAlert("proceso", "nodo_offline", tn, "modo_seguro_local", "LWT recibido"); } }
    else if (!strcmp(buf, "online")) marcarVivo(i);
    return;
  }
  JsonDocument d; if (deserializeJson(d, buf)) { secAlert(i, "mensaje_malformado", tn, "descartado", "JSON invalido"); return; }
  const char* id = d["id"] | "";
  if (strcmp(id, tn)) { char m[64]; snprintf(m, sizeof m, "id '%s' en topico de %s", id, tn); secAlert(i + 6, "suplantacion", tn, "descartado", m); return; }
  if (!strcmp(sub, "datos")) {
    String mot; if (!verifyNode(i, d, mot)) { secAlert(i + 6, "firma_invalida", tn, "descartado", mot.c_str()); return; }
    marcarVivo(i); onDatos(i, d);
  } else if (!strcmp(sub, "alerta")) {
    // Alertas de nodo {id,tipo,nodo,accion,motivo,ts}: sin firma -> solo informativas (no disparan acciones)
    marcarVivo(i);
    String s = String("[") + tn + "] " + (const char*)(d["tipo"] | "?") + " " + (const char*)(d["accion"] | "");
    addLog(s, ILI9341_YELLOW); Serial.printf("NODO_ALERTA %s %s\n", tn, buf);
  }
}

// ===================== COMANDOS AL GATEWAY (defensa nodo 7) =====================
bool applyKV(const String& kv, String& err) {
  int eq = kv.indexOf('='); if (eq < 1) { err = "formato clave=valor"; return false; }
  String k = kv.substring(0, eq), v = kv.substring(eq + 1); float f = v.toFloat();
  if (k == "umbral_max") { if (f < 0.5 || f > 6) { err = "fuera de rango seguro"; return false; } tMax = f; }
  else if (k == "umbral_min") { if (f < -2 || f > tMax) { err = "fuera de rango seguro"; return false; } tMin = f; }
  else if (k == "presupuesto") { if (f < 30 || f > 180) { err = "fuera de rango seguro"; return false; } budgetMin = f; }
  else if (k == "prealerta") { if (f < 0.5 || f > 0.95) { err = "fuera de rango seguro"; return false; } prePct = f; }
  else if (k == "modo") { if (v != "A" && v != "B") { err = "modo A|B"; return false; } modo = v[0]; Serial.printf("MODO %c\n", modo); }
  else { err = "parametro desconocido"; return false; }
  return true;
}
void doAck(const char* who) { unacked = 0; lastSec = 0; addLog(String("ACK ") + who, ILI9341_WHITE); Serial.printf("ACK %s\n", who); }

void onGwCmd(char* buf) {
  JsonDocument d; if (deserializeJson(d, buf)) return;
  if (String((const char*)(d["id"] | "")) == "gw" && d["dest"].is<const char*>() && String((const char*)d["dest"]) != "gw") return;  // eco propio
  String mot;
  if (!verifyCmd(d, mot)) { secAlert(12, "config_rechazada", "gw", "descartada", mot.c_str()); return; }
  String c = d["cmd"] | "", a = d["arg"] | "";
  if (c == "set") {              // paso 1: queda pendiente de confirmacion
    pend.on = true; pend.kv = a; pend.t = millis();
    gwAlert("seguridad", "config_pendiente", "gw", "esperar_confirmacion_15s", a.c_str());
  } else if (c == "confirm") {   // paso 2: misma clave=valor dentro de 15 s
    if (!pend.on || millis() - pend.t > CONFIRM_MS || a != pend.kv) { pend.on = false; secAlert(12, "config_rechazada", "gw", "descartada", "confirmacion ausente, tardia o distinta"); return; }
    pend.on = false; String err;
    if (applyKV(a, err)) gwAlert("seguridad", "config_aplicada", "gw", "aplicada", a.c_str());
    else secAlert(12, "config_rechazada", "gw", "descartada", err.c_str());
  } else if (c == "gen_stop") {
    if (!redOk) secAlert(15, "orden_insegura", "gw", "bloqueada", "no se apaga el grupo sin red");
    else sendOrder("n6", "gen_stop", "operador");
  } else if (c == "ack") doAck("mqtt");
}

// ===================== RED (no bloqueante) =====================
void netLoop() {
  static uint32_t tw = 0, tm = 0; static bool begun = false;
  if (!begun) { begun = true; g2Wifi(); tw = millis(); }
  if (WiFi.status() != WL_CONNECTED) { if (millis() - tw > 15000) { tw = millis(); WiFi.disconnect(); g2Wifi(); } return; }
  if (!mqtt.connected()) {
    if (millis() - tm < 5000) return; tm = millis();
    g2ConfigureMqtt(onMsg); mqtt.setBufferSize(512);
    String cid = String("g2-gw-") + String((uint32_t)esp_random(), HEX);
    if (mqtt.connect(cid.c_str(), nullptr, nullptr, tpc("estado").c_str(), 0, true, "offline")) {
      mqtt.publish(tpc("estado").c_str(), "online", true);
      mqtt.subscribe("upc/g2/+/datos"); mqtt.subscribe("upc/g2/+/alerta"); mqtt.subscribe("upc/g2/+/estado"); mqtt.subscribe(tpc("cmd").c_str());
      publicarLote();
      Serial.println("MQTT conectado");
    } else Serial.printf("MQTT no conectado (rc=%d)\n", mqtt.state());
    return;
  }
  mqtt.loop(); g2Heartbeat();
}

void thingSpeak() {   // opcional: se omite en silencio sin WAN o sin clave
  static uint32_t t = 0; if (!strlen(TS_WRITE_KEY) || WiFi.status() != WL_CONNECTED || millis() - t < TS_PERIOD_MS) return; t = millis();
  int riesgo = 0; for (auto& l : lotes) if (l.used && (l.pre || l.rup)) riesgo++;
  String url = String("http://api.thingspeak.com/update?api_key=") + TS_WRITE_KEY + "&field1=" + fmtV(nodos[2].temp) + "&field2=" + fmtV(nodos[3].temp) +
               "&field3=" + fmtV(nodos[1].temp) + "&field4=" + riesgo + "&field5=" + nAlert + "&field6=" + (redOk ? 1 : 0);
  WiFiClient c; HTTPClient h; h.setConnectTimeout(1500); h.setTimeout(1500);
  if (h.begin(c, url)) { h.GET(); h.end(); }
}

// ===================== SUPERVISION =====================
void supervise() {
  for (int i = 0; i < N_NODOS; i++) {
    Nodo& n = nodos[i];
    if (n.online && millis() - n.lastSeen > OFFLINE_MS) { n.online = false; gwAlert("proceso", "nodo_offline", NODOS[i], "modo_seguro_local", "sin heartbeat >60 s"); }
  }
  if (genWait && millis() - genT0 > GEN_TIMEOUT_MS) {   // sin confirmacion del grupo
    genWait = false; respaldoFalla = true;
    uint32_t det = micros();
    gwAlert("proceso", "falla_respaldo", "n6", "cerrar_puertas_suspender_carga", "falla del respaldo electrico: grupo sin confirmar en 30 s");
    sendOrder("n3", "cerrar_puerta", "respaldo"); sendOrder("n4", "cerrar_puerta", "respaldo"); sendOrder("n5", "suspender_carga", "respaldo");
    Serial.printf("LAT,energia,falla_respaldo,lat_us=%lu\n", (unsigned long)(micros() - det));
  }
  if (pend.on && millis() - pend.t > CONFIRM_MS) { pend.on = false; secAlert(12, "config_rechazada", "gw", "expirada", "sin segunda confirmacion en 15 s"); }
}

// 0 verde, 1 ambar, 2 rojo
int nivelPlanta() {
  int lv = 0;
  for (auto& l : lotes) if (l.used) { if (l.rup) lv = 2; else if (l.pre || l.aFlag) lv = max(lv, 1); }
  if (respaldoFalla || (lastSec && millis() - lastSec < 60000)) lv = 2;
  if (!redOk || pend.on) lv = max(lv, 1);
  for (auto& n : nodos) if (n.everSeen && !n.online) lv = max(lv, 1);
  return lv;
}
void drawPix() {
  bool blink = unacked && (millis() / 400) % 2;
  for (int i = 0; i < N_NODOS; i++) {
    Nodo& n = nodos[i];
    pix.setPixelColor(i, !n.everSeen ? pix.Color(0, 0, 8) : (n.online ? pix.Color(0, 40, 0) : pix.Color(60, 25, 0)));
  }
  int lv = nivelPlanta(); uint32_t c = lv == 2 ? pix.Color(80, 0, 0) : lv == 1 ? pix.Color(80, 40, 0) : pix.Color(0, 60, 0);
  pix.setPixelColor(6, blink ? 0 : c); pix.setPixelColor(7, blink ? 0 : c); pix.show();
}

void txt(int x, int y, const String& s, uint16_t fg, uint8_t sz = 1, int w = 0) {
  tft.setTextSize(sz); tft.setTextColor(fg, ILI9341_BLACK); tft.setCursor(x, y);
  String o = s; if (w) while ((int)o.length() < w) o += ' '; tft.print(o);
}
String ft(float v) { return isnan(v) ? String(" -- ") : String(v, 1); }
void drawTft() {
  int lv = nivelPlanta(); uint16_t lc = lv == 2 ? ILI9341_RED : lv == 1 ? ILI9341_ORANGE : ILI9341_GREEN;
  tft.fillRect(0, 0, 320, 18, lc);
  tft.setTextColor(ILI9341_BLACK); tft.setTextSize(2); tft.setCursor(4, 1);
  tft.print("G2 GW "); tft.print(hora()); tft.print(" M:"); tft.print(modo);
  tft.print(WiFi.status() == WL_CONNECTED ? (mqtt.connected() ? " MQ" : " WF") : " LOC");
  const char* et[6] = {"REC", "HID", "CA1", "CA2", "AND", "ENE"};
  for (int i = 0; i < N_NODOS; i++) {
    Nodo& n = nodos[i]; int x = 2 + i * 53;
    uint16_t c = !n.everSeen ? ILI9341_DARKGREY : !n.online ? ILI9341_ORANGE : (i < 5 && fuera(n.temp)) ? ILI9341_RED : ILI9341_GREEN;
    txt(x, 22, String(et[i]) + (n.online ? "" : "!"), c, 1, 8);
    if (i < 5) txt(x, 32, ft(n.temp), c, 2, 4);
    else txt(x, 32, redOk ? "RED" : (grupoOn ? "GRP" : "OFF"), redOk ? ILI9341_GREEN : grupoOn ? ILI9341_ORANGE : ILI9341_RED, 2, 3);
    String ex = i == 1 ? "ag " + ft(n.tAux) : (i == 2 || i == 3) ? (n.puerta == 1 ? "PUERTA" : n.puerta == 0 ? "cerr" : "") : i == 4 ? ft(n.tAux) + "m" : "";
    txt(x, 50, ex, ILI9341_LIGHTGREY, 1, 8);
  }
  tft.drawFastHLine(0, 60, 320, ILI9341_DARKGREY);
  char hb[64]; snprintf(hb, sizeof hb, "Lote         Etapa  Fuera/Ppto  REC  HID  CAM  AND  [%.1f-%.1fC]", tMin, tMax);
  txt(2, 64, hb, ILI9341_WHITE, 1, 53);
  int y = 76;
  for (auto& l : lotes) {
    if (!l.used || y > 130) continue;
    uint16_t c = l.rup ? ILI9341_RED : (l.pre || l.aFlag) ? ILI9341_ORANGE : ILI9341_GREEN;
    char b[80]; snprintf(b, sizeof b, "%-12s %-6.6s %4.0f/%-4.0f %4.0f %4.0f %4.0f %4.0f", l.id, ETAPAS[l.etapa], l.minTot, budgetMin, l.minEt[0], l.minEt[1], l.minEt[2], l.minEt[3]);
    txt(2, y, b, c, 1, 53); y += 11;
  }
  while (y <= 130) { txt(2, y, "", ILI9341_WHITE, 1, 53); y += 11; }
  tft.drawFastHLine(0, 142, 320, ILI9341_DARKGREY);
  for (int i = 0; i < NLOG; i++) txt(2, 148 + i * 12, logL[i], logL[i].length() ? logC[i] : ILI9341_WHITE, 1, 53);
  char f[64]; snprintf(f, sizeof f, "Alertas:%d sin ACK:%d  A:%d B:%d %s", nAlert, unacked, nAlertA, nAlertB, genWait ? "ESPERA GRUPO" : (respaldoFalla ? "FALLA RESPALDO" : ""));
  txt(2, 214, f, unacked ? ILI9341_YELLOW : ILI9341_LIGHTGREY, 1, 53);
  txt(2, 226, rtcOk ? "RTC DS1307 OK   [boton = ACK]" : "RTC ausente: hora=uptime   [boton = ACK]", ILI9341_LIGHTGREY, 1, 53);
}

// ===================== SETUP / LOOP =====================
void setup() {
  Serial.begin(115200);
  pinMode(PIN_ACK, INPUT_PULLUP);
  prefs.begin("g2gw", false); gwCtr = prefs.getUInt("ctr", 0); ctrReserved = gwCtr;   // salta el bloque reservado previo
  Wire.begin(21, 22);
  rtcOk = rtc.begin();
  if (rtcOk && !rtc.isrunning()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // Contador >= hora RTC: sigue creciendo aunque se pierda la NVS (p. ej. reinicio del gateway en Wokwi con nodos vivos)
  if (rtcOk && (uint32_t)rtc.now().unixtime() > gwCtr) { gwCtr = rtc.now().unixtime(); ctrReserved = gwCtr; }
  Serial.printf("GW inicio RTC=%s ctr=%lu modo=%c SIM_X=%d\n", rtcOk ? "ok" : "ausente", (unsigned long)gwCtr, modo, SIM_X);
  pix.begin(); pix.setBrightness(80); pix.clear(); pix.show();
  tft.begin(); tft.setRotation(1); tft.fillScreen(ILI9341_BLACK);
  addLog("gateway iniciado (decision local)", ILI9341_WHITE);
}

void loop() {
  delay(10);  // cede CPU entre lecturas al simular varios ESP32
  netLoop();
  static uint32_t tTick = millis(), tDraw = 0, tPix = 0, tBtn = 0, tLote = 0; static bool lastBtn = HIGH;
  uint32_t now = millis();
  if (now - tTick >= 1000) { tickLotes(now - tTick); tTick = now; supervise(); }
  if (now - tLote >= 2000) { tLote = now; publicarLote(); }
  bool b = digitalRead(PIN_ACK);
  if (b == LOW && lastBtn == HIGH && now - tBtn > 250) { tBtn = now; doAck("boton");
    gwAlert("operador", "ack", "gw", "alertas_reconocidas", "boton local"); unacked = 0; }
  lastBtn = b;
  if (now - tPix > 100) { tPix = now; drawPix(); }
  if (now - tDraw > 1000) { tDraw = now; drawTft(); }
  thingSpeak();
}
