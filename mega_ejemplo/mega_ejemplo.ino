/*
 * Arduino Mega · ejemplo de envío de tramas al gateway ESP32
 *
 * No lee sensores reales: sirve para probar el sistema completo
 * (Mega → ESP32 → MQTT → backend) sin montar sensores.
 *
 * Dos formas de enviar datos:
 *   1. Automática: cada 5 s envía una trama con valores aleatorios.
 *   2. Manual: lo que se escriba en el Monitor Serie de la PC se reenvía al ESP32.
 *      Al escribir algo se pausa el envío automático; escriba "auto" para reanudarlo.
 *
 * Monitor Serie: 115200 baudios, final de línea "Nueva línea".
 * Ejemplo de envío manual (una línea por vez):
 *   TMP_INT:temperature:25.0:C
 *   SOIL_1:soil_moisture:40:%
 *   END
 *
 * Conexiones con el ESP32 (Serial1 del Mega, 9600 baudios):
 *   Mega TX1 (pin 18) → divisor 1 kΩ / 2 kΩ → ESP32 GPIO16 (RX)
 *   Mega RX1 (pin 19) ← ESP32 GPIO17 (TX)
 *   GND ↔ GND
 */

const unsigned long INTERVALO_MS = 5000;

unsigned long ultimoEnvio = 0;
bool automatico = true;

void setup() {
  Serial.begin(115200);  // PC (Monitor Serie)
  Serial1.begin(9600);   // ESP32: misma velocidad que el gateway
  randomSeed(analogRead(A0));

  Serial.println(F("Mega de ejemplo listo."));
  Serial.println(F("Envio automatico cada 5 s. Escriba una linea para enviarla al ESP32,"));
  Serial.println(F("o \"auto\" para activar/desactivar el envio automatico."));
}

void loop() {
  // Modo manual: reenviar lo que llega desde la PC
  if (Serial.available()) {
    String linea = Serial.readStringUntil('\n');
    linea.trim();

    if (linea.equalsIgnoreCase("auto")) {
      automatico = !automatico;
      Serial.println(automatico ? F("Envio automatico: ACTIVADO") : F("Envio automatico: PAUSADO"));
    } else if (linea.length() > 0) {
      if (automatico) {
        automatico = false;  // evita mezclar tramas manuales y automáticas
        Serial.println(F("Envio automatico PAUSADO (escriba \"auto\" para reanudarlo)"));
      }
      enviarLinea(linea);
    }
  }

  // Modo automático: una trama aleatoria cada INTERVALO_MS
  if (automatico && millis() - ultimoEnvio >= INTERVALO_MS) {
    ultimoEnvio = millis();
    enviarTramaAleatoria();
  }
}

void enviarTramaAleatoria() {
  enviarLinea("TMP_INT:temperature:" + String(aleatorio(20, 32), 1) + ":C");
  enviarLinea("HUM_INT:humidity:" + String(aleatorio(40, 85), 1) + ":%");
  enviarLinea("SOIL_1:soil_moisture:" + String(aleatorio(15, 70), 1) + ":%");
  enviarLinea("END");
}

// Envía una línea al ESP32 y la muestra en el Monitor Serie
void enviarLinea(const String& linea) {
  Serial1.println(linea);
  Serial.print(F("-> "));
  Serial.println(linea);
}

float aleatorio(float minimo, float maximo) {
  return minimo + random(0, 1001) / 1000.0 * (maximo - minimo);
}
