// Contrato comun G2 (copiado a cada nodo por tools/sync.sh)
// Topicos: upc/g2/<nodo>/{datos,alerta,estado,cmd}
// Mensaje: {id,lote,ts,valor,unidad,calidad,firma}
// firma = HMAC-SHA256(key, "id|lote|ts|valor|unidad|calidad") en hex (16 primeros bytes)
// Orden:  {id:"gw",dest,cmd,arg,ctr,ts,firma} firma = HMAC(GW_KEY,"gw|dest|cmd|arg|ctr|ts")
#pragma once
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "mbedtls/md.h"
#include <Preferences.h>
#include "secrets.h"

#ifndef NODE_ID
#define NODE_ID "n0"
#endif

static WiFiClient g2wifi;
static PubSubClient mqtt(g2wifi);
static String tpc(const char* sub, const char* nodo = NODE_ID) { return String("upc/g2/") + nodo + "/" + sub; }

static String hmacHex(const char* key, const String& msg) {
  uint8_t out[32];
  mbedtls_md_context_t c; mbedtls_md_init(&c);
  mbedtls_md_setup(&c, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&c, (const unsigned char*)key, strlen(key));
  mbedtls_md_hmac_update(&c, (const unsigned char*)msg.c_str(), msg.length());
  mbedtls_md_hmac_finish(&c, out); mbedtls_md_free(&c);
  char h[33]; for (int i = 0; i < 16; i++) sprintf(h + 2 * i, "%02x", out[i]);
  return String(h);
}

static bool ctEq(const String& a, const String& b) {   // comparacion en tiempo constante
  if (a.length() != b.length()) return false;
  uint8_t d = 0; for (size_t i = 0; i < a.length(); i++) d |= a[i] ^ b[i];
  return d == 0;
}

static String fmtV(float v) { char b[16]; snprintf(b, sizeof b, "%.2f", v); return String(b); }

// Publica una lectura firmada (solo nodos; el gateway usa la misma funcion con su clave)
static void pubSigned(const char* sub, const char* lote, float valor, const char* unidad, const char* calidad, const char* key = NODE_KEY) {
  String ts = String((unsigned long)(millis() / 1000));
  String v = fmtV(valor);
  String sig = hmacHex(key, String(NODE_ID) + "|" + lote + "|" + ts + "|" + v + "|" + unidad + "|" + calidad);
  JsonDocument d;
  d["id"] = NODE_ID; d["lote"] = lote; d["ts"] = ts.toInt(); d["valor"] = v; d["unidad"] = unidad; d["calidad"] = calidad; d["firma"] = sig;
  char buf[256]; size_t n = serializeJson(d, buf, sizeof buf);
  mqtt.publish(tpc(sub).c_str(), (const uint8_t*)buf, n, false);
}

// Valida una orden del gateway: firma + contador estrictamente creciente (anti-replay)
// lastCtr se guarda en NVS: un reinicio del nodo no reabre la ventana de replay
static uint32_t lastCtr = 0; static bool ctrCargado = false;
static bool verifyCmd(JsonDocument& d, String& motivo) {
  if (!ctrCargado) { Preferences p; p.begin("g2n", true); lastCtr = p.getUInt("ctr", 0); p.end(); ctrCargado = true; }
  String dest = d["dest"] | "", cmd = d["cmd"] | "", arg = d["arg"] | "", ts = String((long)(d["ts"] | 0L));
  uint32_t ctr = d["ctr"] | 0UL; String sig = d["firma"] | "";
  if (String((const char*)(d["id"] | "")) != "gw") { motivo = "emisor no es gw"; return false; }
  if (dest != NODE_ID && dest != "all") { motivo = "destino ajeno"; return false; }
  String esperado = hmacHex(GW_KEY, "gw|" + dest + "|" + cmd + "|" + arg + "|" + String(ctr) + "|" + ts);
  if (!ctEq(esperado, sig)) { motivo = "firma invalida"; return false; }
  if (ctr <= lastCtr) { motivo = "contador repetido (replay)"; return false; }
  lastCtr = ctr; Preferences p; p.begin("g2n", false); p.putUInt("ctr", ctr); p.end(); return true;
}

// Formato de lote L-AAMMDD-NN
static bool g2LoteOk(const String& s) {
  if (s.length() != 11 || s[0] != 'L' || s[1] != '-' || s[8] != '-') return false;
  for (int i : {2,3,4,5,6,7,9,10}) if (!isDigit(s[i])) return false;
  return true;
}

static void g2Alert(const char* tipo, const char* nodo, const char* accion, const char* motivo) {
  JsonDocument d; d["id"] = NODE_ID; d["tipo"] = tipo; d["nodo"] = nodo; d["accion"] = accion; d["motivo"] = motivo; d["ts"] = (long)(millis() / 1000);
  char buf[256]; size_t n = serializeJson(d, buf, sizeof buf);
  mqtt.publish(tpc("alerta").c_str(), (const uint8_t*)buf, n, false);
}

// Wi-Fi no bloqueante: solo inicia la conexion; g2Reconnect reintenta cada 15 s sin esperar
static void g2Wifi() { WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASS, strcmp(WIFI_SSID, "Wokwi-GUEST") == 0 ? 6 : 0); }
static const char* g2ExtraSub = nullptr;   // topico adicional opcional (p. ej. estado del gateway)

static void g2ConfigureMqtt(MQTT_CALLBACK_SIGNATURE) {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(callback);
  mqtt.setSocketTimeout(3);
  // El broker usa tiempo real aunque Wokwi ejecute el ESP32 más lento.
  mqtt.setKeepAlive(90);
}

// Conexion no bloqueante con LWT "offline"; el nodo sigue decidiendo localmente si no hay red
static void g2Reconnect(MQTT_CALLBACK_SIGNATURE) {
  static uint32_t last = 0, tw = 0; if (mqtt.connected() || millis() - last < 3000) return; last = millis();
  if (WiFi.status() != WL_CONNECTED) { if (millis() - tw > 15000) { tw = millis(); WiFi.disconnect(); g2Wifi(); } return; }
  g2ConfigureMqtt(callback);
  String cid = String("g2-") + NODE_ID + "-" + String((uint32_t)esp_random(), HEX);
  if (mqtt.connect(cid.c_str(), nullptr, nullptr, tpc("estado").c_str(), 0, true, "offline")) {
    mqtt.publish(tpc("estado").c_str(), "online", true);
    mqtt.subscribe(tpc("cmd").c_str());
    if (g2ExtraSub) mqtt.subscribe(g2ExtraSub);
    Serial.println("MQTT conectado");
  } else Serial.printf("MQTT no conectado (rc=%d)\n", mqtt.state());
}
static void g2Heartbeat() { static uint32_t t = 0; if (mqtt.connected() && millis() - t >= 5000) { t = millis(); mqtt.publish(tpc("estado").c_str(), "online", true); } }
