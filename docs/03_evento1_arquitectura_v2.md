# Evento 1 — Ola de calor, lluvia, apagón de 8 h, internet caído, sensores bloqueados en aduana

> **Adaptación.** El enunciado habla de «Hilandería Pacífico» (D3, bus I2C, zona de hilado, husos). El caso del G2 es la cadena de frío de espárrago. Se aplicaron las mismas perturbaciones: calor 36 °C × 5 días, filtración que cortocircuita un nodo y su bus, humedad saturada, apagón de 8 h + WAN caída, repuestos retenidos, tope de demanda de 460→360 kW y fiscalización de calor. Las cargas en kW son **supuestos del grupo** (no vienen en el caso); ajustar con el medidor real.

## 1. Arquitectura v2 (lo que cambió)

| Capa | v1 | **v2 (cambio)** |
|---|---|---|
| Device | Sensores únicos, cajas sin sellado | **Cajas IP65 en alto y con gotero**, bus I2C del gateway en caja sellada; **sensores redundantes 2-de-3** en cámaras (DHT22 + DS18B20 + lectura manual) |
| Embedded | Lectura simple | Validación por rango y salto, **bandera de calidad `bad`**, voto 2-de-3 y reemplazo por estimador (T° del evaporador) si un sensor falla |
| Edge | Alarmas/cierre de puerta, modo seguro | + **Modo degradado local**: sin gateway, cada cámara mantiene setpoint y ventilador; n6 arranca el grupo solo a los 10 s |
| Fog | Gateway único | **Gateway con UPS 2 h**, RTC con pila, **registro en microSD**, **control de demanda local** (recorte por prioridad), configuración versionada (ver Evento 2) |
| Cloud | ThingSpeak por WAN | WAN principal + **respaldo 4G**; la nube sólo historia; **sin dependencia remota** para decidir |
| Seguridad | HMAC + contador | Igual + registro de eventos en microSD |

## 2. WAN de respaldo (sin LoRa) y modo degradado

**Elección: SIM 4G LTE de otro operador** en router/gateway (S/ 40–60 al mes con 1–2 GB; cumple ≤ S/ 150). Si cae el proveedor fijo, el gateway cambia solo. Riesgo: la antena del operador también puede caer por el apagón → por eso el modo degradado **no necesita WAN**.

**Modo degradado (solo resúmenes):** el gateway deja de enviar lecturas y envía **1 resumen por lote cada 60 s** + alertas.

| Concepto | Cálculo | Resultado |
|---|---|---|
| Resumen por lote | 1 msg/min × 1 440 min × ~300 B (JSON 200 B + cabeceras/TLS ~100 B) | 432 KB/día |
| Alertas | ~200/día × 400 B | 80 KB/día |
| Total diario | 432 + 80 | ≈ 512 KB/día |
| **Mensual** | 512 KB × 30 | **≈ 15 MB** |
| Con 30 % de margen (reintentos/keep-alive) | 15 × 1,3 | **≈ 20 MB/mes** |

Para comparar, el modo normal (cada 15 s, ~300 B) = 5 760 msg/día ≈ 52 MB/mes: también cabe, pero el degradado permite una SIM de 100 MB.

## 3. Control de demanda y de humedad sin internet ni proveedor remoto

- **Todo corre en el gateway** (fog): lee el medidor/contador del tablero (entrada digital de pulsos o Modbus local) cada 5 s. Regla: si `kW > 360` durante 30 s en horario punta → recorta por prioridad (tabla siguiente) hasta `< 340 kW`; reingresa cargas con histéresis y 5 min de separación entre arranques. La «demanda en punta» es una **hora fija** guardada en el RTC; no depende de la señal del proveedor.
- **Humedad:** lazo local con humidificador/ventilador del evaporador y **histéresis 90–95 % HR**. Con sensores saturados (HR = 100 % constante > 10 min o salto > 15 %): marcan `bad`, el nodo usa el **modelo** (HR ≈ f(T_evaporador, T_aire)) y el operador toma lectura **manual con higrómetro portátil** cada 2 h anotada en el gateway (botón + registro). Nunca se mantiene el humidificador encendido con lectura `bad` más de 10 min.
- **Cámaras:** pre-enfriar a 0 °C antes de la punta (la masa térmica del producto actúa de «batería») y luego subir el setpoint a 1,5 °C durante la punta.

## 4. Recorte: cuántos kW y qué cargas (supuesto de 460 kW)

Meta: **−100 kW** (460 → 360).

| Prioridad de corte | Carga | kW actual | Recorte | Quedan |
|---|---|---|---|---|
| 1 | Oficinas / climatización | 30 | −25 | 5 |
| 2 | Iluminación y ventilación del andén (nivel mínimo) | 40 | −15 | 25 |
| 3 | Línea de empaque / palta (turnos escalonados, 50 %) | 90 | −45 | 45 |
| 4 | Hidroenfriador (pausa entre lotes, bomba por pulsos) | 120 | −15 | 105 |
| **Nunca se corta** | Compresores de cámaras (la cadena de frío) | 150 | 0 | 150 |
| Otros | — | 30 | 0 | 30 |
| **Total** | | **460** | **−100** | **360** |

**Equilibrio:** producción (se reduce al 50 % sólo la línea de palta/empaque; el espárrago ya enfriado no se detiene), **salud** (la fiscalización por calor/polvo: se conserva ventilación mínima y descansos programados en el andén, no se corta ventilación del personal), **distrito** (cumple el pedido de la distribuidora sin cortar viviendas) y **producto** (cámaras intactas). Mientras dure el apagón de 8 h el grupo cubre solo cargas críticas: cámaras + gateway + nodos.

## 5. Costo de las alternativas con sensores locales de reemplazo

Precios **estimados** (proveedores locales, S/); reemplazan el material retenido en aduana.

| Ítem | A (edge) | B (fog) | C (predictivo) |
|---|---|---|---|
| 7× ESP32 | 7 × 35 = 245 | 245 | 245 |
| Sensores (DS18B20 sumergible, DHT22, repuesto ×2) | 360 | 360 | 360 |
| Gateway ampliado (UPS, microSD, TFT, RTC) | — | 520 | 520 |
| 4G respaldo (equipo + 3 meses SIM) | — | 280 | 280 |
| Cómputo para modelo (mini-PC / Raspberry en fog) | — | — | 450 |
| Mano de obra de calibración y entrenamiento | 300 | 500 | 1 100 |
| **Total inicial (S/)** | **905** | **1 905** | **2 955** |
| Costo mensual (SIM) | 0 | 50 | 50 |

Lectura: A es lo más barato pero produce 75 % de falsas alarmas en el simulador; B mejora localización y falsas alarmas por ~S/ 1 000 más; C solo se justifica si supera a B en el simulador (bonificación).
