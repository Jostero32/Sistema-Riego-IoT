# Sistema de riego IoT

<p>
  <img alt="ESP32" src="https://img.shields.io/badge/ESP32-gateway-E7352C?logo=espressif&logoColor=white" />
  <img alt="MQTT" src="https://img.shields.io/badge/MQTT-Mosquitto_2-660066?logo=eclipsemosquitto&logoColor=white" />
  <img alt="Spring Boot" src="https://img.shields.io/badge/Spring_Boot-4-6DB33F?logo=springboot&logoColor=white" />
  <img alt="MongoDB" src="https://img.shields.io/badge/MongoDB-7-47A248?logo=mongodb&logoColor=white" />
  <img alt="Next.js" src="https://img.shields.io/badge/Next.js-14-000000?logo=nextdotjs&logoColor=white" />
  <img alt="Docker" src="https://img.shields.io/badge/Docker_Compose-2496ED?logo=docker&logoColor=white" />
</p>

Plataforma de monitoreo para un sistema de riego, desarrollada como proyecto de **vinculación con la
sociedad** (Universidad Técnica de Ambato). Un **Arduino Mega** envía las lecturas por puerto serie, un
**ESP32** hace de *gateway* y las publica por **MQTT**, y un backend **Spring Boot** las guarda en
**MongoDB** y las expone por **API REST** y **WebSocket** en tiempo real. Un **dashboard Next.js** muestra
los sensores en vivo junto al clima de referencia (Open-Meteo) y recomienda cuándo regar.

![Dashboard](docs/img/dashboard.png)

| Sensores en vivo | Riego | Alertas |
|---|---|---|
| <img src="docs/img/sensores.png" alt="Sensores" /> | <img src="docs/img/riego.png" alt="Riego" /> | <img src="docs/img/alertas.png" alt="Alertas" /> |

<sub>Capturas con las lecturas de ejemplo de `mongo-init/` y del simulador del ESP32.</sub>

> Este repositorio contiene el **gateway ESP32**, el **servidor** y el **dashboard** (`frontend/`). Para el Mega incluye un programa de
> **ejemplo** (`mega_ejemplo/`) que envía tramas de prueba, sin sensores reales.

## Arquitectura

```mermaid
flowchart LR
    subgraph CAMPO["En campo"]
        S["Sensores del cultivo<br/>(en el ejemplo: valores<br/>aleatorios o manuales)"] --> M["Arduino Mega 2560<br/>mega_ejemplo/"]
        M -->|"UART 9600<br/>trama de texto"| E["ESP32 gateway<br/>esp32_iot/"]
    end
    subgraph SERVIDOR["Servidor (docker compose)"]
        B[("Mosquitto<br/>:1883")]
        API["Spring Boot<br/>backend/"]
        DB[("MongoDB<br/>sensor_readings")]
        ME["mongo-express<br/>:8081"]
    end
    E -->|"MQTT QoS 1<br/>esp/sensores/&lt;deviceId&gt;"| B
    B -->|"suscripción esp/sensores/#"| API
    API --> DB
    ME --> DB
    API -->|"REST /api/v1"| C["Dashboard Next.js<br/>frontend/ :3000"]
    API -->|"STOMP /ws<br/>/topic/devices/&lt;id&gt;/readings"| C
    OM["Open-Meteo<br/>clima y pronóstico"] --> C
```

## Flujo de una lectura

```mermaid
sequenceDiagram
    participant M as Arduino Mega
    participant E as ESP32
    participant Q as Mosquitto
    participant S as Spring Boot
    participant D as MongoDB
    participant C as Cliente web
    M->>E: TMP_INT:temperature:32.1:°C … END
    E->>E: parsea la trama y agrega hora NTP (UTC)
    E->>Q: publica JSON en esp/sensores/esp32_01
    Q->>S: entrega (suscripción esp/sensores/#)
    S->>D: guarda una lectura por sensor
    S-->>C: STOMP /topic/readings/live y /topic/devices/esp32_01/readings
    C->>S: GET /api/v1/readings/latest?deviceId=esp32_01
```

## Hardware

![Conexión Arduino Mega – ESP32](docs/esquema-conexiones.svg)

| Conexión | ESP32 | Arduino Mega | Nota |
|---|---|---|---|
| ESP32 → Mega | GPIO17 (TX2) | RX1 (pin 19) | Directo: 3,3 V se lee como nivel alto en el Mega |
| Mega → ESP32 | GPIO16 (RX2) | TX1 (pin 18) | **Con divisor 1 kΩ / 2 kΩ**: el Mega emite 5 V y el ESP32 trabaja a 3,3 V |
| Tierra | GND | GND | Obligatoria |
| Portal de configuración | GPIO0 (BOOT) | — | Presionado al encender |
| Estado | GPIO2 (LED integrado) | — | Ver patrones abajo |

> **Importante:** los pines del ESP32 no admiten 5 V. Sin el divisor, la señal TX del Mega puede dañar el
> GPIO16 con el tiempo. El divisor (1 kΩ en serie y 2 kΩ a GND) deja la señal en unos 3,3 V.

| LED (GPIO2) | Estado |
|---|---|
| Encendido fijo | Wi-Fi y MQTT conectados |
| Parpadeo lento (1 s) | Wi-Fi conectado, sin MQTT (reintenta cada 5 s) |
| Parpadeo medio (0,5 s) | Sin Wi-Fi, reconectando |
| Parpadeo rápido (0,2 s) | Modo AP: portal de configuración activo |

## Programa de ejemplo del Mega (`mega_ejemplo/`)

Es lo más simple posible: no lee sensores, solo envía tramas al ESP32 por `Serial1` (9600 baudios).

- **Automático:** cada 5 s envía temperatura, humedad y humedad del suelo con valores aleatorios.
- **Manual:** lo que se escribe en el Monitor Serie de la PC (115200 baudios, «Nueva línea») se reenvía al
  ESP32. Al escribir se pausa el modo automático; `auto` lo reanuda.

```
-> TMP_INT:temperature:27.4:C
-> HUM_INT:humidity:63.1:%
-> SOIL_1:soil_moisture:38.9:%
-> END
```

Compila con Arduino IDE, placa **Arduino Mega or Mega 2560** (3 % de la flash). Para usar sensores reales,
basta con reemplazar los valores de `enviarTramaAleatoria()` por las lecturas y conservar el formato.

## Protocolos

**Trama serie (Mega → ESP32)**, una línea por sensor y `END` al final:

```
TMP_INT:temperature:32.1:°C
HUM_INT:humidity:60.2:%
SOIL_1:soil_moisture:41.5:%
END
```

**Mensaje MQTT (ESP32 → broker)**, tópico `esp/sensores/<deviceId>`, QoS 1:

```json
{
  "metadata": { "deviceId": "esp32_01", "timestamp": "2026-11-25T04:16:35Z" },
  "payload": [
    { "sensorId": "TMP_INT", "type": "temperature",   "value": 32.1, "unit": "°C" },
    { "sensorId": "HUM_INT", "type": "humidity",      "value": 60.2, "unit": "%" },
    { "sensorId": "SOIL_1",  "type": "soil_moisture", "value": 41.5, "unit": "%" }
  ]
}
```

## Inicio rápido (servidor)

```bash
docker compose up -d --build
```

| Servicio | Dirección |
|---|---|
| Dashboard | <http://localhost:3000> |
| Swagger UI | <http://localhost:8080/swagger-ui.html> |
| API REST | <http://localhost:8080/api/v1/devices> |
| WebSocket (STOMP) | `ws://localhost:8080/ws` |
| MQTT (para el ESP32) | `<IP-del-PC>:1883` |
| mongo-express | <http://localhost:8081> |

La primera vez MongoDB se llena con 1000 lecturas de ejemplo de 10 dispositivos (`mongo-init/01-seed.js`).
Puertos y credenciales se cambian con un `.env` (ver `.env.example`).

### Probar sin hardware

```bash
./tools/simular-esp32.sh            # publica 10 lecturas de esp32_01, una cada 3 s
curl "http://localhost:8080/api/v1/readings/latest?deviceId=esp32_01"
```

El script publica por MQTT exactamente el JSON que arma el firmware, así que prueba el mismo camino que el ESP32.
Con el dashboard abierto en el dispositivo `esp32-1`, `./tools/simular-esp32.sh 20 2 esp32-1` muestra los valores
cambiando en vivo (el indicador **WS** queda en verde).

![Swagger UI](docs/img/swagger.png)

## API

| Método | Ruta | Descripción |
|---|---|---|
| `GET` | `/api/v1/readings` | Lecturas paginadas |
| `GET` | `/api/v1/readings/latest?deviceId=` | Última lectura de cada sensor de un dispositivo |
| `GET` | `/api/v1/readings/history` | Histórico con filtros (`deviceId`, `type`, `sensorId`, `from`, `to`) |
| `GET` | `/api/v1/readings/stats?deviceId=` | Mínimo, máximo, promedio y cantidad por sensor |
| `GET` | `/api/v1/devices` | Dispositivos registrados |
| `GET` | `/api/v1/devices/{deviceId}/sensors` | Sensores de un dispositivo |

| Tópico STOMP | Contenido |
|---|---|
| `/topic/readings/live` | Todas las lecturas nuevas |
| `/topic/devices/{deviceId}/readings` | Lecturas de un dispositivo |
| `/topic/devices/{deviceId}/sensors/{sensorId}` | Lecturas de un sensor |

Detalle de respuestas y ejemplos: [`backend/API_REST_WEBSOCKET.md`](backend/API_REST_WEBSOCKET.md) ·
peticiones listas para probar: [`backend/api-tests.http`](backend/api-tests.http).

## Firmware del gateway (`esp32_iot/`)

| Módulo | Responsabilidad |
|---|---|
| `esp32_iot.ino` | Arranque, bucle principal, NTP, armado del JSON, LED |
| `SerialParser` | Lee la trama línea por línea hasta `END` |
| `NetManager` | Conexión Wi-Fi con reintentos y modo AP |
| `MqttManager` | Cliente PubSubClient (buffer de 1 KB) y reconexión |
| `WebPortal` | Portal de configuración en `http://192.168.4.1` |
| `ConfigManager` | Configuración persistente en NVS |

**Compilar:** Arduino IDE 2.x, placa **ESP32 Dev Module** (core esp32 3.3.x) y las librerías
**PubSubClient** 2.8 y **ArduinoJson** 7.x desde el gestor de librerías. Compila sin advertencias y usa el
76 % de la flash.

**Configurar:** sin Wi-Fi guardado, o con **BOOT** presionado al encender, el ESP32 crea la red
`ESP32_IoT_<deviceId>`. En `http://192.168.4.1` se configuran el Wi-Fi, el broker MQTT (host, puerto y
tópico base) y el identificador del dispositivo. Si pierde el Wi-Fi y no se reconecta tras 5 intentos,
vuelve al modo AP.

**Probar sin el Mega:** el firmware también lee tramas desde el **Monitor Serie** (115200 baudios). Escriba
las líneas de la trama y `END`, y el ESP32 las publica igual que si vinieran del Mega.

## Limitaciones y pendientes

- El programa del **Mega con sensores reales** no forma parte del proyecto: `mega_ejemplo/` solo genera
  tramas de prueba con el formato que espera el gateway.
- Mosquitto acepta conexiones **anónimas** y sin TLS, y las credenciales de MongoDB son de desarrollo: antes
  de exponerlo fuera de la red local hay que activar usuarios en el broker y cambiar las credenciales.
- Las reconexiones Wi-Fi del firmware son bloqueantes durante los reintentos.

## Estructura

```
├── esp32_iot/          Firmware del gateway ESP32 (Arduino)
├── mega_ejemplo/       Programa de ejemplo del Mega: tramas aleatorias o escritas desde la PC
├── backend/            Spring Boot 4 · Java 21 · MQTT (Paho) · MongoDB · STOMP · springdoc
├── mosquitto/config/   Configuración del broker
├── mongo-init/         Datos de ejemplo
├── tools/              Simulador del ESP32 por MQTT
├── docs/               Esquema de conexiones y capturas
└── docker-compose.yml
```

## Créditos

Proyecto de vinculación desarrollado en equipo. El **dashboard** (`frontend/`) es obra de
[Leo538](https://github.com/Leo538); se incorporó con su historial de commits desde
[Vinculacion-Sistema-Riego](https://github.com/Leo538/Vinculacion-Sistema-Riego). El firmware del gateway, el
backend y la infraestructura Docker son de [Jostero32](https://github.com/Jostero32).
