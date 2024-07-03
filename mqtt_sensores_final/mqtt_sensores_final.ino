#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <SensirionI2cSht3x.h>


/************************instance************************************/
// I2C D22-SCL, D21-SDA
WiFiClient espClient;
PubSubClient client(espClient);

//--------Variables para SHT3x--------------------------
SensirionI2cSht3x sensor;
static char errorMessage[64];
static int16_t error;

/************************Hardware Related Macros************************************/

const int calibrationLed = 13;                      //when the calibration start , LED pin 13 will light up , off when finish calibrating
const int MQ135_PIN = 27;                                //define which analog input channel you are going to use
const int MQ4_PIN = 35;                                //define which analog input channel you are going to use
int RL_VALUE = 1;                                     //define the load resistance on the board, in kilo ohms
float RO_MQ135_CLEAN_AIR_FACTOR = 3.6;                     //RO_CLEAR_AIR_FACTOR=(Sensor resistance in clean air)/RO,
float RO_MQ4_CLEAN_AIR_FACTOR = 4.4;                     //RO_CLEAR_AIR_FACTOR=(Sensor resistance in clean air)/RO,
                                                    //which is derived from the chart in datasheet
 
/***********************Software Related Macros************************************/
int CALIBARAION_SAMPLE_TIMES = 50;                    //define how many samples you are going to take in the calibration phase
int CALIBRATION_SAMPLE_INTERVAL = 500;                //define the time interal(in milisecond) between each samples in the
                                                    //cablibration phase
int READ_SAMPLE_INTERVAL = 50;                        //define how many samples you are going to take in normal operation
int READ_SAMPLE_TIMES = 5;                            //define the time interal(in milisecond) between each samples in 
                                                    //normal operation
 
/**********************Application Related Macros**********************************/
/*********************MQ135*********************/
#define         GAS_NH3               0   
#define         GAS_CO2               1   

/*********************MQ4*********************/
#define         GAS_CH4               2    
 
/*****************************Globals***********************************************/
/*********************MQ135*********************/
float           NH3Curve[3]  =  {1, 0.4, -0.39};   //two points are taken from the curve. 
                                                    //with these two points, a line is formed which is "approximately equivalent"
                                                    //to the original curve. 
                                                    //data format:{ x, y, slope}; point1: (lg10, 2.6), point2: (lg200, 1) 
float           CO2Curve[3]  =  {1, 0.36,-0.34};    //two points are taken from the curve. 
                                                    //with these two points, a line is formed which is "approximately equivalent" 
                                                    //to the original curve.
                                                    //data format:{ x, y, slope}; point1: (lg10, 2.4), point2: (lg200, 1.1)) 

/*********************MQ4*********************/
float           CH4Curve[3] ={2.3, 0.25, -0.35};    //two points are taken from the curve. 
                                                    //with these two points, a line is formed which is "approximately equivalent" 
                                                    //to the original curve.
                                                    //data format:{ x, y, slope}; point1: (lg200, 1.8), point2: (lg10000,  0.44)                                                     
float           Ro_MQ135         =  10;             //Ro is initialized to 10 kilo ohms
float           Ro_MQ4           =  10;             //Ro is initialized to 10 kilo ohms

long lastMsg;
char msg[50];
int value = 0;

//******* Variables para enviar al broker
float temperatura = 0;
float humedad = 0;
long iPPM_NH3 = 0;
long iPPM_CO2 = 0;
long iPPM_CH4 = 0;

//*******************SSID-Password servidor de internet
const char* ssid = "NETWORK";
const char* password = "12345678";

//******************Direccion del MQTT Broker IP address
const char* mqtt_server = "192.168.0.10/24";

void setup() {
  Serial.begin(115200);
  Wire.begin();
  sensor.begin(Wire, SHT30_I2C_ADDR_44);

  sensor.stopMeasurement();
  delay(1);
  sensor.softReset();
  delay(100);
  uint16_t aStatusRegister = 0u;
  error = sensor.readStatusRegister(aStatusRegister);
  if (error != NO_ERROR) {
      Serial.print("Error trying to execute readStatusRegister(): ");
      errorToString(error, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
      return;
  }
  Serial.print("aStatusRegister: ");
  Serial.print(aStatusRegister);
  Serial.println();
  error = sensor.startPeriodicMeasurement(REPEATABILITY_MEDIUM,
                                          MPS_ONE_PER_SECOND);
  if (error != NO_ERROR) {
      Serial.print("Error trying to execute startPeriodicMeasurement(): ");
      errorToString(error, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
      return;
  }

  setup_wifi();
  client.setServer(mqtt_server, 1883);

  pinMode(calibrationLed,  OUTPUT);
  digitalWrite(calibrationLed, HIGH);
  Serial.print("Calibrating...");

  Ro_MQ135 = MQCalibration(MQ135_PIN, RO_MQ135_CLEAN_AIR_FACTOR);                         //Calibrating the sensor. Please make sure the sensor is in clean air         
  Ro_MQ4 = MQCalibration(MQ4_PIN, RO_MQ4_CLEAN_AIR_FACTOR);                         //Calibrating the sensor. Please make sure the sensor is in clean air         
  digitalWrite(calibrationLed, LOW);

  Serial.println("done!");
  Serial.print("R0_MQ135: ");
  Serial.println(Ro_MQ135);
  
  Serial.print("R0_MQ4: ");
  Serial.println(Ro_MQ4);

  delay(1000);
}

void loop() {
  if (!client.connected()) {
   reconnect();
  }

  client.loop();

  long now = millis();
  if (now - lastMsg > 1000) {
    lastMsg = now;

    //************* Señal 1 que se envia al broker
    error = sensor.blockingReadMeasurement(temperatura, humedad);
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute blockingReadMeasurement(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }

    //Convertir el valor en char array
    char tempString[8];
    dtostrf(temperatura, 1, 2, tempString);
    Serial.print("Temperatura: ");
    Serial.println(tempString);
    client.publish("esp32/temperature", tempString); //Topic: "esp32/temperature"

    //**************** Señal 2 que se envia al broker
    //Convertir el valor en char array
    char humString[8];
    dtostrf(humedad, 1, 2, humString);
    Serial.print("Humedad: ");
    Serial.println(humString);
    client.publish("esp32/humidity", humString); //Topic: "esp32/humidity"

    //**************** Señal 3 que se envia al broker
    iPPM_NH3 = MQGetGasPercentage(MQRead(MQ135_PIN)/Ro_MQ135,GAS_NH3);

    //Convertir el valor en char array
    char NH3String[8];
    dtostrf(iPPM_NH3, 1, 2, NH3String);
    Serial.print("Amoniaco: ");
    Serial.println(NH3String);
    // client.publish("esp32/ammonia", NH3String); //Topic: "esp32/ammonia"
    
    // //**************** Señal 4 que se envia al broker
    iPPM_CO2 = MQGetGasPercentage(MQRead(MQ135_PIN)/Ro_MQ135,GAS_CO2);

    //Convertir el valor en char array
    char CO2String[8];
    dtostrf(iPPM_CO2, 1, 2, CO2String);
    Serial.print("Dioxido de carbono: ");
    Serial.println(CO2String);
    // client.publish("esp32/co2", CO2String); //Topic: "esp32/co2"
    
    // //**************** Señal que se envia al broker
    iPPM_CH4 = MQGetGasPercentage(MQRead(MQ4_PIN)/Ro_MQ4,GAS_CH4);

    //Convertir el valor en char array
    char CH4String[8];
    dtostrf(iPPM_CH4, 1, 2, CH4String);
    Serial.print("Metano: ");
    Serial.println(CH4String);
    client.publish("esp32/methane", CH4String); //Topic: "esp32/methane"
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


/****************** MQResistanceCalculation ****************************************
Input:   raw_adc - raw value read from adc, which represents the voltage
Output:  the calculated sensor resistance
Remarks: The sensor and the load resistor forms a voltage divider. Given the voltage
         across the load resistor and its resistance, the resistance of the sensor
         could be derived.
************************************************************************************/ 
float MQResistanceCalculation(int raw_adc){
  return ( ((float)RL_VALUE*(1023-raw_adc)/raw_adc));
}
 
/***************************** MQCalibration ****************************************
Input:   mq_pin - analog channel
Output:  Ro of the sensor
Remarks: This function assumes that the sensor is in clean air. It use  
         MQResistanceCalculation to calculates the sensor resistance in clean air 
         and then divides it with RO_CLEAN_AIR_FACTOR. RO_CLEAN_AIR_FACTOR is about 
         10, which differs slightly between different sensors.
************************************************************************************/ 
float MQCalibration(int mq_pin, float Ro){
  int i;
  float val=0;

  for (i=0;i<CALIBARAION_SAMPLE_TIMES;i++) {            //take multiple samples
    val += MQResistanceCalculation(analogRead(mq_pin));
    delay(CALIBRATION_SAMPLE_INTERVAL);
  }
  val = val/CALIBARAION_SAMPLE_TIMES;                   //calculate the average value
  val = val/Ro;                        //divided by RO_CLEAN_AIR_FACTOR yields the Ro                                        
  return val;                                                      //according to the chart in the datasheet 

}
 
/*****************************  MQRead *********************************************
Input:   mq_pin - analog channel
Output:  Rs of the sensor
Remarks: This function use MQResistanceCalculation to caculate the sensor resistenc (Rs).
         The Rs changes as the sensor is in the different consentration of the target
         gas. The sample times and the time interval between samples could be configured
         by changing the definition of the macros.
************************************************************************************/ 
float MQRead(int mq_pin){
  int i;
  float rs=0;
 
  for (i=0;i<READ_SAMPLE_TIMES;i++) {
    rs += MQResistanceCalculation(analogRead(mq_pin));
    delay(READ_SAMPLE_INTERVAL);
  }
 
  rs = rs/READ_SAMPLE_TIMES;
 
  return rs;  
}
 
/*****************************  MQGetGasPercentage **********************************
Input:   rs_ro_ratio - Rs divided by Ro
         gas_id      - target gas type
Output:  ppm of the target gas
Remarks: This function passes different curves to the MQGetPercentage function which 
         calculates the ppm (parts per million) of the target gas.
************************************************************************************/ 
long MQGetGasPercentage(float rs_ro_ratio, int gas_id){
  if ( gas_id == GAS_NH3 ) {
     return MQGetPercentage(rs_ro_ratio,NH3Curve);
  } else if ( gas_id == GAS_CO2 ) {
     return MQGetPercentage(rs_ro_ratio,CO2Curve);
  } else if ( gas_id == GAS_CH4 ) {
     return MQGetPercentage(rs_ro_ratio,CH4Curve);
  } 

  return 0;
}
 
/*****************************  MQGetPercentage **********************************
Input:   rs_ro_ratio - Rs divided by Ro
         pcurve      - pointer to the curve of the target gas
Output:  ppm of the target gas
Remarks: By using the slope and a point of the line. The x(logarithmic value of ppm) 
         of the line could be derived if y(rs_ro_ratio) is provided. As it is a 
         logarithmic coordinate, power of 10 is used to convert the result to non-logarithmic 
         value.
************************************************************************************/ 
long  MQGetPercentage(float rs_ro_ratio, float *pcurve){
  return (pow(10,( ((log(rs_ro_ratio)-pcurve[1])/pcurve[2]) + pcurve[0])));
}


