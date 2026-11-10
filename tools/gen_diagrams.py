#!/usr/bin/env python3
"""Genera diagram.json (Wokwi) y libraries.txt de los nodos 1-6. Nodo 7 lo genera su propio autor.
Placa board-esp32-devkit-c-v4: los GPIO se nombran con el numero ("esp:4"); "esp:D2"/"esp:D3" son pines de flash."""
import json, os
ROOT = os.path.join(os.path.dirname(__file__), "..", "nodos")
GND, V3 = "esp:GND.1", "esp:3V3"
def part(t, i, x, y, **a): return {"type": t, "id": i, "top": y, "left": x, "attrs": a}
def w(a, b, col="green"): return [a, b, col, []]
def esp(): return part("board-esp32-devkit-c-v4", "esp", 0, 0)
SERIAL = [w("esp:TX", "$serialMonitor:RX", ""), w("esp:RX", "$serialMonitor:TX", "")]
_dev = iter(range(1, 99))
def ds(i, x, y, pin, temp="22"):  # DS18B20 + pull-up 4.7k; deviceID unico (obligatorio si comparten bus)
    return [part("wokwi-ds18b20", i, x, y, deviceID=f"0102030405{next(_dev):02x}", temperature=temp), part("wokwi-resistor", "r_"+i, x+60, y-40, value="4700")], \
           [w(f"{i}:GND", GND, "black"), w(f"{i}:VCC", V3, "red"), w(f"{i}:DQ", f"esp:{pin}", "green"),
            w(f"r_{i}:1", f"{i}:VCC", "red"), w(f"r_{i}:2", f"{i}:DQ", "green")]
def sw(i, x, y, pin): return [part("wokwi-slide-switch", i, x, y)], [w(f"{i}:1", GND, "black"), w(f"{i}:2", f"esp:{pin}", "blue")]
def btn(i, x, y, pin): return [part("wokwi-pushbutton", i, x, y)], [w(f"{i}:1.l", f"esp:{pin}", "blue"), w(f"{i}:2.l", GND, "black")]
def rgb(i, x, y, r, g, b):
    return [part("wokwi-rgb-led", i, x, y)], [w(f"{i}:COM", GND, "black"), w(f"{i}:R", f"esp:{r}", "red"), w(f"{i}:G", f"esp:{g}", "green"), w(f"{i}:B", f"esp:{b}", "blue")]
def relay(i, x, y, pin): return [part("wokwi-relay-module", i, x, y)], [w(f"{i}:VCC", "esp:5V", "red"), w(f"{i}:GND", GND, "black"), w(f"{i}:IN", f"esp:{pin}", "orange")]
def servo(i, x, y, pin): return [part("wokwi-servo", i, x, y)], [w(f"{i}:GND", GND, "black"), w(f"{i}:V+", "esp:5V", "red"), w(f"{i}:PWM", f"esp:{pin}", "orange")]
def dht(i, x, y, pin): return [part("wokwi-dht22", i, x, y)], [w(f"{i}:VCC", V3, "red"), w(f"{i}:GND", GND, "black"), w(f"{i}:SDA", f"esp:{pin}", "green")]
def buzzer(i, x, y, pin): return [part("wokwi-buzzer", i, x, y, volume="0.1")], [w(f"{i}:1", GND, "black"), w(f"{i}:2", f"esp:{pin}", "red")]
def pir(i, x, y, pin): return [part("wokwi-pir-motion-sensor", i, x, y)], [w(f"{i}:VCC", "esp:5V", "red"), w(f"{i}:GND", GND, "black"), w(f"{i}:OUT", f"esp:{pin}", "green")]
def led(i, x, y, pin):
    return [part("wokwi-led", i, x, y, color="green"), part("wokwi-resistor", "r_"+i, x, y+60, value="220")], \
           [w(f"{i}:A", f"r_{i}:1", "green"), w(f"r_{i}:2", f"esp:{pin}", "green"), w(f"{i}:C", GND, "black")]
def build(name, blocks, libs):
    parts, conns = [esp()], list(SERIAL)
    for p, c in blocks: parts += p; conns += c
    d = os.path.join(ROOT, name); os.makedirs(d, exist_ok=True)
    json.dump({"version": 1, "author": "G2", "editor": "wokwi", "parts": parts, "connections": conns, "dependencies": {}}, open(f"{d}/diagram.json", "w"), indent=1)
    open(f"{d}/libraries.txt", "w").write("\n".join(["PubSubClient", "ArduinoJson"] + libs) + "\n")
    print("ok", name, len(parts), "partes")
build("n1_recepcion", [ds("t1", 160, -80, 4), btn("b1", 160, 60, 5), rgb("l1", 280, 0, 25, 26, 27)], ["OneWire", "DallasTemperature"])
build("n2_hidroenfriador", [ds("t1", 160, -80, 4, "3"), ds("t2", 160, 60, 16), relay("k1", 300, 0, 26)], ["OneWire", "DallasTemperature"])
build("n3_camara1", [dht("h1", 160, -80, 4), sw("s1", 160, 60, 14), buzzer("z1", 300, 0, 27), servo("sv", 300, 100, 13)], ["DHT sensor library", "ESP32Servo"])
build("n4_camara2", [dht("h1", 160, -80, 4), sw("s1", 160, 60, 14), relay("k1", 300, 0, 26), servo("sv", 300, 100, 13), led("a1", 420, 0, 2)], ["DHT sensor library", "ESP32Servo"])
build("n5_anden", [ds("t1", 160, -80, 4), pir("p1", 160, 60, 27), rgb("l1", 300, 0, 25, 26, 33)], ["OneWire", "DallasTemperature"])
build("n6_energia", [sw("s1", 160, -40, 14), sw("s2", 160, 60, 12), relay("k1", 300, 0, 26), led("e1", 420, 0, 27)], [])
