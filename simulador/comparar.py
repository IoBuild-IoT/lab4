#!/usr/bin/env python3
"""Comparacion A (edge: umbral instantaneo) vs B (fog: minutos acumulados por lote).
Modelo de un lote (inercia termica) recorriendo 4 etapas; inyecta aperturas de puerta,
esperas en anden, apagones y lecturas falsas. Mide deteccion/localizacion/falsas alarmas.
Uso: python3 comparar.py [--limite 20] [--alerta-pct 0.8] [--umbral 2.0] [--runs 300] [--seed 1]
Las latencias de red/actuacion son parametros del modelo (reemplazar por las medidas en Wokwi)."""
import argparse, json, random, statistics as st, os

ETAPAS = ["recepcion", "hidro", "camara", "anden"]
p = argparse.ArgumentParser()
p.add_argument("--limite", type=float, default=20, help="min fuera de rango (>umbral) tolerados por lote")
p.add_argument("--alerta-pct", type=float, default=0.8, help="B alerta al x%% del limite")
p.add_argument("--umbral", type=float, default=2.0, help="limite superior de temperatura (C)")
p.add_argument("--umbral-anden", type=float, default=10.0, help="umbral de A en el anden (zona no frigorifica)")
p.add_argument("--runs", type=int, default=300)
p.add_argument("--seed", type=int, default=1)
p.add_argument("--lat-edge", type=float, default=2.1, help="s: muestreo 2 s + actuacion local")
p.add_argument("--lat-fog", type=float, default=3.0, help="s: muestreo 2 s + MQTT + decision gateway + orden")
p.add_argument("--tau", type=float, default=90, help="constante termica del pallet (min)")
p.add_argument("--ruido", type=float, default=0.25, help="sigma ruido sensor aire (C)")
a = p.parse_args()
rng = random.Random(a.seed)

def escenario(tipo):
    """Devuelve lista de (etapa, minuto_inicio_en_corrida, duracion_min, T_ambiente_etapa) - cada etapa dura 60 min"""
    base = {"recepcion": 25, "hidro": 1.0, "camara": 1.0, "anden": 8.0}
    ev = []
    if tipo == "puerta_breve":      # falsa amenaza: apertura de 1-2 min, no compromete el lote
        ev.append(("camara", rng.randint(150, 200), rng.uniform(1, 2.5), 10.0))
    elif tipo == "puerta_larga":    # ruptura real: puerta abierta 25-45 min
        ev.append(("camara", rng.randint(130, 180), rng.uniform(25, 45), 16.0))
    elif tipo == "espera_anden":    # ruptura real: pallet espera en anden con 38C ambiente
        ev.append(("anden", rng.randint(185, 200), rng.uniform(25, 45), 32.0))
    elif tipo == "apagon":          # ruptura real: apagon sin respaldo efectivo
        ev.append(("camara", rng.randint(130, 180), rng.uniform(25, 40), 12.0))
    elif tipo == "lectura_falsa":   # picos espurios del sensor, lote sano
        ev = []
    return base, ev

def corrida(tipo, modo_ruido=True):
    base, ev = escenario(tipo)
    T = 12.0 if False else 1.0     # lote ya enfriado entra a camara (inicio en rango)
    tl = 1.0                        # temp de pulpa
    ac = 0.0                        # minutos fuera de rango acumulados (verdad)
    acc_et = {e: 0.0 for e in ETAPAS}
    A = {"alerta_min": None, "etapa": None}
    B = {"alerta_min": None, "etapa": None}
    breach_min = None; breach_et = None; consec = 0
    for minuto in range(0, 4 * 60):
        et = ETAPAS[min(3, minuto // 60)]
        if et in ("recepcion", "hidro"):     # fase de enfriado: el lote baja de 25 a ~1 (no cuenta como ruptura)
            tl = max(1.0, tl + (base["hidro"] - tl) * 0.07) if minuto else 25.0
            if minuto == 0: tl = 25.0
            continue
        amb = base[et]; en_evento = False
        if et == 'anden' and minuto - 180 >= 10: amb = 1.0   # espera normal 10 min; luego carga al contenedor
        for (e, m0, d, tev) in ev:
            if e == et and m0 <= minuto < m0 + d: amb = tev; en_evento = True
        # inercia del lote (tau parametrico) vs aire (tau ~ 4 min, medido por sensor del nodo)
        tl += (amb - tl) * (1 / a.tau)
        aire = amb + rng.gauss(0, a.ruido) if modo_ruido else amb
        if tipo == "lectura_falsa" and rng.random() < 0.03: aire += rng.uniform(2.5, 6)   # spike espurio
        # ----- verdad
        if tl > a.umbral:
            ac += 1; acc_et[et] += 1
            if breach_min is None and ac >= a.limite: breach_min = minuto; breach_et = max(acc_et, key=acc_et.get)
        # ----- A: umbral instantaneo sobre aire del nodo (sin memoria, un solo dato)
        if A["alerta_min"] is None and aire > (a.umbral if et == 'camara' else a.umbral_anden): A = {"alerta_min": minuto, "etapa": et}
        # ----- B: acumula minutos de temperatura de PULPA del lote (2 lecturas consecutivas para ruido)
        consec = consec + 1 if tl + rng.gauss(0, 0.1) > a.umbral else 0
        if B["alerta_min"] is None and ac >= a.limite * a.alerta_pct and consec >= 2:
            B = {"alerta_min": minuto, "etapa": max(acc_et, key=acc_et.get)}
    real = breach_min is not None
    return real, breach_min, breach_et, A, B

TIPOS = ["puerta_breve", "puerta_larga", "espera_anden", "apagon", "lectura_falsa"]
res = {"parametros": vars(a), "escenarios": {}, "resumen": {}}
tot = {"A": {"tp": 0, "fn": 0, "fp": 0, "loc_ok": 0, "loc_n": 0, "lead": [], "lat": []}, "B": {"tp": 0, "fn": 0, "fp": 0, "loc_ok": 0, "loc_n": 0, "lead": [], "lat": []}}
for t in TIPOS:
    fila = {"A": {"tp": 0, "fn": 0, "fp": 0, "loc_ok": 0, "loc_n": 0, "lead": []}, "B": {"tp": 0, "fn": 0, "fp": 0, "loc_ok": 0, "loc_n": 0, "lead": []}, "ruptura_real": 0}
    for _ in range(a.runs):
        real, bm, be, A, B = corrida(t)
        fila["ruptura_real"] += real
        for nom, r, lat in (("A", A, a.lat_edge), ("B", B, a.lat_fog)):
            d = fila[nom]; alert = r["alerta_min"] is not None
            if real and alert and r["alerta_min"] <= bm: d["tp"] += 1; d["lead"].append(bm - r["alerta_min"]); d["loc_n"] += 1; d["loc_ok"] += (r["etapa"] == be); tot[nom]["lat"].append(lat)
            elif real and (not alert or r["alerta_min"] > bm): d["fn"] += 1
            elif not real and alert: d["fp"] += 1
    res["escenarios"][t] = {k: ({**v, "lead_prom_min": round(st.mean(v["lead"]), 1) if v["lead"] else None} if isinstance(v, dict) else v) for k, v in fila.items()}
    for nom in "AB":
        for k in ("tp", "fn", "fp", "loc_ok", "loc_n"): tot[nom][k] += fila[nom][k]
        tot[nom]["lead"] += fila[nom]["lead"]
for nom in "AB":
    d = tot[nom]; n_real = d["tp"] + d["fn"]; n_sano = a.runs * len(TIPOS) - n_real
    res["resumen"][nom] = {
        "deteccion_pct": round(100 * d["tp"] / max(1, n_real), 1),
        "falsas_alarmas_pct": round(100 * d["fp"] / max(1, n_sano), 1),
        "localizacion_pct": round(100 * d["loc_ok"] / max(1, d["loc_n"]), 1),
        "anticipacion_prom_min": round(st.mean(d["lead"]), 1) if d["lead"] else None,
        "latencia_deteccion_actuacion_s": round(st.mean(d["lat"]), 2) if d["lat"] else None,
        "tp": d["tp"], "fn": d["fn"], "fp": d["fp"]}
    for k in list(res["escenarios"].values()):
        k[nom].pop("lead", None)
os.makedirs(os.path.join(os.path.dirname(__file__), "resultados"), exist_ok=True)
out = os.path.join(os.path.dirname(__file__), "resultados", "resultados.json")
json.dump(res, open(out, "w"), indent=1, ensure_ascii=False)
print(json.dumps(res["resumen"], indent=1)); print("->", out)
open(os.path.join(os.path.dirname(__file__), "..", "dashboard", "resultados.js"), "w").write("window.G2_RES = " + json.dumps(res, ensure_ascii=False) + ";")
