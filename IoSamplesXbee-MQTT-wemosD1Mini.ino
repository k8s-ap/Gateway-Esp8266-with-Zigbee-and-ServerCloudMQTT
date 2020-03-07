/*
 * Rama fusion/timestamp
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

// Connect Wemos-d1-mini pin Rx to TX of Coordinator-XbeeS2C device
// Connect Wemos-d1-mini pin Tx to RX of Coordinator-XbeeS2C device
// Remember to connect all devices to a common Ground: Coordinator-XbeeS2C, Wemos-d1-mini and TTL-USB-Serial device

*/
#include <XBee.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h> // How we connect to your local wifi
#include <PubSubClient.h>
#include <WiFiUdp.h> // UDP library which is how we communicate with Time Server
#include <TimeLib.h> // See Arduino Playground for details of this useful time synchronisation library

// Update these with values suitable for your network.
//const char* ssid = "TNTsport";
//const char* password = "9122018TresUno";
const char* mqtt_server = "mqtt.diveriot.com";
const int mqtt_port = 1883;
#define _ssid "TNTsport"
#define _password "9122018TresUno"

//------------BEGIN Declaracion feature timestamp--------------//
#define WifiTimeOutSeconds 10
unsigned int localPort = 8888; // Just an open port we can use for the UDP packets coming back in

// This is the "pool" name for any number of NTP servers in the pool.
// If you're not in the UK, use "time.nist.gov"
// Elsewhere: USA us.pool.ntp.org
// Read more here: http://www.pool.ntp.org/en/use.html
char timeServer[] = "time.nist.gov"; //"uk.pool.ntp.org";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP Udp; // A UDP instance to let us send and receive packets over UDP
const int timeZone = -3; // Your time zone relative to GMT/UTC. // Not used (yet)
String DoW[] = {"Domingo", "Lunes","Martes","Miercoles","Jueves","Viernes","Sabado"}; // Days of week. Day 1 = Sunday

// How often to resync the time (under normal and error conditions)
//#define _resyncSeconds 60
#define _resyncSeconds 3600 //300 is 5 minutos // 3600 is 1 hour. 86400 is on day
#define _resyncErrorSeconds 15
#define _millisMinute 60000

// forward declarations
void connectToWifi();
void printDigits(int digits);
void digitalClockDisplay();
time_t getNTPTime();

//------------FIN Declaracion feature timestamp--------------//


//------------BEGIN Declaracion feature mqtt and Xbee-Arduino--------------//
WiFiClient espClient;
PubSubClient client(espClient);
//long lastMsg = 0;
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


//------------FIN Declaracion feature mqtt and Xbee-Arduino--------------//


//-----------------------------------------------------------------------------
// SETUP     SETUP     SETUP     SETUP     SETUP     SETUP     SETUP     SETUP
//-----------------------------------------------------------------------------

void setup() { 
  //-------config de propositos compartidos entre todas las bibliotecas ---------//
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output.  //LED del Wemos D1. Para otra placa borrar esta variable con toda la funcione que implique
  Serial.begin(9600);           // Inicio comunicacion-serial con el XBeeS2C-Coordinador
  xbee.setSerial(Serial);     // Seteo/vinculo dicha conmunicacion-serial con el objeto xbee.
  // start soft serial
  Serial1.begin(9600);        // Inicio comunicacion-serial-1 para mostrar por pantalla los mensajes de depuración. ¿Y espero a que se abra el puerto declarado al inicio?
  WiFi.mode(WIFI_OFF);        //Prevents reconnection issue (taking too long to connect)
  //setup_wifi();  // ya no uso este primer metodo para conectarme a la red.  Luego eliminar
  connectToWifi();  // Connect to your local wifi (one time operation)

  //-------config timestamp---------//
  Udp.begin(localPort); // What port will the UDP/NTP packet respond on?
  setSyncProvider(getNTPTime); // What is the function that gets the time (in ms since 01/01/1900)?
  // How often should we synchronise the time on this machine (in seconds)?
  // Use 300 for 5 minutes but once an hour (3600) is more than enough usually
  // Use 86400 for 1 daybut once an hour (3600) is more than enough usually
  setSyncInterval(_resyncSeconds); // just for demo purposes!
  //setSyncInterval(300); // se sincronizara cada 5 minutos

  //-------config mqtt---------//
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}


//-----------------------------------------------------------------------------
// LOOP     LOOP     LOOP     LOOP     LOOP     LOOP     LOOP     LOOP     LOOP
//-----------------------------------------------------------------------------
void loop() {
  //-------logica timestamp---------//
  
  digitalClockDisplay(); // This just prints the "system time"  
  delay(0.1 * _millisMinute); // mins * millis in 1 min // Do other stuff etc, delay here just emulates that

  
  //-------logica mqtt---------//
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
                  snprintf (gas, 50, "{\"value\":\"Danger\", \"timestamp\":\"22:13:00\"}");
                  Serial1.print("Publish message topic 'Casa/Cocina/Gas': ");
                  Serial1.println(gas);
                  client.publish("Casa/Cocina/Gas", gas);
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

//-----------------------------------------------------------------------------
// DECLARATIONS METHODS AND FUNCTIONS   -   DECLARATIONS METHODS AND FUNCTIONS
//-----------------------------------------------------------------------------


// Prints a nice time display
//-----------------------------------------------------------------------------
void digitalClockDisplay() {
  // We'll grab the time so it doesn't change whilst we're printing it
  time_t t=now();

  //Now print all the elements of the time secure that it won't change under our feet
  //printDigits(hour(t)-3); // le sumo tres unidades para obtgener la hora correspondiente a mi zona (timezone=-3)
  printDigits(hour(t));
  Serial1.print(":");
  printDigits(minute(t));
  Serial1.print(":");
  printDigits(second(t));
  Serial1.print("    ");
  Serial1.print(DoW[weekday(t)-1]);
  Serial1.print(" ");
  printDigits(day(t));
  Serial1.print("/");
  printDigits(month(t));
  Serial1.print("/");
  printDigits(year(t));
  Serial1.println();
}

void printDigits(int digits) {
  // utility for digital clock display: prints leading 0
  if (digits < 10) Serial1.print('0');
  Serial1.print(digits);
}


// This is the function to contact the NTP pool and retrieve the time
//-----------------------------------------------------------------------------
time_t getNTPTime() {
  // Send a UDP packet to the NTP pool address
  Serial1.print("\nSending NTP packet to "); 
  Serial1.println(timeServer);
  sendNTPpacket(timeServer);  // Enviamos un paquete al servidor NTP para pedir los segundos desde 1900 y sincronizar

  // Wait to see if a reply is available - timeout after X seconds. At least
  // this way we exit the 'delay' as soon as we have a UDP packet to process
  #define UDPtimeoutSecs 5
  int timeOutCnt = 0;
  while (Udp.parsePacket() == 0 && ++timeOutCnt < (UDPtimeoutSecs * 10)){
    delay(100);
    yield(); //Pasa el control a otras tareas cuando se llama. Idealmente, yield () debería usarse en funciones que tardarán un tiempo en completarse
  }

  // Is there UDP data present to be processed? Sneak a peek!
  if (Udp.peek() != -1) {
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    // The time-stamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);

    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900)
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial1.print("Seconds since Jan 1 1900 = ");
    Serial1.println(secsSince1900);
 
    // now convert NTP time into everyday time:
    //Serial1.print("Unix time = ");

    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;

    // subtract seventy years and convert to time zone local:
    unsigned long epoch = secsSince1900 - seventyYears + timeZone * SECS_PER_HOUR;

    // Reset the interval to get the time from NTP server in case we previously changed it. Por ejemplo en caso de error del servidor que no responde y utilizo el _resyncErrorSeconds
    setSyncInterval(_resyncSeconds);

    // LED indication that all is well
    //digitalWrite(LED_BUILTIN,LOW);  // led de indicacion de que todo esta bien. 

    return epoch;
  }

  // Failed to get an NTP/UDP response
  Serial1.println("No response");
  setSyncInterval(_resyncErrorSeconds);

  return 0;
}

//-----------------------------------------------------------------------------
// send an NTP request to the time server at the given address
//-----------------------------------------------------------------------------
void sendNTPpacket(const char* address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  // all NTP fields have been given values, now you can send a packet requesting a timestamp:
  // Note that Udp.begin will request automatic translation (via a DNS server) from a
  // name (eg pool.ntp.org) to an IP address. Never use a specific IP address yourself,
  // let the DNS give back a random server IP address
  Udp.beginPacket(address, 123); //NTP requests are to port 123

  // Get the data back
  Udp.write(packetBuffer, NTP_PACKET_SIZE);

  // All done, the underlying buffer is now updated
  Udp.endPacket();
}

// -----------------------------------------------------------------------
// Establish a WiFi connection with your router
// -----------------------------------------------------------------------
void connectToWifi() {
  Serial1.print("Connecting to: ");
  Serial1.println(_ssid);
  WiFi.begin(_ssid, _password);

  // Try to connect 4 times a second for X seconds before timing out
  int timeout = WifiTimeOutSeconds * 4;
  while (WiFi.status() != WL_CONNECTED && (timeout-- > 0)) {
    delay(250);
    Serial1.print(".");
  }

  // Successful connection?
  if (WiFi.status() != WL_CONNECTED) {
    Serial1.println("\nFailed to connect, exiting");
    // Set some LED failure here, for example
    delay(1000);
    return;
  }

  Serial1.print("\nWiFi connected with (local) IP address of: ");
  Serial1.println(WiFi.localIP());
}




/*void setup_wifi() {
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
}*/

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
      client.subscribe("Casa/LivingRoom/Luz"); // Esto creo que esta de más? O me debo suscribir a tal TOPIC cuando me conecto por primera vez al serverMQTT?
    } else {
      Serial1.print("failed, rc=");
      Serial1.print(client.state());
      Serial1.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
