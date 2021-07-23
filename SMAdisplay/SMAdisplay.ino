/*
  SMAdisplay

  This sketch displays
  - current supplyed Power into the power grid
  - current consumed Power from the power grid
  - current earned Power from solar
  - energy supplyed Power into the power grid for the current day
  - energy consumed Power from the power grid for the current day
  - energy earned Power from solar for the current day
  
  The circuit:
  * green led on D0
  * red led on D5
  * switch on D3
  * OLED display on I2C

  Created 01.07.2021
  By Gerhard Waidelich / Luisa Waidelich

  blog to this project
  http://photovoltaik-power.com

*/

// please adapt this values to your environment

#include "config.h"

#include <SPI.h>
#include <ModbusIP_ESP8266.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <sys/time.h>


// SCL GPIO5
// SDA GPIO4
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

#define LED_GREEN D0
#define LED_RED D5
#define TASTER D3
#define LED_ON HIGH
#define LED_OFF LOW


#define BUFFER_SIZE 700
#define PORT_MULTI 9522 // See Description of SMA-EnergyMeter

// registers of speedwire
#define POS_170 32
#define LEN_170 4
#define POS_180 52
#define LEN_180 4
#define POS_CON 40
#define LEN_CON 8
#define POS_SUP 60
#define LEN_SUP 8

WiFiUDP Udp; // An EthernetUDP instance to let us send and receive packets over UDP
ModbusIP mb;  //ModbusIP object
const int POWER_REG = 30775;            
const int TIME_REG = 30193;
const int POWER_ACC_REG = 30513;         

IPAddress remote(IP_ADDRESS_SMA);  // Address of Modbus Slave device
uint32_t ACpower_rev = 0;
uint64_t ACpowerAccumulated_rev = 0;
uint32_t timestamp_rev = 0;
uint32_t timestamp = 0;
uint32_t startDaySupplyCounter = 0;
uint32_t startDayConsumeCounter = 0;
uint32_t startDayACPowerCounter = 0;

// display data
int display_screen = -1;
int currentDay = 0;
int consume = 0;
int supply = 0;
int consumeCounter = 0; // in mWh
int supplyCounter = 0; // in mWh
uint32_t ACpower = 0;
uint64_t ACpowerAccumulated = 0;

bool cb_trans(Modbus::ResultCode event, uint16_t transactionId, void* data)
{
         
      if (event != 0) {
         Serial.print("error code:");
         Serial.println(event);
        

      } else {
        if (data != 0) {
       
         /*Serial.print("data in:  ");
         Serial.println(event);
         Serial.println(transactionId);*/
         
      }
      }
      
     
    return true;
  }

uint64_t swap64(uint64_t in) {
   uint64_t out;
    out = (uint64_t)*((uint16_t *)&in + 3) | (uint64_t)*((uint16_t *)&in + 2) << 16 | (uint64_t)*((uint16_t *)&in + 1) << 32 | in << 48;
    return out;
}

uint32_t swap32(uint32_t in) {
   uint32_t out;
   out = (uint64_t)*((uint16_t *)&in + 1) | in << 16;
   return out;
}


struct tm* timeinfo;

void Check_Time(void) {
  struct timeval tv=  { timestamp, 0 };
  
  settimeofday(&tv, NULL);
 
  time_t now;
  time(&now);
  timeinfo = localtime(&now);
  Serial.println(asctime(timeinfo));
}

int last_click;
void ICACHE_RAM_ATTR isr_taster() {
  if (millis() - last_click < 200) {
    return;
  }
  last_click = millis();
  display_screen += 1;
  display_screen = display_screen % 3;
  draw_display();
  Serial.println("click.");  
}
void setup() {
  last_click = millis();
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(TASTER, INPUT_PULLUP);
  
  
  digitalWrite(LED_GREEN, LED_ON);
  digitalWrite(LED_RED, LED_ON);

  // zeit setzen
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/ 3", 1); // https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
  tzset();
  
  // Multicast Adress of the SMAEnergymeter
  IPAddress ipMulti(IP_ADRESS_ENERGY_METER);
    
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Made");   
  display.println("by");   
  display.println("Luisa");   
  display.display();
  
  WiFi.begin(WIFI_HOME, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // start the UDP
  Udp.beginMulticast(WiFi.localIP(), ipMulti, PORT_MULTI);
    

  attachInterrupt(digitalPinToInterrupt(TASTER), isr_taster, FALLING );

  digitalWrite(LED_GREEN, LED_OFF);
  digitalWrite(LED_RED, LED_OFF);
  digitalWrite(LED_BUILTIN, LED_ON);

  // Clear the screen
  display.clearDisplay();
  display.display();
}

void draw_display() {

    if (display_screen < 0) {
      return;
    }
    
    display.clearDisplay();

    // screen saver
    if (millis() - last_click > 10*60*1000) {
      display_screen = -1;
      display.display();
      return;
    }
    // draw screen 
    display.drawLine(display_screen*10,0,display_screen*10+10,0, WHITE);

    switch (display_screen) {
      case 0:
        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.setCursor(10,5);
         
        if (supply > 0 && consume == 0) {
          display.println("GOOD");      
        } else if (consume > 0 && supply == 0) {
          display.println("BAD");     
        } else {
           display.println("???");
        }
    
        display.setTextSize(1);
        display.setCursor(2,28);
        display.print(supply-consume);
        display.print(" Watt");
        display.setCursor(2,41);
        display.print(ACpower);
        display.print(" Watt");
        break;
      case 1:
        display.setTextSize(1);
        display.setCursor(0,5);
        display.println("Heute");
        display.println("verkauft:");
        display.print(" ");
        display.print((consumeCounter-startDayConsumeCounter));
        display.println(" Wh");

        display.println("gekauft:");
        display.print(" ");
        display.print((supplyCounter-startDaySupplyCounter));
        display.print(" Wh");   
        break;     
      case 2:
        display.setTextSize(1);
        display.setCursor(0,5);
        display.println("Heute von der Sonne empfangen:");
        display.println();
        display.print(" ");
        display.print(ACpowerAccumulated-startDayACPowerCounter);
        display.print(" Wh");
    }
    
    display.display();
}

void loop() {
 // buffers for receiving data
  byte packetBuffer[BUFFER_SIZE];

  int packetSize = 0; // The real size of the packet
  int count = 0; // Counter for the errors while recive a packet

 
  // try to recive packet from Udp max. 10 times
  // and wait 200 millisec. between
  while ((packetSize <= 0) && (count < 10)) {
    count++;
    packetSize = Udp.parsePacket();
    delay(200);
   
  }

  if (packetSize) {
    // read all packets, and use the last
    while (packetSize > 0) {
     
         Serial.print("*");
        // read the packet into packetBuffer
        if (packetSize > BUFFER_SIZE) {
          Udp.read(packetBuffer, BUFFER_SIZE);
        } else {
          Udp.read(packetBuffer, packetSize);
        }
        // next packet
        packetSize = Udp.parsePacket();
      }

    // Read "PowerIn" from SMA-Telegram
    uint64_t energyValue = 0;
    for (count = 0; count < LEN_170; count++) {
      energyValue = (energyValue * 256) + packetBuffer[(POS_170 + count)];
    }
    consume = (energyValue / 10);
    
    // Read "PowerOut" from SMA-Telegram
    energyValue = 0;
    for (count = 0; count < LEN_180; count++) {
      energyValue = (energyValue * 256) + packetBuffer[(POS_180 + count)];
    }
    supply = (energyValue / 10);

   // Read "PowerOutCounter" from SMA-Telegram
    energyValue = 0;
    for (count = 0; count < LEN_SUP; count++) {
      energyValue = (energyValue * 256) + packetBuffer[(POS_SUP + count)];
    }
    consumeCounter = (energyValue / 3600);

    // Read "PowerIn" from SMA-Telegram
    energyValue = 0;
    for (count = 0; count < LEN_CON; count++) {
      energyValue = (energyValue * 256) + packetBuffer[(POS_CON + count)];
    }
    supplyCounter = (energyValue / 3600);
    
    if (display_screen >= 0) {
      digitalWrite(LED_BUILTIN, LED_OFF);   // Turn the LED on (Note that LOW is the voltage level
      delay(100);
      digitalWrite(LED_BUILTIN, LED_ON);   // Turn the LED on (Note that LOW is the voltage level
    }
    
    /* test only!
    digitalWrite(LED_RED, LED_ON);
    delay(200);
    digitalWrite(LED_RED, LED_OFF);
    
    digitalWrite(LED_GREEN, LED_ON);
    delay(200);
    digitalWrite(LED_GREEN, LED_OFF);
     end test 
    */

    draw_display();

    // set LED
    if (supply > 0 && consume == 0) {  
        digitalWrite(LED_GREEN, LED_ON);
        digitalWrite(LED_RED, LED_OFF);
    } else if (consume > 0 && supply == 0) {
        digitalWrite(LED_GREEN, LED_OFF);
        digitalWrite(LED_RED, LED_ON);
    }
        
    Serial.print(consume);
    Serial.print(" ");
    Serial.print(supply);
    Serial.print(" ");
    Serial.println(ACpower);
    
    Serial.println(digitalRead(TASTER));
  

    // check modbus
    // Try to connect
    int ret=0;
  
    if (!mb.isConnected(remote)) {
      ret= mb.connect(remote);
       //Serial.println("connected");
    }
    if (ret == 1) 
    { 
     //Serial.println("get value");
      
      int ret = mb.readHreg(remote,POWER_REG,(uint16_t *)&ACpower_rev,2, cb_trans, 3);
          ret = mb.readHreg(remote,POWER_ACC_REG,(uint16_t *)&ACpowerAccumulated_rev,4, cb_trans, 3);
          ret = mb.readHreg(remote,TIME_REG,(uint16_t *)&timestamp_rev,2, cb_trans, 3);
      mb.task();  // Common local Modbus task
  
      ACpower = swap32(ACpower_rev);
      ACpowerAccumulated = swap64(ACpowerAccumulated_rev);
      timestamp = swap32(timestamp_rev);
  
      if (timestamp > 0) {
        
        int day = timestamp / 86400;
        // new day?
        if (day > currentDay) {
          startDaySupplyCounter = supplyCounter;
          startDayConsumeCounter = consumeCounter;
          startDayACPowerCounter = ACpowerAccumulated;
          currentDay = day;
       }
       Check_Time();
       Serial.print("b: ");
       Serial.print(consumeCounter-startDayConsumeCounter);
       Serial.print(" kWh ,s: ");
       Serial.print(supplyCounter-startDaySupplyCounter);
       Serial.print(" kWh, sun: ");
       Serial.print(ACpowerAccumulated-startDayACPowerCounter);
       Serial.println(" kWh");
      }
      
    } else {
     // Serial.println(ret);
    }
  }
  else {
    display.setTextSize(4);
    display.setTextColor(WHITE);
    display.setCursor(0,20);
    display.println("no data");
    display.display();
  }
  delay(500);
  mb.dropTransactions();
  mb.task();
  mb.disconnect(remote);
  
  // Read and provide the data every 5 Seconds is enought
  delay(5000);
}
