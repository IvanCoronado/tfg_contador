//https://www.pjrc.com/teensy/td_libs_TimerOne.html
#include <TimerOne.h>
//http://cactus.io/hookups/sensors/temperature-humidity/am2302/hookup-arduino-to-am2302-temp-humidity-sensor
#include "cactus_io_AM2302.h"
#include <SPI.h>
#include <Ethernet.h>


#define COUNT_ID 1            // bbdd ID for counter sensor
#define TEMPERATURE_ID 2      // bbdd ID for temperature sensor
#define HUMIDITY_ID 3         // bbdd ID for humidity sensor

#define SENSOR_EXTERNAL_PIN 7 // what pin on arduino is the external sensor data line connected to
#define SENSOR_INTERNAL_PIN 6 // what pin on arduino is the internal sensor data line connected to
#define AM2302_PIN 5          // what pin on the arduino is the DHT22 data line connected to

/*
 * ETHERNET VARIABLES
 */
 
// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
IPAddress server(192,168,1,84);  // numeric IP (no DNS)
//char server[] = "http://localhost";    // name address (using DNS)

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192,168,1,200);

// Initialize the Ethernet client library
// with the IP address and port of the server 
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;


/*
 * COUNTER SENSOR VARIABLES
 */

int externalOn;                       //Value of SENSOR_EXTERNAL_PIN
int internalOn;                       //Value of SENSOR_INTERNAL_PIN

/*
 * We are goin to use a state machine
 *         extOff   (externalOff and internalOn)
 *          ^v
 *         idle     (externalOn and internalOn) goTo extOff when !externalOn /// goTo intOff when !internalOn
 *          ^v
 *         intOff    (externalOn and internalOff)
 */         
//volatile for shared variables
volatile int state = 0;              
volatile int idle = 0;  
int extOff = 1;
int intOff = 2;


/*
 * TEMPERATURE/HUMIDITY SENSOR VARIABLES
 */
long previousMillis = 0;
long interval = 60000;  //2min
// Initialize DHT sensor for normal 16mhz Arduino. 
AM2302 dht(AM2302_PIN);

void setup() {
  Serial.begin(9600);
  
  Timer1.initialize(2500000); //2,5 seg
  
  pinMode(SENSOR_EXTERNAL_PIN, INPUT);
  pinMode(SENSOR_INTERNAL_PIN, INPUT);

  Serial.println("Ethernet start");
  // start the Ethernet connection:
  Ethernet.begin(mac, ip);

  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.println("Ethernet started");

  dht.begin();

}



void loop() {
  externalOn = digitalRead(SENSOR_EXTERNAL_PIN);
  internalOn = digitalRead(SENSOR_INTERNAL_PIN);
  
  unsigned long currentMillis = millis();
  
  if(currentMillis - previousMillis > interval) {
    Serial.println("AQUI!!");
      // save the last time you send the data
      previousMillis = currentMillis;   
      dht.readHumidity();
      dht.readTemperature();

      // JSON Object compatible:
      // data =  {
      //            "value": 25.5
      //         }
      
      char temp[10];
      String tempAsString;
      String jsonDataTEMP;
      
      dtostrf(dht.temperature_C,1,2,temp);
      tempAsString = String(temp);
      
      jsonDataTEMP = "{\"value\":"+tempAsString+"}";
      postCount(jsonDataTEMP, TEMPERATURE_ID);

      // JSON Object compatible:
      // data =  {
      //            "value": 25.5
      //         }
      
      char hum[10];
      String humAsString;
      String jsonDataHUM;
      
      dtostrf(dht.humidity,1,2,hum);
      humAsString = String(hum);
      
      jsonDataHUM = "{\"value\":"+humAsString+"}";
      postCount(jsonDataHUM, HUMIDITY_ID);
    
  }
  if (state == idle) {
    Timer1.detachInterrupt();
    if (!externalOn && internalOn) {                    //If something block externalOn
      Serial.println("Sensor external cortado");
      state = extOff;                                   //GOTO state = extOff
      Timer1.attachInterrupt(backToIdle);               //Active interruption, so if the internalOn doesnt get blocked we backToIdle state in 2,5sec
 
    } else if (!internalOn && externalOn) {             //If something block internalOn
      Serial.println("Sensor internal cortado");
      state = intOff;                                   //GOTO state = intOff
      Timer1.attachInterrupt(backToIdle);               //Active interruption, so if the internalOn doesnt get blocked we backToIdle state in 2,5sec
    } 
    
    delay(100);
  }

  if (state == extOff) { 
    if (!internalOn && externalOn) {                    //If someone block internalOn while we are in extOff status, we count 1
      Serial.println("+1");
      // JSON Object compatible:
      // data =  {
      //            "value": 1
      //         }
      char data[50] = "{"
                      "\"value\":1"
                      "}";
      postCount(data, COUNT_ID);
      state = idle;                                     //And we go back to idle status
      delay(100);
    }
    
  }

  if (state == intOff) { 
    if (!externalOn && internalOn) {                    //If someone block internalOn while we are in intOff status, we count -1
      Serial.println("-1");
      // JSON Object compatible:
      // data =  {
      //            "value": -1
      //         }
      char data[50] = "{"
                      "\"value\":-1"
                      "}";
      postCount(data, COUNT_ID);
      state = idle;
      delay(100);
    }
  }
 

}

void backToIdle(void){
  state = idle;
  Serial.println("Reset");
 
}

//   FUNCTION: postCount()   //
void postCount(String data, int idDevice)
{
  Serial.println("--------------in post-------------------------");
  Serial.println(data);
  if (client.connect(server, 8080)) // Connect to server
  {
    Serial.println("--------------START POST-------------------------");
    // HTTP POST Headers
    client.print("POST /0/devices/");
    client.print(idDevice);
    client.println("/counts HTTP/1.1");  
    client.println("Host: localhost");  
    client.println("User-Agent: Arduino/1.0");  
    client.println("Connection: close"); 
    client.println("Content-Type: application/json");  
    client.println("Content-Length: 15");
    client.println();   
          
    // HTTP POST Body
    client.print(data);
         
    client.stop();
    
  } else{
     Serial.println("--------------error-------------------------");
  }
}


