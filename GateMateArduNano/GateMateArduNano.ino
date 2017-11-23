#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <Servo.h>
#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include "memorysaver.h"

//ardunano 192.168.1.20/stream Port 83
//espino  192.168.1.16 Port 84

//Set your WiFi details
const char* ssid="#####";             
const char* password="#####";     

ESP8266WebServer server(83);

const int CS = 16;

static const size_t bufferSize = 4096; //4096;
static uint8_t buffer[bufferSize] = {0xFF};
uint8_t temp = 0, temp_last = 0;
int i = 0;
bool is_header = false;

#if defined (OV2640_MINI_2MP) || defined (OV2640_CAM)
  ArduCAM myCAM(OV2640, CS);
#elif defined (OV5640_MINI_5MP_PLUS) || defined (OV5640_CAM)
  ArduCAM myCAM(OV5640, CS);
#elif defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) ||(defined (OV5642_CAM))
  ArduCAM myCAM(OV5642, CS);
#endif

void start_capture(){
  myCAM.clear_fifo_flag();
  myCAM.start_capture();
}

void serverStream(){
  WiFiClient client = server.client();
  if (!client) {
    client.flush();
    return;
  }
  client.setNoDelay(1);
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);
  
  while (1){   
    start_capture();
    while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));
    size_t len = myCAM.read_fifo_length();
    if (len >= MAX_FIFO_SIZE) //8M
    {
    Serial.println(F("Over size."));
    continue;
    }
    if (len == 0 ) //0 kb
    {
    Serial.println(F("Size is 0."));
    continue;
    } 
    myCAM.CS_LOW();
    myCAM.set_fifo_burst();
    if (!client.connected()) break;
    response = "--frame\r\n";
    response += "Content-Type: image/jpeg\r\n\r\n";
    server.sendContent(response); 
    while ( len-- )
    {
    temp_last = temp;
    temp =  SPI.transfer(0x00);
    
    //Read JPEG data from FIFO
    if ( (temp == 0xD9) && (temp_last == 0xFF) ) //If find the end ,break while,
    {
    buffer[i++] = temp;  //save the last  0XD9     
    //Write the remain bytes in the buffer
    myCAM.CS_HIGH();; 
    if (!client.connected()) break;
    client.write(&buffer[0], i);
    is_header = false;
    i = 0;
    }  
    if (is_header == true)
    { 
    //Write image data to buffer if not full
    if (i < bufferSize)
    buffer[i++] = temp;
    else
    {
    //Write bufferSize bytes image data to file
    myCAM.CS_HIGH(); 
    if (!client.connected()) break;
    client.write(&buffer[0], bufferSize);
    i = 0;
    buffer[i++] = temp;
    myCAM.CS_LOW();
    myCAM.set_fifo_burst();
    }        
    }
    else if ((temp == 0xD8) & (temp_last == 0xFF))
    {
    is_header = true;
    buffer[i++] = temp_last;
    buffer[i++] = temp;   
    } 
    }
    if (!client.connected()) break;
   }
}

void setup() {
 
  uint8_t vid, pid;
  uint8_t temp;
  #if defined(__SAM3X8E__)
  Wire1.begin();
  #else
  Wire.begin();
  #endif
  Serial.begin(115200); 

  
  WiFi.begin(ssid,password);
  while(WiFi.status()!=WL_CONNECTED)delay(500);
  WiFi.mode(WIFI_STA);
  Serial.println("\n\nBOOTING ESP8266 ...");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("Station IP address = ");
  Serial.println(WiFi.localIP());

// set the CS as an output:
  pinMode(CS, OUTPUT);
  
  // initialize SPI:
  SPI.begin();
  SPI.setFrequency(4000000); //4MHz
  
  //Check if the ArduCAM SPI bus is OK
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM.read_reg(ARDUCHIP_TEST1);
  if (temp != 0x55){
  Serial.println(F("SPI1 interface Error!"));
  while(1);
  }
  #if defined (OV2640_MINI_2MP) || defined (OV2640_CAM)
  //Check if the camera module type is OV2640
  myCAM.wrSensorReg8_8(0xff, 0x01);
  myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
  if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 )))
  Serial.println(F("Can't find OV2640 module!"));
  else
  Serial.println(F("OV2640 detected."));
  #elif defined (OV5640_MINI_5MP_PLUS) || defined (OV5640_CAM)
  //Check if the camera module type is OV5640
  myCAM.wrSensorReg16_8(0xff, 0x01);
  myCAM.rdSensorReg16_8(OV5640_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5640_CHIPID_LOW, &pid);
  if((vid != 0x56) || (pid != 0x40))
  Serial.println(F("Can't find OV5640 module!"));
  else
  Serial.println(F("OV5640 detected."));
  #elif defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) ||(defined (OV5642_CAM))
  //Check if the camera module type is OV5642
  myCAM.wrSensorReg16_8(0xff, 0x01);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
  if((vid != 0x56) || (pid != 0x42)){
  Serial.println(F("Can't find OV5642 module!"));
  }
  else
  Serial.println(F("OV5642 detected."));
  #endif
  
  
  //Change to JPEG capture mode and initialize the OV2640 module
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  #if defined (OV2640_MINI_2MP) || defined (OV2640_CAM)
  myCAM.OV2640_set_JPEG_size(OV2640_320x240);
  #elif defined (OV5640_MINI_5MP_PLUS) || defined (OV5640_CAM)
  myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
  myCAM.OV5640_set_JPEG_size(OV5640_320x240);
  #elif defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) ||(defined (OV5642_CAM))
  myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
  myCAM.OV5640_set_JPEG_size(OV5642_320x240);  
  #endif
  
  myCAM.clear_fifo_flag();

  server.on("/stream", HTTP_GET, serverStream);
  
  server.begin();
 
}

void loop() {
  server.handleClient();
}
