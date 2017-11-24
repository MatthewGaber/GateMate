#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <Servo.h>
#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include "memorysaver.h"
#include "FS.h"
#include "AES.h"
#include "Base64.h"
#include "Crypto.h"
#include <EEPROM.h>


//ardunano 192.168.1.20/stream Port 83
//espino  192.168.1.16 Port 84

//Set your WiFi details
const char* ssid="####.";             // yourSSID
const char* password="####";     // yourPASSWORD

int sliderVal=100;     
int sliderVal2=90; // Default value of the slider

ESP8266WebServer server(84);

Servo horservo;
Servo vertservo;
const int GATE_PIN = 4;

AES aes;

#define KEY_LENGTH 16
// The aeskey is the first 16 characters of the shared secret entered in the iphone app converted to hex
// The hashkey is the last 16 characters of the shared secret entered in the iphone app converted to hex
byte aeskey[] = { 0x6d, 0x69, 0x63, 0x6b, 0x65, 0x79, 0x6d, 0x69, 0x6e, 0x6e, 0x69, 0x65, 0x70, 0x6f, 0x70, 0x65};
byte hashkey[] = { 0x79, 0x65, 0x62, 0x72, 0x75, 0x74, 0x75, 0x73, 0x67, 0x6f, 0x6f, 0x66, 0x79, 0x70, 0x6c, 0x75 };

byte out[10];

void handleHorSlider() {  
  sliderVal = server.arg("hor").toInt();
  horservo.write(sliderVal);
}

void handleVerSlider() {    
  sliderVal2 = server.arg("ver").toInt();
  vertservo.write(sliderVal2);
}

void handleSetButton() { 
  String cipherSt = server.arg("couscous");
  cipherSt.replace(" ","+");
  String ivSt = server.arg("tabouli");
  ivSt.replace(" ","+");
  String hashSt = server.arg("tahini");
  hashSt.replace(" ","+");
  String stickSt = server.arg("chilli");
 
  hashSt.toUpperCase();

  //Hash stickSt
  SHA256HMAC hmac(hashkey, KEY_LENGTH);
  const char *stickCst = stickSt.c_str();
  hmac.doUpdate(stickCst, strlen(stickCst));
  byte authCode[SHA256HMAC_SIZE];
  hmac.doFinal(authCode);

  //Convert authCode to char dest
  byte sCode = sizeof(authCode);
  char dest[200];
  memset(dest, 0, sizeof(dest));
  // test the size of the character array
  if((sizeof(dest) - 1) / 2 < sCode)
  {
    Serial.println("Character buffer too small");
    for(;;);
  }

  for (int cnt = 0; cnt < sCode; cnt++)
  {
    // convert byte to its ascii representation
    sprintf(&dest[cnt * 2], "%02X", authCode[cnt]);
  }
  
  //If the hash of stickSt is equal to hashSt(the hash from the client)
  //go ahead and read the previous hash from eeprom 
  const char *hashchar = hashSt.c_str();
  int charLength=hashSt.length();
  String prevhash;
  const char *prevhashCst;
  if (strcmp(dest, hashchar) == 0){
    Serial.println("Reading EEPROM previous hash");
    
    for (int i = 0; i < charLength; ++i)
      {
        prevhash += char(EEPROM.read(i));
      }
          
    prevhashCst = prevhash.c_str();
     //If the current hash is not the same as the previous hash go ahead and save the current hash
    //to eeprom and decrypt the cipherSt
      if (strcmp(prevhashCst, hashchar) != 0 && strlen(prevhashCst) == strlen(hashchar) ){
        Serial.println("writing eeprom with current hash:");
        for (int i = 0; i < hashSt.length(); ++i)
          {
            EEPROM.write(i, hashSt[i]);
            //Serial.print("Wrote: ");
            //Serial.println(hashSt[i]); 
          }

        char data_decoded[200];
        char iv_decoded[200];
        char temp[200];
        cipherSt.toCharArray(temp, 200);
        base64_decode(data_decoded, temp, cipherSt.length());
        ivSt.toCharArray(temp, 200);
        base64_decode(iv_decoded, temp, ivSt.length());    
        aes.do_aes_decrypt((byte *)data_decoded, 16, out, aeskey, 128, (byte *)iv_decoded);

        byte comp[11] = { 0x66, 0x44, 0x52, 0x6b, 0x72, 0x67, 0x4b, 0x4a, 0x5a, 0x54 };

        //If the decrypted cipherSt is equal to the byte comp
        //go ahead and open the gate
        if (memcmp(out, comp, sizeof(out)) == 0) {
          digitalWrite(GATE_PIN, HIGH);
          delay(500);
          digitalWrite(GATE_PIN, LOW);    
        }
    aes.clean();
    }
  }
}

void setup() {
  horservo.attach(5);
  vertservo.attach(16);
  pinMode(GATE_PIN, OUTPUT);
  EEPROM.begin(512); 
  
  Serial.begin(115200); 
  SPIFFS.begin();
  
  WiFi.begin(ssid,password);
  while(WiFi.status()!=WL_CONNECTED)delay(500);
  WiFi.mode(WIFI_STA);
  Serial.println("\n\nBOOTING ESP8266 ...");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("Station IP address = ");
  Serial.println(WiFi.localIP());

  // Start the server
  server.on("/setHorSlider", HTTP_POST, handleHorSlider);
  server.on("/setVerSlider", HTTP_POST, handleVerSlider);
  server.on("/setButton", HTTP_POST, handleSetButton);
  server.begin();
  
  String initHash = "A41FB9E48F0A0121405B5F6B86A95AAB192B66CC0126F248555929CA06F4AB0D";
  for (int i = 0; i < initHash.length(); ++i)
      {
        EEPROM.write(i, initHash[i]);
        Serial.print("Wrote: ");
        Serial.println(initHash[i]); 
      }
}

void loop() {
  server.handleClient();
  
}
