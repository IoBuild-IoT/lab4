# Contrato de interfaz y reglas

**Tópicos** `upc/g2/<nodo>/{datos,alerta,estado,cmd}` con nodo ∈ n1..n6, gw.
**Mensaje** `{id, lote, ts, valor, unidad, calidad, firma}`; `firma` = HMAC-SHA256(clave del nodo, `id|lote|ts|valor|unidad|calidad`) (16 bytes en hex).
**Orden** `{id:"gw", dest, cmd, arg, ctr, ts, firma}`; firma sobre `gw|dest|cmd|arg|ctr|ts`; el actuador exige firma válida **y** `ctr` mayor al último aceptado.
**Estado** `online` cada 30 s; LWT `offline`. **Nube** solo el gateway escribe en ThingSpeak, ≥ 15 s.

| Nodo | Entradas (señal) | Salidas | Edge | Ataque → defensa |
|---|---|---|---|---|
| n1 Recepción | DS18B20 (1-Wire), pulsador (digital) | LED RGB | Valida rango/salto; semáforo | Lote falso → formato + unicidad |
| n2 Hidro | 2× DS18B20 (1-Wire) | Relé bomba | Histéresis 2/1 °C; alerta a 90 min | «Lote enfriado» falso → firma + coherencia T°/tiempo |
| n3 Cámara 1 | DHT22, interruptor puerta | Buzzer, servo | 60 s alarma / 120 s cierre | Suplantación → HMAC por nodo |
| n4 Cámara 2 | DHT22, interruptor puerta | Relé ventilador, servo, LED | Modo seguro 60 s | Orden falsa/replay → firma + contador; no apaga con T > 2 °C |
| n5 Andén | DS18B20, PIR | LED RGB | Semáforo por tiempo | Inundación → máx. 5 msg/s |
| n6 Energía | 2 interruptores (red, grupo) | Relé grupo, LED | Arranque local si el gateway no ordena en 10 s | Orden falsa de apagar → firma + «no se apaga sin red» |
| n7 Gateway | Pulsador, RTC DS1307 (I2C) | TFT ILI9341 (SPI), NeoPixel | Seguimiento por lote (B), lazo cerrado | Cambio de config → doble confirmación + registro |

Las claves están en `common/secrets.h` (no versionado; ver `secrets.example.h`). La página solo usa la clave de **lectura** de ThingSpeak.
