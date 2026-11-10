# Diagnóstico del requerimiento (G2 Agroindustria)

> «Queremos que una inteligencia artificial en la nube controle todo el frío de la planta y nos avise antes de que el espárrago se malogre. Que sea automático, en tiempo real, que no necesite personal y que funcione aunque se vaya la luz. Pongan LoRa en todas las cámaras, que es lo más moderno.»

| # | Tipo | Defecto | Por qué es un problema | Cómo se corrige |
|---|------|---------|------------------------|-----------------|
| 1 | Solución impuesta | «IA en la nube» | Dicta la tecnología antes del problema; la nube cae con el internet. | Se exige *resultado* (detectar y actuar); la capa se elige con evidencia (A vs B). |
| 2 | Contradicción | «en la nube» + «funcione aunque se vaya la luz» (y sin internet) | La nube no decide si no hay WAN/energía. | Decisión y acción en edge/fog; la nube solo guarda historia. |
| 3 | Tecnología prohibida | «Pongan LoRa» | La aseguradora lo prohíbe (auditoría). | Wi-Fi/Ethernet en planta; 4G solo como WAN de respaldo. |
| 4 | Término no medible | «todo el frío», «antes de que se malogre» | Sin umbral ni plazo no se puede verificar. | 0–2 °C; enfriar < 2 h; alerta con ≥ 20 % del presupuesto de minutos restante. |
| 5 | Término no medible | «tiempo real» | Sin latencia no hay criterio. | Detección→actuación local ≤ 5 s; alerta de gateway ≤ 10 s. |
| 6 | Alcance indefinido | «toda la planta» | Incluye espárrago y palta, sin etapas. | Alcance: espárrago, 5 etapas (recepción, hidroenfriado, cámaras 1–2, andén). |
| 7 | Expectativa irreal | «no necesite personal» | Alguien debe reconocer alertas y cargar/descargar. | Automático para actuar; un operador reconoce alertas (botón) y valida reanudaciones. |
| 8 | Expectativa irreal | «funcione aunque se vaya la luz» (sin respaldo definido) | Requiere grupo electrógeno/UPS con autonomía. | Grupo con arranque ≤ 30 s, UPS para nodos y gateway, y alerta si falla el respaldo. |
| 9 | Falta de restricción | Sin presupuesto de WAN ni seguridad | — | WAN ≤ S/ 150/mes; órdenes firmadas (HMAC); claves fuera del repositorio. |

## Requerimiento reescrito (verificable)

1. **RF1** El sistema registrará cada lote de espárrago en recepción (formato `L-AAMMDD-NN`, único) y seguirá su temperatura de pulpa por 4 etapas hasta el andén.
2. **RF2** Enfriará la pulpa a ≤ 2 °C en < 120 min desde la cosecha; si a los 90 min sigue > 2 °C, alertará.
3. **RF3** Mantendrá 0–2 °C; acumulará por lote los minutos fuera de rango y alertará al 80 % del presupuesto (120 min de la ventana de 2 h), indicando **la etapa** donde ocurrió.
4. **RF4** Si una puerta de cámara está abierta > 60 s sonará alarma; a los 120 s un servo la cerrará **sin esperar al gateway**.
5. **RF5** Un pallet en andén tendrá semáforo: amarillo al 50 % y rojo al 100 % del tiempo máximo; a rojo se alerta.
6. **RF6** Ante un apagón, el gateway ordenará arrancar el grupo; sin confirmación en 30 s alertará «falla del respaldo eléctrico», cerrará puertas y suspenderá la carga.
7. **RF7** Si una cámara pierde contacto con el gateway > 60 s: ventilador encendido y alarma local.
8. **RNF1** Detección y actuación operan **sin internet y sin red eléctrica** (gateway con UPS/RTC).
9. **RNF2** Sin LoRa; WAN ≤ S/ 150/mes; 3 marcas de cámara integradas por MQTT con mensaje común.
10. **RNF3** Toda orden a actuadores va firmada y con contador; los ataques se rechazan y se registran (hora, nodo, tipo, acción, motivo).
11. **Criterio de aceptación** Con los escenarios inyectados, falsas alarmas ≤ 5 % y localización correcta de la etapa ≥ 90 % (medido por `simulador/comparar.py`).
