#!/usr/bin/env python3
"""Pruebas en host (solo stdlib) del contrato G2: formatos HMAC de g2_common.h y del gateway,
anti-replay por contador, limitador de inundacion, formato de lote y coherencia diagram.json <-> .ino.
Uso: python3 tests/test_contract.py"""
import hashlib, hmac, json, os, re, unittest

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
NODOS = os.path.join(ROOT, "nodos")
GW_KEY, KEYS = "gw-demo-key-cambiar", {f"n{i}": f"key-n{i}-cambiar" for i in range(1, 7)}   # = secrets.example.h


# ---------- reimplementacion de g2_common.h ----------
def hmac_hex(key, msg):                       # hmacHex: 16 primeros bytes en hex
    return hmac.new(key.encode(), msg.encode(), hashlib.sha256).digest()[:16].hex()

def fmt_v(v):                                 # fmtV: "%.2f"
    return "%.2f" % v

def pub_signed(nodo, key, lote, ts, valor, unidad, calidad):   # pubSigned -> JSON publicado
    v = fmt_v(valor)
    sig = hmac_hex(key, f"{nodo}|{lote}|{ts}|{v}|{unidad}|{calidad}")
    return json.dumps({"id": nodo, "lote": lote, "ts": ts, "valor": v, "unidad": unidad, "calidad": calidad, "firma": sig})

class Nodo:                                   # verifyCmd (lastCtr persistente en NVS)
    def __init__(self, nid, nvs=None):
        self.nid, self.nvs = nid, nvs if nvs is not None else {}
        self.last = self.nvs.get("ctr", 0)
    def verify(self, raw):
        d = json.loads(raw)
        dest, cmd, arg, ctr, ts = d.get("dest", ""), d.get("cmd", ""), d.get("arg", ""), int(d.get("ctr", 0)), int(d.get("ts", 0))
        if d.get("id") != "gw": return False, "emisor no es gw"
        if dest not in (self.nid, "all"): return False, "destino ajeno"
        if not hmac.compare_digest(hmac_hex(GW_KEY, f"gw|{dest}|{cmd}|{arg}|{ctr}|{ts}"), d.get("firma", "")): return False, "firma invalida"
        if ctr <= self.last: return False, "contador repetido (replay)"
        self.last = self.nvs["ctr"] = ctr
        return True, ""

# ---------- reimplementacion del gateway (n7) ----------
class MockGateway:
    """sendOrder/nextCtr/verifyNode del gateway; 'publica' en una lista (sin red)."""
    def __init__(self, nvs=None, rtc=None):
        self.nvs = nvs if nvs is not None else {}
        self.ctr = self.nvs.get("ctr", 0); self.res = self.ctr
        if rtc and rtc > self.ctr: self.ctr = self.res = rtc        # contador >= hora RTC
        self.out, self.last = [], {}
    def next_ctr(self):
        self.ctr += 1
        if self.ctr >= self.res: self.res = self.ctr + 64; self.nvs["ctr"] = self.res
        return self.ctr
    def send_order(self, dest, cmd, arg, ts=1000):
        ctr = self.next_ctr()
        sig = hmac_hex(GW_KEY, f"gw|{dest}|{cmd}|{arg}|{ctr}|{ts}")
        msg = json.dumps({"id": "gw", "dest": dest, "cmd": cmd, "arg": arg, "ctr": ctr, "ts": ts, "firma": sig})
        self.out.append((f"upc/g2/{dest}/cmd", msg)); return msg
    def verify_node(self, nodo, raw):
        d = json.loads(raw)
        v = d["valor"] if isinstance(d["valor"], str) else fmt_v(d["valor"])
        ts = int(d["ts"]); m = f"{nodo}|{d.get('lote', '')}|{ts}|{v}|{d.get('unidad', '')}|{d.get('calidad', '')}"
        if not hmac.compare_digest(hmac_hex(KEYS[nodo], m), d.get("firma", "")): return False
        lt, ls = self.last.get(nodo, (-1, ""))
        if ls == d["firma"] or (ts < lt - 5 and ts > 60): return False
        self.last[nodo] = (ts, d["firma"]); return True

def lote_ok(s):                               # g2LoteOk / formatoOk de n1
    return bool(re.fullmatch(r"L-\d{6}-\d{2}", s))

class Flood:                                  # n5: max 5 msg/s, ventana de 1 s
    def __init__(self): self.v, self.c, self.drop = -10**9, 0, 0
    def accept(self, ms):
        if ms - self.v > 1000: self.v, self.c, self.drop = ms, 0, 0
        self.c += 1
        if self.c > 5: self.drop += 1; return False
        return True


class TestHmac(unittest.TestCase):
    def test_vector_fijo(self):   # vector calculado con hmac/hashlib; cualquier cambio de formato lo rompe
        self.assertEqual(hmac_hex(KEYS["n1"], "n1|L-251103-01|5|3.20|C|ok"), "8019e04c57713456db7ae29ed52c45fc")
        self.assertEqual(len(hmac_hex("k", "x")), 32)
    def test_nodo_a_gateway(self):
        gw = MockGateway()
        for n in KEYS:
            raw = pub_signed(n, KEYS[n], "L-251103-01" if n != "n6" else "-", 42, 1.005 if n == "n1" else 0, "C", "ok")
            self.assertTrue(gw.verify_node(n, raw), n)
    def test_suplantacion_y_alteracion(self):
        gw = MockGateway()
        raw = pub_signed("n3", KEYS["n4"], "L-251103-01", 7, -5, "C", "ok")      # firmado con clave ajena
        self.assertFalse(gw.verify_node("n3", raw))
        d = json.loads(pub_signed("n3", KEYS["n3"], "L-251103-01", 8, 1.5, "C", "ok")); d["valor"] = "9.50"
        self.assertFalse(gw.verify_node("n3", json.dumps(d)))
    def test_valor_numerico_tambien_verifica(self):   # el gateway reformatea con %.2f si llega como numero
        gw = MockGateway(); d = json.loads(pub_signed("n2", KEYS["n2"], "L-251103-01", 9, 2.5, "C", "ok")); d["valor"] = 2.5
        self.assertTrue(gw.verify_node("n2", json.dumps(d)))
    def test_replay_de_datos(self):
        gw = MockGateway(); raw = pub_signed("n5", KEYS["n5"], "L-251103-01", 100, 3, "C", "ok")
        self.assertTrue(gw.verify_node("n5", raw)); self.assertFalse(gw.verify_node("n5", raw))


class TestOrdenes(unittest.TestCase):
    def test_gateway_a_nodo(self):
        gw, n4 = MockGateway(), Nodo("n4")
        self.assertEqual(n4.verify(gw.send_order("n4", "fan_on", "L-251103-01")), (True, ""))
    def test_replay_y_destino(self):
        gw, n4 = MockGateway(), Nodo("n4"); m = gw.send_order("n4", "fan_off", "")
        self.assertTrue(n4.verify(m)[0]); self.assertEqual(n4.verify(m)[1], "contador repetido (replay)")
        self.assertEqual(Nodo("n3").verify(gw.send_order("n4", "x", ""))[1], "destino ajeno")
    def test_firma_falsa_dashboard(self):
        fake = json.dumps({"id": "gw", "dest": "n6", "cmd": "gen_stop", "arg": "", "ctr": 9997, "ts": 1, "firma": "0" * 32})
        self.assertEqual(Nodo("n6").verify(fake)[1], "firma invalida")
    def test_replay_tras_reinicio_del_nodo(self):   # lastCtr en NVS
        nvs, gw = {}, MockGateway(); vieja = gw.send_order("n6", "gen_start", "apagon"); gw.send_order("n6", "gen_stop", "")
        n6 = Nodo("n6", nvs); n6.verify(vieja); n6.verify(gw.out[-1][1])
        self.assertFalse(Nodo("n6", nvs).verify(vieja)[0])          # reinicio: la orden vieja sigue rechazada
    def test_contador_gateway_monotono_tras_reinicio(self):
        nvs = {}; gw = MockGateway(nvs); a = [gw.next_ctr() for _ in range(70)][-1]
        self.assertGreater(MockGateway(nvs).next_ctr(), a)            # con NVS (bloques de 64)
        g1 = MockGateway({}, rtc=1_790_000_000); c1 = g1.next_ctr()
        self.assertGreater(MockGateway({}, rtc=1_790_000_010).next_ctr(), c1)   # sin NVS (Wokwi): por RTC
        self.assertLess(c1, 2**31)                                    # cabe en el int de ArduinoJson (d["ctr"] | 0)


class TestReglas(unittest.TestCase):
    def test_lote(self):
        for ok in ("L-251103-01", "L-000000-99"): self.assertTrue(lote_ok(ok))
        for bad in ("HACK!", "L-251103-1", "X-251103-01", "L-25110A-01", "-", ""): self.assertFalse(lote_ok(bad))
    def test_flood(self):
        f = Flood(); r = [f.accept(t) for t in range(0, 200, 10)]   # 20 msg en 200 ms
        self.assertEqual(sum(r), 5); self.assertTrue(f.accept(1500))


# ---------- diagram.json <-> .ino ----------
PINS = {
    "wokwi-ds18b20": {"GND", "DQ", "VCC"}, "wokwi-dht22": {"VCC", "SDA", "NC", "GND"}, "wokwi-slide-switch": {"1", "2", "3"},
    "wokwi-pushbutton": {"1.l", "2.l", "1.r", "2.r"}, "wokwi-rgb-led": {"R", "COM", "G", "B"},
    "wokwi-relay-module": {"VCC", "GND", "IN", "NC", "COM", "NO"}, "wokwi-servo": {"GND", "V+", "PWM"}, "wokwi-buzzer": {"1", "2"},
    "wokwi-pir-motion-sensor": {"VCC", "OUT", "GND"}, "wokwi-led": {"A", "C"}, "wokwi-resistor": {"1", "2"},
    "wokwi-ili9341": {"VCC", "GND", "CS", "RST", "D/C", "MOSI", "SCK", "LED", "MISO"}, "wokwi-ds1307": {"GND", "5V", "SDA", "SCL", "SQW"},
    "wokwi-led-ring": {"GND", "VCC", "DIN", "DOUT"},
    # github.com/wokwi/wokwi-boards boards/esp32-devkit-c-v4/board.json
    "board-esp32-devkit-c-v4": set("3V3 EN VP VN 34 35 32 33 25 26 27 14 12 GND.1 13 D2 D3 CMD 5V GND.2 23 22 TX RX 21 GND.3 19 18 5 17 16 4 0 2 15 D1 D0 CLK".split()),
    "wokwi-esp32-devkit-v1": set("EN VP VN D34 D35 D32 D33 D25 D26 D27 D14 D12 D13 GND.2 VIN 3V3 GND.1 D15 D2 D4 RX2 TX2 D5 D18 D19 D21 RX0 TX0 D22 D23".split()),
}
GPIO_PREFIX = {"board-esp32-devkit-c-v4": "", "wokwi-esp32-devkit-v1": "D"}

class TestDiagramas(unittest.TestCase):
    def test_partes_pines_y_cableado(self):
        for n in sorted(os.listdir(NODOS)):
            with open(os.path.join(NODOS, n, "diagram.json")) as f: d = json.load(f)
            tipos = {p["id"]: p["type"] for p in d["parts"]}
            ids = [p["deviceID"] for p in [q.get("attrs", {}) for q in d["parts"] if q["type"] == "wokwi-ds18b20"]]
            self.assertEqual(len(ids), len(set(ids)), f"{n}: deviceID DS18B20 repetido")
            net = set()
            for a, b, *_ in d["connections"]:
                for e in (a, b):
                    pid, pin = e.split(":", 1)
                    if pid.startswith("$"): continue
                    self.assertIn(pid, tipos, f"{n}: parte inexistente {e}")
                    self.assertIn(pin, PINS[tipos[pid]], f"{n}: pin invalido {e} ({tipos[pid]})")
                    if pid == "esp": net.add(pin)
            with open(os.path.join(NODOS, n, n + ".ino")) as f: ino = f.read()
            pre = GPIO_PREFIX[tipos["esp"]]
            used = re.findall(r"#define\s+(?:PIN_\w+|TFT_CS|TFT_DC|TFT_RST)\s+(\d+)", ino)
            for g in used: self.assertIn(pre + g, net, f"{n}: GPIO {g} usado en .ino y no cableado")


if __name__ == "__main__":
    unittest.main(verbosity=2)
