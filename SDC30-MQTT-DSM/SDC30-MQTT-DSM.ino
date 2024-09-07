#include <WiFi.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <MQ137.h>
#include <Wire.h>
#include "Adafruit_SCD30.h"

const int MQ137_PIN = 35; // Cambia A0 si usas otro pin analógico

// Inicialmente, usa un valor estimado para R0 (puedes cambiarlo después de la calibración)
float R0_inicial = 1; 

MQ137 sensor_mq137(MQ137_PIN, R0_inicial, true); // true para habilitar mensajes de depuración

Adafruit_SCD30 scd30;

/************************instance************************************/
// I2C D22-SCL, D21-SDA
WiFiClient espClient;
PubSubClient client(espClient);

/************************Hardware Related Macros************************************/

const int calibrationLed = 13;                      //when the calibration start , LED pin 13 will light up , off when finish calibrating

// Tiempo de espera entre mediciones (en microsegundos)
const int SLEEP_DURATION = 120e6; // 10 segundos

long lastMsg;
char msg[50];
int value = 0;

//******* Variables para enviar al broker
float temperatura = 0;
float humedad = 0;
long iPPM_NH3 = 0;
long iPPM_CO2 = 0;

//*******************SSID-Password servidor de internet
const char* ssid = "IZZI-22EA";
const char* password = "3PmpFrGpQCc3";

//******************Direccion del MQTT Broker IP address
const char* mqtt_server = "192.168.0.214";

uint64_t uuid = ESP.getEfuseMac(); // Use MAC as UUID
char payload[100]; 

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);

  sensor_mq137.begin(); // Inicia el sensor y muestra información de depuración

  // Calibración: 
  Serial.println("Calibrando... Asegúrate de que el sensor esté en aire limpio.");
  delay(20000); // Espera 20 segundos para que el sensor se estabilice
  float R0_calibrado = sensor_mq137.getRo();
  Serial.print("Valor de R0 calibrado: ");
  Serial.println(R0_calibrado);

  // Actualiza el valor de R0 en el objeto del sensor
  sensor_mq137 = MQ137(MQ137_PIN, R0_calibrado, true); 

  if (!scd30.begin()) {
    Serial.println("No se pudo encontrar el sensor SCD30, verifica la conexión.");
  }

  // Activa el modo de sueño profundo después del setup inicial
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION);
}

void loop() {
  iPPM_NH3 = sensor_mq137.getPPM();
  temperatura = scd30.temperature;
  humedad = scd30.relative_humidity;

  if (!client.connected()) {
   reconnect();
  }

  client.loop();

  Serial.print("Concentración de NH3 (ppm): ");
  Serial.println(iPPM_NH3);

  if (scd30.dataReady()) {
    if (!scd30.read()) {
      Serial.println("Error al leer los datos del sensor");
      return;
    }
    iPPM_CO2 = scd30.CO2;

  }

  long now = millis();
  if(now - lastMsg > 1000) {
      lastMsg = now;
      if(iPPM_CO2 > 0){
        if(iPPM_CO2 < 100){
          iPPM_CO2 += 350;
        }
        else if(iPPM_CO2 >= 100 && iPPM_CO2 < 200){
          iPPM_CO2 += 300;
        }
        else if(iPPM_CO2 >= 200 && iPPM_CO2 < 300){
          iPPM_CO2 += 100;
        }
        Serial.print("CO2: "); Serial.print(iPPM_CO2); Serial.println(" ppm");
        Serial.print("Humedad: "); Serial.print(humedad); Serial.println(" %");
        Serial.print("Temperatura: "); Serial.print(temperatura); Serial.println(" °C");
        Serial.println();
        //**************** Señal 2 que se envia al broker
        //Convertir el valor en char array
        char humString[8];
        dtostrf(humedad, 1, 2, humString);
        Serial.print("Humedad: ");
        Serial.println(humString);
        client.publish("esp32/humidity", humString); //Topic: "esp32/humidity"

        //**************** Señal 3 que se envia al broker
        //Convertir el valor en char array
        char NH3String[8];
        dtostrf(iPPM_NH3, 1, 2, NH3String);
        Serial.print("Amoniaco: ");
        Serial.println(NH3String);
        client.publish("esp32/ammonia", NH3String); //Topic: "esp32/ammonia"

        // //**************** Señal 4 que se envia al broker
        //Convertir el valor en char array
        char CO2String[8];
        dtostrf(iPPM_CO2, 1, 2, CO2String);
        Serial.print("Dioxido de carbono: ");
        Serial.println(CO2String);
        client.publish("esp32/co2", CO2String); //Topic: "esp32/co2"

        // //**************** Señal que se envia al broker

        //Convertir el valor en char array
        char tempString[8];
        dtostrf(temperatura, 1, 2, tempString);
        Serial.print("Temperatura: ");
        Serial.println(tempString);
        client.publish("esp32/temp", tempString); //Topic: "esp32/temp"

        delay(1000);
        esp_deep_sleep_start();
        // snprintf(payload, sizeof(payload), "mac:%012llx, CO2:%.2f, NH3:%.2f, temp:%.2f, hum:%.2f", uuid, iPPM_CO2, iPPM_NH3, temperatura, humedad);
        // client.publish("/sensors", payload);
    }
  } 
} 

void setup_wifi(){
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while(!client.connected()) {
    Serial.print("Intentando conexion MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("conectado");
    }
    else {
      Serial.print("Fallo, rc=");
      Serial.print(client.state());
      Serial.print(" Intentelo de nuevo en 5s");
      delay(5000);
    }
  }
}
