# G2 Agroindustria — Sistema IoT Industrial para Cadena de Frío
> **Metodología de IoT Industrial en 12 Pasos**  
> **Proyecto:** Monitoreo y control inteligente de cadena de frío para exportación de espárrago fresco (7 nodos ESP32 + MQTT + Fog Gateway + Dashboard).

---

## Índice de Secciones
* [Paso Previo: Diagnóstico del Requerimiento](#paso-previo-diagnóstico-del-requerimiento)
* [Paso 1: Requisitos del Sistema (Time-Delay y Suministro de Energía)](#paso-1-requisitos-del-sistema-time-delay-y-suministro-de-energía)
* [Paso 2: Tipología Elegida y su Justificación](#paso-2-tipología-elegida-y-su-justificación)
* [Paso 3: Tabla de Nodos con Incertidumbre y Exactitud](#paso-3-tabla-de-nodos-con-incertidumbre-y-exactitud)
* [Paso 4: Requisitos de Intercambio (WAN Elegida y Restricciones)](#paso-4-requisitos-de-intercambio-wan-elegida-y-restricciones)
* [Paso 5: Usuarios Finales e Información Integrada](#paso-5-usuarios-finales-e-información-integrada)
* [Paso 6: Servicios por Usuario (Alertas, Actuación, Registro y Acceso Remoto)](#paso-6-servicios-por-usuario-alertas-actuación-registro-y-acceso-remoto)
* [Paso 7: Contrato de Interfaz (Tópicos, Mensajes, Órdenes, Latido y LWT)](#paso-7-contrato-de-interfaz-tópicos-mensajes-órdenes-latido-y-lwt)
* [Paso 8: Diagrama en Cirkit (Sensores, Actuadores y Tipo de Señal)](#paso-8-diagrama-en-cirkit-sensores-actuadores-y-tipo-de-señal)
* [Paso 9: Simulación Local en Wokwi para Visual Studio Code](#paso-9-simulación-local-en-wokwi-para-visual-studio-code)
* [Paso 10: Reglas de Procesamiento (Edge en Nodos y Fog en Gateway)](#paso-10-reglas-de-procesamiento-edge-en-nodos-y-fog-en-gateway)
* [Paso 11: Comparación A (Edge) vs B (Fog) con Datos Propios](#paso-11-comparación-a-edge-vs-b-fog-con-datos-propios)
* [Paso 12: Dashboard (5 Pestañas, ThingSpeak y Pantalla TFT)](#paso-12-dashboard-5-pestañas-thingspeak-y-pantalla-tft)
* [Sección Transversal: Ciberseguridad, Ataques Simulados y Defensas](#sección-transversal-ciberseguridad-ataques-simulados-y-defensas)
* [Guía de Puesta en Marcha y Validación](#guía-de-puesta-en-marcha-y-validación)

---

## Paso Previo: Diagnóstico del Requerimiento

### 1. Requerimiento original del cliente
> *«Queremos que una inteligencia artificial en la nube controle todo el frío de la planta y nos avise antes de que el espárrago se malogre. Que sea automático, en tiempo real, que no necesite personal y que funcione aunque se vaya la luz. Pongan LoRa en todas las cámaras, que es lo más moderno.»*

### 2. Tabla de Defectos Identificados y Corrección Técnica

| # | Tipo de Defecto | Fragmento del Requerimiento | Por qué es un problema | Corrección técnica esperada |
|---|---|---|---|---|
| **1** | **Solución impuesta** | *«IA en la nube»* | Dicta una arquitectura prematura. La nube es vulnerable a caídas de internet y añade latencias inaceptables para control crítico. | Exigir resultados verificables (detectar y actuar en local). Usar la nube únicamente para histórico y reportes. |
| **2** | **Contradicción** | *«En la nube»* + *«funcione aunque se vaya la luz»* | Si se corta la energía comercial y la conectividad WAN, la nube queda inaccesible e inoperante. | Lógica de detección y actuación distribuida en **Edge** (nodos) y **Fog** (gateway local con UPS); telemetría diferida. |
| **3** | **Tecnología prohibida** | *«Pongan LoRa en todas las cámaras»* | Las cámaras frigoríficas actúan como jaulas de Faraday metálicas que degradan señales RF de baja potencia; además la aseguradora prohíbe LoRa para trazabilidad auditable. | Red cableada Ethernet y Wi-Fi industrial en planta; router WAN 4G LTE solo como enlace de respaldo a internet. |
| **4** | **Término no medible** | *«Nos avise antes de que el espárrago se malogre»* | Concepto ambiguo sin umbral físico, base biológica ni criterio de aceptación verificable. | Control de 0 a 2 °C; enfriamiento inicial < 120 min; alertar al consumir el 80 % del presupuesto de minutos fuera de rango por lote. |
| **5** | **Término no medible** | *«En tiempo real»* | No establece latencia máxima admisible en milisegundos o segundos. | Time-delay máximo: detección $\to$ actuación local $\le 5\text{ s}$; alerta y coordinación en gateway $\le 10\text{ s}$. |
| **6** | **Alcance indefinido** | *«Todo el frío de la planta»* | Mezcla etapas y diferentes productos agrícolas (espárrago vs palta) con requerimientos térmicos disímiles. | Acotar el flujo exclusivo a espárrago en 5 etapas secuenciales: Recepción, Hidroenfriado, Cámara 1, Cámara 2 y Andén de Despacho. |
| **7** | **Expectativa irreal** | *«Que no necesite personal»* | Un sistema industrial autónomo no puede sustituir la validación humana en contingencias, reanudación y maniobras de carga. | Automatizar la actuación crítica inmediata pero exigir reconocimiento físico (*acknowledge*) por parte del operador ante alarmas. |
| **8** | **Expectativa irreal** | *«Funcione aunque se vaya la luz»* (sin definir respaldo) | Los dispositivos electrónicos requieren suministro continuo ininterrumpido. | Definir grupo electrógeno con arranque automático $\le 30\text{ s}$ y respaldo UPS de $\ge 2\text{ h}$ para gateway y nodos. |
| **9** | **Falta de restricción** | Sin presupuesto de conectividad ni seguridad | Riesgo de sobrecostos de telecomunicaciones y vulnerabilidad ante comandos no autorizados. | Restringir gasto WAN a $\le \text{S/ } 150/\text{mes}$; cifrado y autenticación HMAC-SHA256 con protección anti-replay en todas las órdenes. |

### 3. Requerimiento Reescrito (Verificable)
* **RF1:** Registrar cada lote de espárrago en recepción bajo formato único `L-AAMMDD-NN` y rastrear su temperatura de pulpa a través de las etapas del proceso.
* **RF2:** Enfriar la pulpa a $\le 2^\circ\text{C}$ en $< 120\text{ min}$ tras cosecha; si a los 90 min continúa $> 2^\circ\text{C}$, emitir pre-alerta de proceso.
* **RF3:** Mantener la pulpa entre $0$ y $2^\circ\text{C}$; acumular por lote los minutos de excursión térmica y disparar alarma al alcanzar el 80 % del presupuesto máximo (120 min), indicando con precisión la etapa responsable.
* **RF4:** En cámaras de almacenamiento, si la puerta permanece abierta $> 60\text{ s}$, sonar alarma local; a los $120\text{ s}$, activar servomotor de cierre forzado de forma local (Edge).
* **RF5:** En andén de despacho, controlar la permanencia del pallet mediante semáforo: amarillo al 50 % y rojo al 100 % del tiempo límite autorizado.
* **RF6:** Ante un corte de energía comercial, ordenar el arranque del grupo electrógeno y confirmar transferencia en $\le 30\text{ s}$; en caso de falla, cerrar compuertas de cámaras y detener carga.
* **RF7:** Si una cámara pierde comunicación con el gateway por más de $60\text{ s}$, entrar en modo seguro local: forzar ventilación encendida y registrar alerta.
* **RNF1:** La detección y actuación deben funcionar de manera autónoma sin internet ni energía comercial (batería/UPS local).
* **RNF2:** Prohibido el uso de LoRa; costo operativo de conectividad externa $\le \text{S/ } 150/\text{mes}$.
* **RNF3:** Todo mensaje de control/orden debe estar firmado mediante HMAC-SHA256 con control de secuencia monotónica (anti-replay).

---

## Paso 1: Requisitos del Sistema (Time-Delay y Suministro de Energía)

### 1. Tiempos de Respuesta Críticos (*Time-Delay*)

| Evento / Lazo de Control | Detección $\to$ Actuación | Justificación Técnica |
|---|---|---|
| **Alarma local de puerta de cámara** | $\le 60\text{ s}$ | Evita el calentamiento del aire en la cámara frigorífica sin interrumpir maniobras normales de ingreso. |
| **Cierre forzado de puerta por servomotor** | $\le 120\text{ s}$ | Cumplidos 2 minutos, la masa térmica del producto comienza a absorber calor; el nodo cierra la compuerta autónomamente. |
| **Detección $\to$ Actuación local (Edge)** | $\le 5\text{ s}$ | Lectura, filtrado y disparo del actuador dentro del mismo microcontrolador sin depender de la red MQTT. |
| **Coordinación y alerta en Fog Gateway** | $\le 10\text{ s}$ | Tiempo para recibir mensaje MQTT, validar HMAC, evaluar el estado global del lote y despachar orden firmada. |
| **Arranque y transferencia de Grupo Electrógeno** | $\le 30\text{ s}$ | La inercia térmica de las cámaras tolera hasta 30 s sin compresión antes de que la presión del gas refrigerante se desestabilice. |
| **Paso a Modo Seguro por pérdida de enlace (Heartbeat)** | $\le 60\text{ s}$ | Si un nodo de cámara no recibe el latido del gateway durante 2 ciclos (60 s), asume aislamiento y enciende ventilación. |

### 2. Esquema de Suministro de Energía y Autonomía

```
                       [ RED COMERCIAL 220/380 VAC ]
                                     │
                        ┌────────────┴────────────┐
                        ▼                         ▼
            [ Interruptor Red (n6) ]    [ Conmutador de Transferencia ]
                        │                         ▲
   ┌────────────────────┘                         │
   ▼                                              │
[ Falla de Red ] ──(10 s local / gw)──► [ Arranque Grupo Electrógeno ]
                                       (Autonomía combustible: 8 horas)
                                                  │
                                                  ▼
                                       [ TABLERO DE CARGAS CRÍTICAS ]
                                        ├── Compresores de Cámaras
                                        ├── Bombas de Hidroenfriador
                                        └── [ Sistema UPS Online 2 kVA ]
                                                  │ (Autonomía: 2 horas)
                                                  ▼
                                       [ NODOS ESP32 + GATEWAY FOG ]
```

* **Respaldo Primario (Grupo Electrógeno diésel):**
  * Potencia: 150 kVA para alimentar compresores, bombas y servicios vitales.
  * Arranque automático comandado por el nodo `n6` o por el gateway. Tiempo de entrada en régimen: $\le 30\text{ s}$.
  * Autonomía de combustible en tanque de almacenamiento diario: $\ge 8\text{ horas}$ continuas.
* **Respaldo Secundario (UPS Ininterrumpida):**
  * Topología: UPS Online de doble conversión (2 kVA) dedicada exclusivamente al rack del Gateway Fog, switches de red y alimentación DC de los 7 nodos ESP32.
  * Autonomía por baterías selladas VRLA: $\ge 2\text{ horas}$, cubriendo sobradamente el periodo de conmutación del grupo electrógeno.

---

## Paso 2: Tipología Elegida y su Justificación

Se ha seleccionado una **arquitectura en 5 capas con control de lazo cerrado y procesamiento Fog/Edge**, descartando el control directo desde la nube.

```
┌────────────────────────────────────────────────────────────────────────┐
│  5. CAPA CLOUD (Nube)                                                  │
│     • ThingSpeak API: Almacenamiento histórico, tendencias y auditoría │
│     • Acceso remoto gerencial (solo lectura)                           │
└───────────────────────────────────▲────────────────────────────────────┘
                                    │ WAN (4G LTE / HTTPS - cada 15 s)
┌───────────────────────────────────┴────────────────────────────────────┐
│  4. CAPA FOG (Gateway Local)                                           │
│     • Gateway ESP32/Industrial con RTC DS1307 y display TFT ILI9341    │
│     • Trazabilidad global por lote (L-AAMMDD-NN)                       │
│     • Lógica de control de demanda eléctrica (< 360 kW)                │
│     • Registro local a prueba de caídas WAN en tarjeta microSD         │
└───────────────────────────────────▲────────────────────────────────────┘
                                    │ LAN Wi-Fi / Broker MQTT (HiveMQ)
┌───────────────────────────────────┴────────────────────────────────────┐
│  3. CAPA EDGE (Nodos Inteligentes n1..n6)                              │
│     • Microcontroladores ESP32 dedicados por área                      │
│     • Toma de decisiones autónoma instantánea (< 5 s)                  │
│     • Enclavamientos de seguridad locales y modo seguro (Fail-Safe)    │
└───────────────────────────────────▲────────────────────────────────────┘
                                    │ GPIO / ADC / Timers
┌───────────────────────────────────┴────────────────────────────────────┐
│  2. CAPA EMBEDDED (Firmware y Protocolos)                              │
│     • Firmware C++ Arduino-ESP32, JSON (ArduinoJson), HMAC-SHA256      │
│     • Filtrado de ruido, detección de flancos y control anti-rebote    │
└───────────────────────────────────▲────────────────────────────────────┘
                                    │ 1-Wire / I2C / Señales analógicas
┌───────────────────────────────────┴────────────────────────────────────┐
│  1. CAPA DEVICE (Sensores y Actuadores Físicos)                        │
│     • Sensores: DS18B20 sumergibles, DHT22, Finales de carrera, PIR    │
│     • Actuadores: Relés de potencia, Servomotores de compuerta, Buzzer │
└────────────────────────────────────────────────────────────────────────┘
```

### Justificación de la Arquitectura
1. **Lazo Cerrado a Nivel Edge:** La planta de frío no puede tolerar caídas de enlace. Si una compuerta queda abierta o la red local colapsa, el microcontrolador local decide el cierre físico y la activación de alarmas sonoras.
2. **Coordinación Fog sin Dependencia Externa:** El cálculo de minutos acumulados fuera de rango de cada lote requiere integrar datos de 4 etapas consecutivas. Esta consolidación se realiza en el Gateway Fog local, manteniendo la trazabilidad íntegra aun cuando se interrumpa la conexión a internet.
3. **Nube Asíncrona (Desacoplada del Control):** La nube actúa como repositorio histórico y panel de supervisión remota. Jamás interviene en el bucle de actuación de los actuadores de potencia.

---

## Paso 3: Tabla de Nodos con Incertidumbre y Exactitud

Cada integrante es responsable del hardware, instrumentación y firmware de su nodo asignado:

| Nodo | Nombre / Etapa | Integrante Asignado | Sensor Utilizado e **Incertidumbre** | Actuador Utilizado y **Exactitud / Tiempo** | Señales de E/S |
|---|---|---|---|---|---|
| **n1** | Recepción | Integrante 1 | **DS18B20:** $\pm 0.5^\circ\text{C}$ (rango $-10$ a $+85^\circ\text{C}$), resolución 12 bits ($0.0625^\circ\text{C}$). Pulsador con anti-rebote software de $50\text{ ms}$. | **LED RGB (Semáforo):** Tiempo de conmutación $< 1\,\mu\text{s}$. Estados visuales definidos (Verde = lote OK, Amarillo = desvío, Rojo = duplicado). | GPIO 4 (1-Wire), GPIO 5 (DI), GPIO 25/26/27 (DO) |
| **n2** | Hidroenfriador | Integrante 2 | **2× DS18B20 sumergibles** (agua y pulpa): $\pm 0.5^\circ\text{C}$, resolución $0.0625^\circ\text{C}$. | **Relé electromecánico 10 A:** Conmutación $< 10\text{ ms}$. Histéresis de temperatura: corte a $1.0^\circ\text{C}$, encendido a $2.0^\circ\text{C}$. | GPIO 4 (1-Wire agua), GPIO 16 (1-Wire pulpa), GPIO 26 (DO) |
| **n3** | Cámara 1 | Integrante 3 | **DHT22:** Temp $\pm 0.5^\circ\text{C}$, Humedad $\pm 2\text{ a } 5\%\,\text{HR}$. **Switch magnético:** detección de apertura en $< 5\text{ ms}$. | **Buzzer:** Alarma local $\ge 85\text{ dB}$ ($< 1\text{ ms}$). **Servo SG90:** Exactitud angular $\pm 1^\circ$, rotación $0\to 90^\circ$ en $0.3\text{ s}$. | GPIO 4 (DHT), GPIO 14 (DI puerta), GPIO 27 (DO buzzer), GPIO 13 (PWM servo) |
| **n4** | Cámara 2 | Integrante 4 | **DHT22:** Temp $\pm 0.5^\circ\text{C}$, Humedad $\pm 2\text{ a } 5\%\,\text{HR}$. **Switch magnético de puerta.** | **Relé Ventilador:** Activación en $< 10\text{ ms}$. **Servo compuerta:** $\pm 1^\circ$. **LED Modo seguro:** Indicación instantánea. | GPIO 4 (DHT), GPIO 14 (DI puerta), GPIO 26 (DO relé), GPIO 13 (PWM servo), GPIO 2 (DO led) |
| **n5** | Andén | Integrante 5 | **DS18B20:** $\pm 0.5^\circ\text{C}$. **Sensor PIR HC-SR501:** Detección infrarroja pasiva, retardo ajustable $0.3\text{–}18\text{ s}$, ángulo $120^\circ$. | **LED RGB:** Semáforo de permanencia de pallet en andén (Verde $< 50\%$, Amarillo $50\text{–}100\%$, Rojo $> 100\%$). | GPIO 4 (1-Wire), GPIO 27 (DI PIR), GPIO 25/26/33 (DO) |
| **n6** | Energía y Respaldo | Integrante 6 | **2× Sensores de presencia de tensión AC:** Tiempo de respuesta ante corte $< 20\text{ ms}$. | **Relé de arranque de Grupo Electrógeno:** Tiempo de contacto $< 10\text{ ms}$; arranque en régimen $\le 30\text{ s}$. **LED falla.** | GPIO 14 (DI red), GPIO 12 (DI grupo), GPIO 26 (DO arranque), GPIO 27 (DO alarma) |
| **n7** | Gateway Fog | Integrante 7 (Integrador) | **RTC DS1307/DS3231:** Desvío $< 2\text{ ppm}$ ($< 1\text{ min/año}$). Pulsador de confirmación manual de operador. | **Display TFT ILI9341 (SPI):** $320\times 240\text{ px}$, refresco $> 15\text{ fps}$, latencia $< 60\text{ ms}$. **NeoPixel WS2812B.** | GPIO 21/22 (I2C RTC), GPIO 15/2/4 (SPI TFT), GPIO 5 (NeoPixel), GPIO 0 (DI botón) |

---

## Paso 4: Requisitos de Intercambio (WAN Elegida y Restricciones)

### 1. Solución WAN Seleccionada y Presupuesto Operativo
* **Tecnología:** Enlace celular **4G LTE** mediante módem industrial / SIM card M2M multicarrier.
* **Costo Operativo Mensual:** Plan de datos de **S/ 45.00 a S/ 60.00 al mes** con bolsa de $2\text{ GB}$, cumpliendo ampliamente la restricción presupuestaria impuesta ($\le \text{S/ } 150/\text{mes}$).

### 2. Dimensionamiento del Tráfico de Datos en Modo Degradado

| Concepto | Frecuencia y Tamaño de Mensaje | Tráfico Diario | Tráfico Mensual |
|---|---|---|---|
| **Resúmenes por Lote** | 1 mensaje/minuto ($\approx 300\text{ bytes}$) | $432\text{ KB/día}$ | $12.96\text{ MB/mes}$ |
| **Alertas e Incidentes** | $\approx 200\text{ eventos/día}$ ($\approx 400\text{ bytes}$) | $80\text{ KB/día}$ | $2.40\text{ MB/mes}$ |
| **Telemetría ordinaria ThingSpeak** | Cada $15\text{ s}$ ($\approx 250\text{ bytes}$) | $1.44\text{ MB/día}$ | $43.20\text{ MB/mes}$ |
| **Margen de seguridad (TLS, reintentos +30%)** | — | — | $\approx 17.5\text{ MB}$ |
| **TOTAL ESTIMADO** | — | — | **$\approx 76.06\text{ MB/mes}$** |

> *Conclusión:* El consumo total no supera los $100\text{ MB/mes}$. El plan contratado de $2\text{ GB}$ ofrece un margen de holgura superior al 2000 %.

### 3. Justificación Técnica: ¿Por qué NO LoRa?
1. **Aislamiento por Jaula de Faraday:** Las cámaras de frío industrial están construidas con paneles sándwich de chapa de acero galvanizado con poliuretano de alta densidad de 100 a 150 mm. Las frecuencias de 915 MHz experimentan una atenuación severa ($> 30\text{ dB}$), causando pérdidas inaceptables de paquetes en mensajes críticos de alarma.
2. **Prohibición de la Aseguradora de Carga:** La póliza de exportación exige acuse de recibo inmediato y no repudio. El ciclo de trabajo (*duty cycle*) legal y la latencia intrínseca de LoRaWAN impiden garantizar confirmaciones de actuación en $< 5\text{ s}$.
3. **Payload Limitado:** La necesidad de incorporar firmas criptográficas HMAC de 16 bytes y contadores de secuencia en cada trama hace ineficiente la fragmentación de tramas en LoRa.

---

## Paso 5: Usuarios Finales e Información Integrada

El Gateway Fog consolida las señales de los 6 nodos de campo y las proyecta según el perfil de cada usuario:

```
                                  [ GATEWAY FOG ]
                                         │
        ┌──────────────────┬─────────────┴─────────────┬──────────────────┐
        ▼                  ▼                           ▼                  ▼
[ Operador Planta ]  [ Jefe Calidad ]        [ Sup. Mantenimiento ]  [ Gerente Operaciones ]
• Alarma de puerta   • Trazabilidad lote     • Estado grupo electrógeno • Reporte ThingSpeak
• Semáforo andén     • Minutos fuera rango   • Latidos LWT de nodos  • % Cumplimiento frío
• Reconocimiento ACK • Etapa de ruptura      • Log ciberseguridad    • Lotes exportables
```

### Información Integrada por Tipo de Usuario:
1. **Operador de Planta / Andén:**
   * Información visual directa: Semáforo de permanencia de pallet en andén (Verde/Amarillo/Rojo).
   * Alarma sonora y baliza si una puerta permanece abierta más de 60 segundos.
   * Botón físico de reconocimiento (*Acknowledge*) para silenciar alarmas y validar reanudación segura.
2. **Jefe de Aseguramiento de la Calidad (QA):**
   * Curva térmica consolidada del lote desde su recepción hasta el despacho.
   * Contador de minutos acumulados fuera de rango ($0\text{–}2^\circ\text{C}$) respecto al presupuesto de 120 min.
   * Diagnóstico inmediato de la **etapa exacta** donde ocurrió la excursión térmica para determinar si el lote califica para exportación marítima o debe derivarse a mercado local.
3. **Supervisor de Mantenimiento y Electromecánica:**
   * Estado de la red comercial y disponibilidad operativa del grupo electrógeno (tiempo de arranque).
   * Panel de conectividad de nodos (estado `online`/`offline` vía LWT y frecuencia de latidos).
   * Bitácora de ciberseguridad con registro de firmas HMAC inválidas, repetición de paquetes o inundaciones.
4. **Gerente de Operaciones:**
   * Cuadro de mando gerencial en la nube (ThingSpeak) accesible desde cualquier ubicación remota.
   * KPIs de desempeño: porcentaje de lotes conforme a norma, índice de interrupciones y demanda de potencia eléctrica bajo control ($< 360\text{ kW}$).

---

## Paso 6: Servicios por Usuario (Alertas, Actuación, Registro y Acceso Remoto)

| Perfil de Usuario | Servicio de Alertas | Servicio de Actuación | Servicio de Registro | Servicio de Acceso Remoto |
|---|---|---|---|---|
| **Operador de Planta** | Sirena local $> 85\text{ dB}$, baliza visual RGB y display TFT en gateway. | Pulsador de ingreso de nuevo lote; pulsador de reconocimiento ACK de fallas. | Registro temporal visual en pantalla TFT del Gateway. | Sin acceso remoto; interacción directa en tablero de planta. |
| **Jefe de Aseguramiento de Calidad** | Notificación push/MQTT de pre-alerta al superar el 80 % del presupuesto de frío (96 min) e indicación de etapa. | Aprobación o bloqueo digital de despacho de lote; liberación de cuarentena. | Histórico completo de temperatura por lote almacenado en microSD y ThingSpeak. | Consulta remota de histórico vía Dashboard Web y ThingSpeak. |
| **Supervisor de Mantenimiento** | Alerta por conmutación a grupo electrógeno, falla de arranque a los 30 s o nodo desconectado (LWT). | Comando firmado de arranque/parada de grupo; activación manual de modo seguro. | Registro de incidentes de seguridad (hora, nodo, ataque, motivo) y eventos eléctricos. | Dashboard Web en red local (vía WebSocket MQTT) y acceso VPN seguro. |
| **Gerente de Operaciones** | Reporte diario de anomalías térmicas y alertas de corte de demanda eléctrica en hora punta. | Autorización de prioridades de corte de cargas no esenciales. | Auditoría consolidada mensual exportable a CSV/Excel. | Monitoreo integral vía ThingSpeak en laptop o dispositivo móvil. |

---

## Paso 7: Contrato de Interfaz (Tópicos, Mensajes, Órdenes, Latido y LWT)

### 1. Estructura de Tópicos MQTT
* Raíz del sistema: `upc/g2/`
* Telemetría de nodos: `upc/g2/<nodo>/datos` (donde `<nodo>` $\in$ `{n1, n2, n3, n4, n5, n6, gw}`)
* Alertas de nodos y gateway: `upc/g2/<nodo>/alerta`
* Estado de conexión y latido: `upc/g2/<nodo>/estado`
* Estado del lote activo: `upc/g2/gw/lote` (última etapa y minutos fuera de rango, retenido para el dashboard)
* Comandos de control hacia actuadores: `upc/g2/<nodo>/cmd`

### 2. Formato de Mensajes JSON y Criptografía

#### A. Telemetría (`upc/g2/<nodo>/datos`)
```json
{
  "id": "n3",
  "lote": "L-261110-01",
  "ts": 1731245000,
  "valor": "1.85",
  "unidad": "C",
  "calidad": "ok",
  "firma": "a4f8c12d3b9e4a10"
}
```
* **Firma criptográfica:** Los primeros 16 caracteres hexadecimales de `HMAC-SHA256(clave_nodo, "id|lote|ts|valor|unidad|calidad")`.
* **Calidad del dato:** `ok` (sensor nominal), `bad` (fuera de rango o salto abrupto $\Delta T > 5^\circ\text{C}$), `reg` (registro de nuevo lote).

#### B. Comandos y Órdenes de Actuación (`upc/g2/<nodo>/cmd`)
```json
{
  "id": "gw",
  "dest": "n4",
  "cmd": "fan_on",
  "arg": "forzado",
  "ctr": 1042,
  "ts": 1731245005,
  "firma": "7f09a2b5e3d1c8f4"
}
```
* **Firma de la orden:** `HMAC-SHA256(clave_nodo, "gw|dest|cmd|arg|ctr|ts")`.
* **Defensa Anti-Replay:** El campo `ctr` es un entero estrictamente monotónico creciente generado por el Gateway y guardado en NVS por el nodo receptor. Si `ctr <= lastCtr`, la orden es rechazada.

#### C. Mensajes de Latido (*Heartbeat*) y LWT
* **Latido periódico:** Publicación cada $30\text{ s}$ en `upc/g2/<nodo>/estado` con payload en texto plano `"online"`.
* **LWT (Last Will and Testament):** Configurado al conectar al broker sobre `upc/g2/<nodo>/estado` con payload `"offline"`, QoS 1, Retain `true`.

---

## Paso 8: Diagrama en Cirkit (Sensores, Actuadores y Tipo de Señal)

La interconexión eléctrica y física de los módulos se sintetiza en la siguiente matriz de señales:

| Nodo | Componente Físico | Tipo de Señal | Pin ESP32 | Parámetros de Operación Eléctrica |
|---|---|---|---|---|
| **n1** | Sensor Temp. DS18B20 | Digital (1-Wire) | GPIO 4 | Pull-up $4.7\text{ k}\Omega$ a 3.3V, dirección ROM única de 64 bits |
| **n1** | Pulsador Registro Lote | Digital (Entrada) | GPIO 5 | Pull-up interno, activo en bajo (GND) |
| **n1** | LED RGB Semáforo | Digital (Salida) | GPIO 25, 26, 27 | Resistencias limitadoras de $220\,\Omega$, cátodo común a GND |
| **n2** | DS18B20 Temp. Agua | Digital (1-Wire) | GPIO 4 | Sonda de inmersión en cascada |
| **n2** | DS18B20 Temp. Pulpa | Digital (1-Wire) | GPIO 16 | Bus independiente para evitar conflictos de direccionamiento |
| **n2** | Módulo Relé Bomba Frío | Digital (Salida) | GPIO 26 | Aislado por optoacoplador, bobina 5V, activo en bajo |
| **n3** | Sensor DHT22 Temp/HR | Digital (Bus Propietario) | GPIO 4 | Rango $-40\text{ a }+80^\circ\text{C}$, $0\text{–}100\%\,\text{HR}$ |
| **n3** | Final de Carrera Puerta | Digital (Entrada) | GPIO 14 | Interruptor normalmente abierto con pull-up |
| **n3** | Buzzer Piezoeléctrico | Digital / PWM | GPIO 27 | Oscilador interno $2.4\text{ kHz}$ |
| **n3** | Servomotor Cierre Puerta | PWM ($50\text{ Hz}$) | GPIO 13 | Pulso $0.5\text{–}2.5\text{ ms}$, alimentación separada 5V |
| **n4** | Sensor DHT22 Temp/HR | Digital | GPIO 4 | Monitoreo ambiental de cámara 2 |
| **n4** | Final de Carrera Puerta | Digital | GPIO 14 | Monitoreo de estado de compuerta |
| **n4** | Relé Forzador Ventilador | Digital (Salida) | GPIO 26 | Control de contactor de evaporador |
| **n4** | Servomotor Compuerta | PWM | GPIO 13 | Cierre de emergencia |
| **n4** | LED Modo Seguro | Digital | GPIO 2 | LED integrado azul / testigo exterior |
| **n5** | DS18B20 Temp. Andén | Digital (1-Wire) | GPIO 4 | Monitoreo térmico exterior |
| **n5** | Sensor Presencia PIR | Digital (Entrada) | GPIO 27 | Salida lógica 3.3V de detección de movimiento |
| **n5** | LED RGB Semáforo | Digital (Salida) | GPIO 25, 26, 33 | Indicador de tiempo máximo en andén |
| **n6** | Detector Presencia Red | Digital (Entrada) | GPIO 14 | Contacto auxiliar de relé supervisor de fase |
| **n6** | Detector Estado Grupo | Digital (Entrada) | GPIO 12 | Retorno de confirmación de alternador de grupo |
| **n6** | Relé Arranque Grupo | Digital (Salida) | GPIO 26 | Contacto seco hacia tarjeta de control DeepSea/ComAp |
| **n6** | LED Falla de Respaldo | Digital (Salida) | GPIO 27 | Parpadeo rápido visual de emergencia |
| **n7** | Display TFT ILI9341 | Digital (SPI) | SCK: 18, MOSI: 23, CS: 15, DC: 2, RST: 4 | Pantalla a color $320\times 240$ px |
| **n7** | Módulo RTC DS1307 | Digital (I2C) | SDA: 21, SCL: 22 | Reloj de tiempo real con pila de botón CR2032 |
| **n7** | Pulsador ACK Operador | Digital (Entrada) | GPIO 0 | Confirmación física de eventos críticos |
| **n7** | LED NeoPixel WS2812B | Digital (Salida) | GPIO 5 | Protocolo serie NRZ de un hilo |

> *Diagrama Gráfico Cirkit:* El esquema circuital completo exportado se encuentra en `docs/cirkit.png`.

---

## Paso 9: Simulación Local en Wokwi para Visual Studio Code

El proyecto utiliza la **extensión oficial de Wokwi para Visual Studio Code** (`wokwi.wokwi-vscode`), lo que permite que todo el hardware, cableado y firmware esté versionado localmente en el repositorio sin depender de enlaces web externos ni almacenamiento en la nube de terceros.

### 1. Estructura de Nodos en el Repositorio
Cada uno de los 7 nodos cuenta con su carpeta autónoma y completa con todos los archivos necesarios para simularse de forma independiente:

| Nodo | Etapa / Función | Integrante | Ruta del Proyecto en VS Code | Archivos Clave del Nodo | Periféricos Simulados |
|---|---|---|---|---|---|
| **n1** | Recepción | Integrante 1 | [`nodos/n1_recepcion/`](nodos/n1_recepcion/) | `n1_recepcion.ino`, `diagram.json`, `libraries.txt` | ESP32, DS18B20 (pulpa), Pulsador Lote, LED RGB |
| **n2** | Hidroenfriador | Integrante 2 | [`nodos/n2_hidroenfriador/`](nodos/n2_hidroenfriador/) | `n2_hidroenfriador.ino`, `diagram.json`, `libraries.txt` | ESP32, 2× DS18B20 (agua/pulpa), Relé Bomba |
| **n3** | Cámara 1 | Integrante 3 | [`nodos/n3_camara1/`](nodos/n3_camara1/) | `n3_camara1.ino`, `diagram.json`, `libraries.txt` | ESP32, DHT22, Switch Puerta, Buzzer, Servo |
| **n4** | Cámara 2 | Integrante 4 | [`nodos/n4_camara2/`](nodos/n4_camara2/) | `n4_camara2.ino`, `diagram.json`, `libraries.txt` | ESP32, DHT22, Switch Puerta, Relé Fan, Servo, LED |
| **n5** | Andén | Integrante 5 | [`nodos/n5_anden/`](nodos/n5_anden/) | `n5_anden.ino`, `diagram.json`, `libraries.txt` | ESP32, DS18B20, Sensor PIR, LED RGB |
| **n6** | Energía | Integrante 6 | [`nodos/n6_energia/`](nodos/n6_energia/) | `n6_energia.ino`, `diagram.json`, `libraries.txt` | ESP32, 2× Switches (Red/Grupo), Relé Grupo, LED |
| **n7** | Gateway Fog | Integrante 7 | [`nodos/n7_gateway/`](nodos/n7_gateway/) | `n7_gateway.ino`, `diagram.json`, `libraries.txt` | ESP32, Display TFT ILI9341, Pulsador ACK, RTC |

### 2. Instrucciones para Simular en Visual Studio Code
1. Instalar PlatformIO y la extensión **Wokwi Simulator** (`wokwi.wokwi-vscode`). Desde la raíz del proyecto, compilar el nodo que se desea ejecutar. Por ejemplo, para Cámara 1:
   ```bash
   pio run -d nodos/n3_camara1
   ```
2. En VS Code, abrir `nodos/n3_camara1/` como carpeta de trabajo. Abrir su `diagram.json` y presionar el botón verde **Play** o ejecutar **Wokwi: Start Simulator** desde la paleta de comandos. El archivo `wokwi.toml` apunta al firmware generado por PlatformIO.
3. Abrir `dashboard/index.html` en el navegador con el modo demo apagado. Cuando el indicador diga **MQTT: conectado**, verificar que N3 aparezca en línea y que la lectura cambie al ajustar la temperatura del DHT22 del simulador.
4. Para ejecutar más nodos, compilar cada uno y abrir su carpeta en una ventana separada de VS Code. En N1, presionar el botón de registro de lote para iniciar la publicación periódica de temperatura.

El simulador ejecuta el firmware desde VS Code. Con la configuración incluida, los ESP32 simulados usan `Wokwi-GUEST` y se comunican por internet con el broker de HiveMQ.

> *Nota de portabilidad:* Cada carpeta es 100 % compatible con la versión web de [wokwi.com](https://wokwi.com); si se desea correr en el navegador, basta con crear un proyecto ESP32 y copiar los 5 archivos de la carpeta correspondiente.

---

## Paso 10: Reglas de Procesamiento (Edge en Nodos y Fog en Gateway)

### 1. Reglas de Decisión Local en el Edge (Nodos)
* **n1 (Recepción):** Al presionar el botón se genera el lote secuencial `L-AAMMDD-NN`. El nodo valida que la lectura de temperatura esté en rango físico ($-5^\circ\text{C}$ a $+40^\circ\text{C}$) y que no presente un salto abrupto $> 5^\circ\text{C}$ respecto a la lectura previa. Si es válida, marca `calidad="ok"`, publica y pone el LED en Verde.
* **n2 (Hidroenfriador):** Aplica lazo cerrado con histéresis: si $T_{\text{agua}} > 2.0^\circ\text{C}$ enciende bomba; si $T_{\text{agua}} \le 1.0^\circ\text{C}$ apaga bomba. Monitorea el tiempo de permanencia: si a los 90 min $T_{\text{pulpa}} > 2.0^\circ\text{C}$, emite alerta inmediata.
* **n3 (Cámara 1):** Control temporal autónomo de puerta abierta:
  * Si $\text{puerta\_abierta} > 60\text{ s} \implies$ activa buzzer de alarma local continuo.
  * Si $\text{puerta\_abierta} > 120\text{ s} \implies$ acciona servomotor a $90^\circ$ forzando el cierre físico de la compuerta sin esperar orden externa.
* **n4 (Cámara 2 - Modo Seguro Fail-Safe):** Si el nodo deja de recibir el mensaje de latido del gateway durante $60\text{ s}$ consecutivos, activa **Modo Seguro**:
  * Enciende el forzador/ventilador al 100 %.
  * Cierra la compuerta por servomotor.
  * Enciende LED testigo de modo degradado.
  * Rechaza cualquier orden remota de apagado (`fan_off`) si la temperatura medida se encuentra por encima de $2.0^\circ\text{C}$.
* **n5 (Andén):** Al detectar presencia mediante el sensor PIR, inicia cronómetro de permanencia de pallet:
  * $t < 50\% \implies$ LED Verde.
  * $50\% \le t < 100\% \implies$ LED Amarillo (preparar estiba inmediata).
  * $t \ge 100\% \implies$ LED Rojo intermitente y publicación de alerta crítica de ruptura en andén.
* **n6 (Energía):** Supervisa el estado de la red eléctrica comercial:
  * Si detecta corte de red comercial (`red == 0`), aguarda $10\text{ s}$ por la orden firmada del Gateway.
  * Si a los $10\text{ s}$ el Gateway no ha emitido orden, el nodo `n6` **arranca autónomamente el grupo electrógeno por relé local**.
  * Si tras $30\text{ s}$ de arranque el grupo no entrega tensión, publica alerta crítica de falla de respaldo eléctrico.

### 2. Reglas Combinadas en el Gateway Fog
* **Trazabilidad Acumulada de Frío (Algoritmo B):** El gateway rastrea el lote activo a lo largo de las etapas. Mantiene un integrador de minutos en los que la temperatura de pulpa supera los $2.0^\circ\text{C}$.
  $$\text{MinutosFuera} = \sum_{k} \Delta t_k \quad \text{para } T_k > 2.0^\circ\text{C}$$
  * Si $\text{MinutosFuera} \ge 80\% \text{ del límite (96 min)} \implies$ Dispara **Pre-alerta de proceso** identificando la etapa actual.
  * Si $\text{MinutosFuera} \ge 100\% \text{ (120 min)} \implies$ Dispara **Alarma de Ruptura Irreversible** y bloquea la liberación del lote.
* **Lógica de Control de Demanda Eléctrica:** Si durante horario punta la potencia supera el umbral máximo ($P > 360\text{ kW}$ durante $30\text{ s}$), ejecuta desconexión escalonada de cargas según tabla de prioridades:
  1. Climatización de oficinas ($-25\text{ kW}$).
  2. Ventilación secundaria de andén ($-15\text{ kW}$).
  3. Línea de empaque de palta al 50 % ($-45\text{ kW}$).
  4. Hidroenfriador en pausa entre lotes ($-15\text{ kW}$).
  * *Compresores de cámaras frigoríficas: Prioridad absoluta, **nunca se desconectan**.*

---

## Paso 11: Comparación A (Edge) vs B (Fog) con Datos Propios

Para cumplir con la exigencia de **datos propios medidos**, se instrumentó el monitor serie del microcontrolador Gateway ESP32 (`n7_gateway`) con marcas de tiempo en microsegundos (`micros()`) para registrar las líneas de telemetría y actuación `LAT,...` (ver [`simulador/mediciones_latencia.md`](simulador/mediciones_latencia.md) y [`simulador/latencias_reales_wokwi.log`](simulador/latencias_reales_wokwi.log)).

### 1. Latencias Reales Medidas en Banco de Pruebas / Wokwi
* **Alternativa A (Edge Local):**
  * Muestreo del sensor digital (DHT22 / DS18B20): $25\text{ ms}$.
  * Periodo de muestreo: $2.00\text{ s}$.
  * Evaluación de regla local y disparo de actuador (relé/buzzer): $10\text{ ms}$.
  * **Latencia Total Medida en Edge:** $\mathbf{2.12\text{ s}}$ (registrada en `LAT,A,... det_ms=2120`).
* **Alternativa B (Fog Gateway):**
  * Muestreo y cálculo de firma HMAC en nodo: $2.01\text{ s}$.
  * Transmisión MQTT hacia Broker y llegada a Gateway: $180\text{ ms}$.
  * Verificación HMAC-SHA256 y acumulador por lote: $13\text{ ms}$.
  * Generación y transmisión de orden firmada al actuador: $200\text{ ms}$.
  * **Latencia Total Medida en Fog:** $\mathbf{2.98\text{ s}}$ (registrada en `LAT,B,... det_ms=2980`).

### 2. Resultados Consolidados de Simulación Montecarlo (2 500 corridas)
Alimentando el simulador (`simulador/comparar.py`) con los parámetros medidos (`--lat-edge 2.12 --lat-fog 2.98 --runs 500`):

| Métrica Evaluada | Alternativa A (Edge - Umbral Simple) | Alternativa B (Fog - Minutos Acumulados) | Criterio de Aceptación |
|---|:---:|:---:|:---:|
| **Detección de rupturas reales** | **100.0 %** (1 423 / 1 423) | **100.0 %** (1 423 / 1 423) | Ambos detectan todas las anomalías |
| **Tasa de Falsas Alarmas** | **74.7 %** (805 falsas alarmas) | **1.1 %** (12 falsas alarmas) | **B cumple con creces el límite ($\le 5\%$)** |
| **Localización Correcta de la Etapa** | **81.0 %** | **96.4 %** | **B supera el mínimo exigido ($\ge 90\%$)** |
| **Anticipación Promedio** | $23.7\text{ min}$ (casi siempre falso positivo) | $4.0\text{ min}$ (pre-alerta real de proceso) | B avisa antes del daño biológico irreversible |
| **Latencia Detección $\to$ Actuación** | **$2.12\text{ s}$** (medido) | **$2.98\text{ s}$** (medido) | Ambas cumplen la norma ($\le 5\text{ s}$) |

### 3. Análisis Crítico y Decisión de Ingeniería
* **Por qué falla A como decisor de lote:** La alternativa A basa su disparo en la temperatura instantánea del aire. Cuando una puerta se abre brevemente para ingresar un pallet, el aire asciende a $6^\circ\text{C}$ durante 3 minutos; sin embargo, la pulpa del espárrago (con alta inercia térmica, $\tau \approx 90\text{ min}$) no sufre daño biológico. La regla A produce **74.7 % de falsas alarmas**, lo que paralizaría el despacho de exportación en packing.
* **Por qué B es la solución óptima:** La alternativa B calcula el daño biológico acumulado en el tiempo. Tolera transitorios breves de aire y solo alarma cuando la pulpa ha absorbido suficiente energía térmica, logrando un **96.4 % de precisión en localizar la etapa responsable**.
* **Decisión Final de Arquitectura (Solución Híbrida):**
  * **La regla Edge (A)** se reserva exclusivamente para la **acción mecánica local inmediata** (sonar alarma a los 60 s y cerrar compuerta a los 120 s).
  * **La regla Fog (B)** es la **autoridad decisoria** para emitir alarmas de proceso, auditar la trazabilidad y autorizar el despacho del lote.

---

## Paso 12: Dashboard (5 Pestañas, ThingSpeak y Pantalla TFT)

El sistema cuenta con una interfaz web en `dashboard/index.html` estructurada en 5 pestañas de operación:

```
┌────────────────────────────────────────────────────────────────────────┐
│  AGROEXPORTADORA VALLE DE ICA — SISTEMA DE GESTIÓN DE FRÍO (G2)        │
├──────────────┬──────────────┬──────────────┬──────────────┬────────────┤
│ 1. Problema  │ 2. Solución  │ 3. Operación │ 4. Seguridad │ 5. Reporte │
└──────────────┴──────────────┴──────────────┴──────────────┴────────────┘
```

1. **Pestaña 1 (Problema):** Contexto agronómico de exportación de espárrago en Ica ($> 38^\circ\text{C}$ ambiental), pérdidas históricas ficticias (USD 75 000 por 3 lotes rechazados), causas raíz identificadas y restricciones normativas.
2. **Pestaña 2 (Solución y Arquitectura):** Diagrama de las 5 capas industriales, funciones asignadas a cada nodo, justificación técnica y visualización del esquema de hardware Cirkit.
3. **Pestaña 3 (Operación en Vivo):**
   * Diagrama dinámico del recorrido del lote activo por las 4 etapas (`Recepción` $\to$ `Hidroenfriador` $\to$ `Cámara` $\to$ `Andén`).
   * Cuadrícula con el panel de cada integrante/nodo en tiempo real (temperaturas, estados de relés, switches y latidos `online`/`offline`).
   * Secuencia cronológica de eventos: *Detecta $\to$ Decide $\to$ Actúa $\to$ Confirma*.
4. **Pestaña 4 (Ciberseguridad y Ataques):** Banco de pruebas interactivo con 7 ataques simulados (uno por nodo) y tabla de auditoría en tiempo real demostrando el rechazo de órdenes falsas y la continuidad ininterrumpida del frío.
5. **Pestaña 5 (Resultados y Comparación):**
   * Gráficos de barras comparativos de Alternativa A vs Alternativa B (Falsas alarmas y Localización).
   * Contenedor embebido de **ThingSpeak** para monitoreo histórico de tendencias desde la nube.
   * **Monitoreo TFT del Gateway:** Réplica en pantalla de la información desplegada en el display local ILI9341 del Gateway (hora RTC, lote activo, temperatura de pulpa y estado del grupo electrógeno).

---

## Sección Transversal: Ciberseguridad, Ataques Simulados y Defensas

Se implementó una defensa en profundidad que asegura que ningún nodo acepte órdenes no autorizadas, ataques de repetición o corrupción de telemetría:

| Nodo Atacado | Tipo de Ataque Inyectado | Descripción del Vector de Ataque | Mecanismo de Defensa Implementado | Acción y Resultado Observado |
|---|---|---|---|---|
| **n1** | **Lote Falsificado** | Inyección de lote duplicado o con formato inválido (`HACK!`). | Validación de expresión regular `L-AAMMDD-NN` y búfer anular de 24 lotes históricos únicos. | **Rechazado:** El nodo descarta el registro, emite alerta `lote_duplicado` y mantiene el lote legítimo. |
| **n2** | **Falso «Lote Enfriado»** | Envío de orden no autorizada para apagar la bomba simulando que el lote ya alcanzó $2^\circ\text{C}$. | Validación de firma HMAC y comprobación de coherencia física ($T_{\text{pulpa}} > 2^\circ\text{C}$). | **Rechazado:** Firma inválida y violación de enclavamiento físico. La bomba sigue funcionando. |
| **n3** | **Suplantación de Identidad** | Inyección de telemetría falsa con temperatura de $-5^\circ\text{C}$ firmada con clave espuria. | Verificación de firma HMAC-SHA256 con la clave secreta compartida del nodo. | **Rechazado:** El gateway detecta firma errónea, descarta la lectura y publica alerta de suplantación. |
| **n4** | **Replay de Orden `fan_off`** | Reenvío de una orden válida capturada previamente para apagar ventiladores en cámara caliente. | Comprobación estricta de contador monotónico (`ctr <= lastCtr`) y enclavamiento de seguridad ($T > 2^\circ\text{C}$). | **Rechazado:** Descartado por contador antiguo y regla de protección térmica local. |
| **n5** | **Inundación de Mensajes (DoS)** | Ráfaga masiva de 200 mensajes en 4 segundos ($50\text{ msg/s}$) buscando saturar la cola del nodo. | Algoritmo *Token Bucket* limitador de tasa local: máximo 5 mensajes por segundo. | **Contenido:** Los mensajes excedentes se eliminan en buffer; el procesador preserva la lógica de control. |
| **n6** | **Orden Falsa: Apagar Grupo** | Comando malicioso `gen_stop` durante un apagón comercial en curso. | Doble validación: verificación de firma HMAC + regla dura de hardware: *«Prohibido apagar grupo si red == 0»*. | **Rechazado:** El relé permanece energizado; el grupo electrógeno continúa alimentando las cámaras. |
| **n7** | **Modificación de Configuración** | Intento de alterar los setpoints de temperatura del Gateway (`umbral_max = 8`). | Procedimiento de dos pasos: requiere firma criptográfica administrativa y pulsación física del botón ACK. | **Rechazado:** Timeout de 15 segundos sin pulsación de operador; configuración previa restaurada intacta. |

---

## Guía de Puesta en Marcha y Validación

### 1. Preparación de Entorno Local
```bash
# Clonar o ubicarse en la carpeta del proyecto
cd "g2-cadena-frio"

# 1. Configurar credenciales y claves de seguridad
cp common/secrets.example.h common/secrets.h
# (Editar secrets.h si se desean personalizar las claves HMAC)

# 2. Sincronizar el contrato común y claves hacia todos los nodos
bash tools/sync.sh
```

### 2. Ejecución de Pruebas Unitarias y de Contrato
```bash
# Ejecutar suite de pruebas de ciberseguridad, HMAC, anti-replay e integridad de diagramas
python3 tests/test_contract.py
```
*Salida esperada:* `Ran 13 tests ... OK`.

### 3. Ejecución del Simulador Comparativo A vs B
```bash
# Ejecutar simulación estocástica montecarlo (genera dashboard/resultados.js)
python3 simulador/comparar.py --limite 20 --umbral 2.0 --runs 300
```

### 4. Lanzamiento del Dashboard
1. Copiar el archivo de configuración:
   ```bash
   cp dashboard/config.example.js dashboard/config.local.js
   ```
2. Configurar el canal y clave de lectura de ThingSpeak en `dashboard/config.local.js`.
3. Abrir `dashboard/index.html` en un navegador web. El indicador **MQTT: conectado** confirma el enlace del navegador con HiveMQ por WebSocket seguro en el puerto 8884. Si aparece **sin conexión · reintentando**, revisar la consola del navegador y comprobar que la red permita ese puerto.
4. Para recibir telemetría en **Operación en vivo**, iniciar los nodos en Wokwi. Los nodos se conectan al mismo broker por TCP en el puerto 1883 y publican bajo `upc/g2`. El gateway muestra `MQTT conectado` en su monitor serie cuando establece su enlace.
5. Usar **Modo demo (sin broker)** para ver datos simulados. El indicador MQTT sigue mostrando el estado del enlace del navegador. ThingSpeak muestra el histórico en una conexión independiente.

Para recorrer un lote con Wokwi, iniciar el gateway y registrar el lote con el botón rojo de N1. El gateway lo asigna a los nodos siguientes. Cuando N2 reporta la pulpa del lote, aparece **Hidroenfriado**. Ajustar el DS18B20 de pulpa de N2 a **1 °C** después de al menos 20 minutos simulados de enfriamiento para pasar a **Cámara**. Activar el movimiento del PIR de N5 para pasar a **Andén**. Los minutos fuera de rango se acumulan en la etapa actual.

Para ver una secuencia de actuación, dejar el interruptor de confirmación del grupo en **0** en N6. Simular un corte de red y, después de ver la orden `gen_start`, cambiar la confirmación del grupo a **1**. El dashboard muestra el corte, la decisión, la orden y la lectura que confirma la marcha del grupo.

---
*G2 Agroindustria — Laboratorio de IoT Industrial.*
