/*
Basic ESP8266 MQTT example

 This sketch demonstrates the capabilities of the pubsub library in combination
 with the ESP8266 board/library.

 It connects to an MQTT server "mqtt.diveriot.com:1883 then:
  - publishes "the samples" to the topic "casa/xbee-coordinator" every receives I/O samples from a remote radio (two seconds)
  - subscribes to the topic "casa/livingroom/luz", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "casa/livingroom/luz" is an 1, envia un frame al xbee-router para ENCENDER luz, sino envia un frame al xbee-router para APAGAR luz(switch ON the ESP Led,
    else switch it off)

 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.

 To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"

  ---------------------
 
This example is for Series 2 (ZigBee) XBee Radios only
Receives I/O samples from a remote radio.
The remote radio must have IR > 0 and at least one digital or analog input enabled.
The XBee coordinator should be connected to the Wemos-D1-Mini.
 
This example uses the SoftSerial library to view the XBee communication.  I am using a 
Modern Device USB BUB board (http://moderndevice.com/connect) and viewing the output
with the Arduino Serial Monitor.
*/
#include <XBee.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Connect Wemos-d1-mini pin Rx to TX of Coordinator-XbeeS2C device
// Connect Wemos-d1-mini pin Tx to RX of Coordinator-XbeeS2C device
// Remember to connect all devices to a common Ground: Coordinator-XbeeS2C, Wemos-d1-mini and TTL-USB-Serial device

// Update these with values suitable for your network.
const char* ssid = "TNTsport";
const char* password = "9122018TresUno";
const char* mqtt_server = "mqtt.diveriot.com";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
//long lastMsg = 0;
char msg[50];
char gas[50];
char motion[50];
char door[50];
int value = 0; //reemplazar el nombre de esta variable por stampTime

XBee xbee = XBee();
ZBRxIoSampleResponse ioSample = ZBRxIoSampleResponse();
XBeeAddress64 routerXBeeAddress = XBeeAddress64(0x0013A200, 0x41809E95); //Luego mas adelante, reemplazar nombre por uno mas descriptivo como por ejemplo: remoteXBeeAddress
XBeeAddress64 endDeviceXBeeAddress = XBeeAddress64(0x0013A200, 0x4180A081);
float nodeHighAddress; // para identificar quien de quien es el frame, del EndDevice o del Router
float nodeLowAddress;

void setup_wifi() {
  //luego reemplazar esta funcion por otra que utiliza la libreria wifi-multi.h
  delay(10);
  // We start by connecting to a WiFi network
  Serial1.println();
  Serial1.print("Connecting to ");
  Serial1.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial1.print(".");
  }

  randomSeed(micros());//genera un numero aletaorio que parte del parametro micros() que devuelve el tiempo(hora:mm:ss:mls) en el que inicio el programa en la placa wemos-d1-mini

  Serial1.println("");
  Serial1.println("WiFi connected");
  Serial1.println("IP address: ");
  Serial1.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  //El WemosD1mini posee esta logica para estar atento si hay nuevas publicaciones en el topic suscrito (casa/living/luz) y en caso afirmativo
  //enviar un frame al Xbee-Router para que activar/desactivar Relee(foco)
  
  // Switch on the LED if an 1 was received as first character//envia un frame al XBee-Router(XBee-terminal) con "1" para activar el Relee que enciende el foco de luz
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial1.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ClienteWemosD1Mini-";
    clientId += String(random(0xffff), HEX);//cada vez que nos reconectamos, debemos hacerlo con un nuevo clientID (no el mismo).
    // Attempt to connect
    if (client.connect(clientId.c_str())) { //c_str() Convierte el contenido de una cadena a una cadena de estilo C. Nunca debe modificar la cadena a través del puntero devuelto.
      Serial1.println("connected");
      // Once connected, publish an announcement...
      client.publish("Log", "Dispositivo ESP8266ClientHome reconectado al servidor mqtt://mqtt.diveriot.com:1883");
      // ... and resubscribe
      client.subscribe("Casa/Living/Luz"); // Esto creo que esta de más? O me debo suscribir a tal TOPIC cuando me conecto por primera vez al serverMQTT?
    } else {
      Serial1.print("failed, rc=");
      Serial1.print(client.state());
      Serial1.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() { 
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output.  //LED del Wemos D1. Para otra placa borrar esta variable con toda la funcione que implique
  Serial.begin(9600);           // Inicio comunicacion-serial con el XBeeS2C-Coordinador
  xbee.setSerial(Serial);     // Seteo/vinculo dicha conmunicacion-serial con el objeto xbee.
  // start soft serial
  Serial1.begin(9600);        // Inicio comunicacion-serial-1 para mostrar por pantalla los mensajes de depuración.
  WiFi.mode(WIFI_OFF);        //Prevents reconnection issue (taking too long to connect)
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  //attempt to read a packet (intento leer un frame)    
  xbee.readPacket();

  if (xbee.getResponse().isAvailable()) {
    // got something (tengo algo)

    if (xbee.getResponse().getApiId() == ZB_IO_SAMPLE_RESPONSE) {
      //verifico que aún estoy conectado al serverMQTT
      if (!client.connected()) {
        reconnect();
      }
      client.loop();  //Para que el cliente procese los mensajes entrantes y mantenga su conexión con el servidor MQTT.
      
      //obtengo el Sample-IO del frame recibido y lo guardo en una variable llamada "ioSample"
      xbee.getResponse().getZBRxIoSampleResponse(ioSample);

      //Serial1.print("Received I/O Sample from: ");
      //Serial1.print(ioSample.getRemoteAddress64().getMsb(), HEX);  
      //Serial1.print(ioSample.getRemoteAddress64().getLsb(), HEX);  
      //Serial1.println("");
      
      //comparar si la terminacion es la direccion del nodo router. 
      if (routerXBeeAddress == ioSample.getRemoteAddress64() ) {
        //Serial1.println("Received I/O Sample from ROUTER");
        if (ioSample.containsDigital()) {
          //Serial1.println("Sample contains digtal data");
          
          // check digital inputs
          for (int i = 0; i <= 12; i++) {
            // si es DI0 (Sensor de movimiento del nodo router)
            if (i == 0) {
              // si DIO0 (pin 20) esta seteado como Digital Input (previamente realizado con XCTU)
              if (ioSample.isDigitalEnabled(i)) {                
                if (ioSample.isDigitalOn(i) == 0) {
                  // todo normal                  
                  //Serial1.println("No se detecto movimiento");                  
                  snprintf (motion, 50, "{\"value\":\"False\", \"timestamp\":\"22:13:00\"}");
                  Serial1.print("Publish message on topic 'Casa/Patio/Motion001': ");
                  Serial1.println(motion);
                  client.publish("Casa/Patio/Motion001", motion); //Falta agregarle stampTime (marca de tiempo)
                }
                if (ioSample.isDigitalOn(i) == 1) {
                  // Se detecto movimiento
                  //Serial1.println("Advertencia! Se detecto movimiento.");
                  snprintf (motion, 50, "{\"value\":\"True\", \"timestamp\":\"22:17:00\"}");
                  Serial1.print("Publish message on topic 'Casa/Patio/Motion001': ");
                  Serial1.println(motion);
                  client.publish("Casa/Patio/Motion001", motion); //{\"value\":\"msg\", \"timestamp\":\"22:13:00\"}
                }                
              }
              else {  
                Serial1.println("I/O Sample no posee al PIN 20 (DIO0) seteado como Digital-Input. Debe habilitarlo con la herramienta XCTU");              
              }
            }
            /*-----------------------------*/
            // si es DI2 (Sensor de puerta)
            if (i == 2) {
              // si DIO2 (pin 18) esta seteado como Digital Input (previamente realizado con XCTU)
              if (ioSample.isDigitalEnabled(i)) {                
                if (ioSample.isDigitalOn(i) == 0) {
                  // todo normal                  
                  //Serial1.println("Puerta cerrada");                  
                  snprintf (door, 50, "{\"value\":\"Closed\", \"timestamp\":\"22:17:00\"}");
                  Serial1.print("Publish message on topic 'Casa/LivingRoom/Door': ");
                  Serial1.println(door);
                  client.publish("Casa/LivingRoom/Door", door);
                }
                if (ioSample.isDigitalOn(i) == 1) {
                  // Se detecto apertura de puerta
                  //Serial1.println("Advertencia! Puerta abierta");
                  snprintf (door, 50, "{\"value\":\"Open\", \"timestamp\":\"22:17:00\"}");
                  Serial1.print("Publish message on topic 'Casa/LivingRoom/Door': ");
                  Serial1.println(door);
                  client.publish("Casa/LivingRoom/Door", door); //{value:msg, timestamp:22:13:00} // cambiar nombre de la variable msg por value
                }                
              }
              else {  
                Serial1.println("I/O Sample no posee al PIN 18 (DIO2) seteado como Digital-Input. Debe habilitarlo con la herramienta XCTU");              
              }
            }
            
          }
        }
        
      }
      
      //Compara si el frame proviene del nodo EndDevice
      if (endDeviceXBeeAddress == ioSample.getRemoteAddress64() ) {
        //Serial1.println("Received I/O Sample from END-DEVICE");
        //El frame contiene un sample con dato Digital
        if (ioSample.containsDigital()) {
          //Serial1.println("Sample contains digtal data");          
          // check digital inputs
          for (int i = 0; i <= 12; i++) {
            // si es DI0 (Sensor de GAS)
            if (i == 0) {
              // si DIO0 (pin 20) esta seteado como Digital Input (previamente realizado con XCTU)
              if (ioSample.isDigitalEnabled(i)) {                
                if (ioSample.isDigitalOn(i) == 1) {
                  // todo normal                  
                  //Serial1.println("No se detecto niveles peligrosos de dioxido de carbono / gas");
                  
                  /**/          
                  //aqui envio el contenido del sample al servidor mqtt, y pruebo la latencia 
                  //(comparandolo con DIGI monitor?), NOTA: debo elaborar un test de velocidad de 
                  //samples recibidos en el coordinador y verificar que estos llegaron bien al servidor mqtt
                  
                  /*
                   * Aunque con sprintf todo funciona bien, recomiendo utilizar snprintf() ya que a éste último 
                   * le debemos decir el tamaño de la variable a escribir y si los valores hacen la cadena más larga de lo que debería, 
                   * ésta será recortada, es decir, si reservamos 50 bytes para la cadena, con sprintf() tal vez se escriban más, 
                   * depende de los datos a escribir, pero snprintf() escribirá 50.
                  */
                  snprintf (gas, 50, "{\"value\":\"Normal\", \"timestamp\":\"22:13:00\"}");
                  Serial1.print("Publish message on topic 'Casa/Cocina/Gas': ");
                  Serial1.println(gas);
                  client.publish("Casa/Cocina/Gas", gas);
                  /**/        
                }
                if (ioSample.isDigitalOn(i) == 0) {
                  // Se detecto fuga de gas o dioxido de carbono                  
                  //Serial1.println("Peligro!.Se detecto Dioxido de Carbono / Gas");

                  /**/                            
                  /*
                   * Aunque con sprintf todo funciona bien, recomiendo utilizar snprintf() ya que a éste último 
                   * le debemos decir el tamaño de la variable a escribir y si los valores hacen la cadena más larga de lo que debería, 
                   * ésta será recortada, es decir, si reservamos 50 bytes para la cadena, con sprintf() tal vez se escriban más, 
                   * depende de los datos a escribir, pero snprintf() escribirá 50.
                  */
                  snprintf (msg, 50, "{\"value\":\"Danger\", \"timestamp\":\"22:13:00\"}");
                  Serial1.print("Publish message topic 'Casa/Cocina/Gas': ");
                  Serial1.println(msg);
                  client.publish("Casa/Cocina/Gas", msg);
                  /**/        
                }
                //Serial1.print("Digital (DI");
                //Serial1.print(i, DEC);
                //Serial1.print(") is ");
                //Serial1.println(ioSample.isDigitalOn(i), DEC);
              }
              else {  
                Serial1.println("I/O Sample no contiene ningun PIN seteado como Digital-Input. Debe habilitarlo con la herramienta XCTU");              
              }
            }
            
          }
        } 
           
      }
     
      
      //if (ioSample.containsAnalog()) {
      //  Serial1.println("Sample contains analog data");
      //}
      // read analog inputs
      //for (int i = 0; i <= 4; i++) {
      //  if (ioSample.isAnalogEnabled(i)) {
      //   Serial1.print("Analog (AI");
      //   Serial1.print(i, DEC);
      //    Serial1.print(") is ");
      //    Serial1.println(ioSample.getAnalog(i), DEC);
      //  }
      //}

      
      
      // method for printing the entire frame data
      //for (int i = 0; i < xbee.getResponse().getFrameDataLength(); i++) {
      //  nss.print("byte [");
      //  nss.print(i, DEC);
      //  nss.print("] is ");
      //  nss.println(xbee.getResponse().getFrameData()[i], HEX);
      //}
    } 
    else {
      Serial1.print("Expected I/O Sample, but got ");
      Serial1.print(xbee.getResponse().getApiId(), HEX);
    }    
  } else if (xbee.getResponse().isError()) {
    Serial1.print("Error reading packet.  Error code: ");  
    Serial1.println(xbee.getResponse().getErrorCode());
  }
}
