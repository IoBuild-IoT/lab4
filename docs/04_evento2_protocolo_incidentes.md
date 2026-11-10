# Evento 2 — Falla del protocolo de incidentes (gateway reiniciado de fábrica)

> Adaptado a la cadena de frío (ver nota del Evento 1). «Husos/compresores» se traducen a cargas no críticas vs. compresores de cámara; «humidificadores» al lazo de humedad de las cámaras.

## 1. Fallas del protocolo versión 1 (≥ 4)

1. **Sin respaldo de configuración:** el reinicio de fábrica borró setpoints, claves y tabla de cargas.
2. **Reinicio sin autorización ni segundo par de ojos:** un solo técnico podía reiniciar el gateway.
3. **Sin validación antes de reanudar:** las órdenes erróneas llegaron a los actuadores (HR 80 % → lote dañado).
4. **Sin reversión (rollback):** no existía versión anterior ni procedimiento para volver.
5. **Orden de desconexión de cargas incorrecto:** se apagó producción (husos) en vez de cargas no críticas.
6. **Los actuadores confiaban ciegamente en el gateway:** ninguna regla de seguridad local impedía HR > 95 %.
7. **Reanudación en bloque:** todo arrancó a la vez al volver la energía (sin escalonar).

## 2. Gestión de configuración

| Elemento | Regla |
|---|---|
| Qué se versiona | `setpoints`, umbrales, claves (en bóveda aparte), tabla de cargas, mapa de nodos |
| Respaldo | Copia automática **cada cambio** y diaria a microSD + copia fuera de línea (USB) firmada con HMAC |
| Versión | `cfg-AAAAMMDD-NN` + hash; el gateway publica la versión activa en `upc/g2/gw/estado` |
| Quién autoriza | **Dos personas**: solicitante (técnico) + autorizador (jefe de planta); el gateway ya exige doble confirmación en 15 s con firma |
| Registro | Cada cambio: hora, quién, versión anterior→nueva, motivo, en el log de seguridad |
| Restauración | `cfg restore <versión>` (también firmado y con doble confirmación) |

## 3. Validación obligatoria antes de reanudar y reversión

**Lista de verificación (todas deben pasar; el gateway no pasa a «RUN» sin ellas):**

1. Energía estable ≥ 5 min en la red (no solo el grupo).
2. Configuración activa = última versión respaldada (hash coincide).
3. Los 6 nodos `online` con latido < 30 s.
4. Lecturas con `calidad=ok` y dentro de rango físico en todas las cámaras.
5. **Prueba en seco:** una orden de cada actuador crítico con confirmación (bomba, ventilador, servo, grupo) y vuelta al estado seguro.
6. Humedad de cámara 85–95 % y T 0–2 °C **medidas** por 2 sensores.
7. Aprobación del jefe de planta (botón de reconocimiento en el gateway).

**Reanudación escalonada:** 1) compresores de cámaras, 2) ventiladores/humidificación, 3) hidroenfriador, 4) andén e iluminación, 5) empaque y oficinas, con 60 s entre cada paso y verificación.

**Reversión:** si cualquier verificación falla o una cámara sale de rango en los 15 min tras reanudar → el gateway vuelve a **modo seguro** (compresores + ventilador ON, humidificador OFF, puertas cerradas) y restaura la última configuración buena automáticamente; se alerta al jefe de planta.

**Protección local (edge):** el actuador de humedad ignora cualquier orden que lleve HR > 95 % (límite duro en el firmware del nodo), igual que n4 ya rechaza `fan_off` con T > 2 °C.

## 4. Orden de desconexión de cargas (corregido)

Cortar **primero lo menos crítico y último lo vital**:

1. Oficinas/climatización → 2. Iluminación/ventilación del andén (a mínimo) → 3. Línea de empaque/palta → 4. Hidroenfriador (tras terminar el lote en curso) → **nunca** compresores de cámaras ni el gateway/nodos/UPS.
Reconexión en orden inverso. Una orden «apagar» sobre una carga de prioridad crítica es **rechazada** por regla de seguridad, como ya hace n6 con el grupo electrógeno durante un apagón.

## 5. Lecciones aprendidas (registro «Z8»)

> «Z8» no figura en los documentos del lab; se asume que es el registro de lecciones aprendidas. Si es otro formato, copiar estas filas.

| # | Qué pasó | Causa raíz | Acción correctiva | Dueño | Fecha |
|---|---|---|---|---|---|
| 1 | Gateway reiniciado de fábrica | Sin respaldo ni doble autorización | Backup automático + doble confirmación | Integrante 7 | — |
| 2 | HR 80 % dañó un lote | Actuador obedeció orden errónea | Límite duro HR ≤ 95 % en el nodo | Dueño del nodo | — |
| 3 | Se apagó producción en vez de cargas no críticas | Orden de corte mal definido | Tabla de prioridades versionada | Integrante 6 | — |
| 4 | Nadie validó antes de reanudar | Protocolo sin check obligatorio | Lista de 7 verificaciones bloqueante | Integrante 7 | — |
| 5 | No se pudo revertir | Sin versión previa | Restauración firmada a versión anterior | Integrante 7 | — |
