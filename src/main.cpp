#include <Arduino.h>
#include <GyverMotor2.h>
#include <GParser.h>
#include <Servo.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <EEPROM.h>

AsyncWebServer server(80);

//#define MKD_Guest
#ifdef MKD_Guest
const char *ssid = "MKD-Guest";
const char *password = "123Qweasd";
#else
const char *ssid = "Keenetic-1649";
const char *password = "jsCMnJpr";
#endif
const uint32_t port = 1234;
const char *hostname = "MINI";
WiFiUDP udp;

IPAddress ServIP(192, 0, 0, 0);

//Лебедка
GMotor2<DRIVER2WIRE> MOT_CR1_Winch(23, 22);
GMotor2<DRIVER2WIRE> MOT_CR2_Winch(32, 33);
//Стрела
GMotor2<DRIVER2WIRE> MOT_CR1_Arm(21, 19);
GMotor2<DRIVER2WIRE> MOT_CR2_Arm(25, 26);
//Поворот
GMotor2<DRIVER2WIRE> MOT_CR1_Rot(17, 16);
GMotor2<DRIVER2WIRE> MOT_CR2_Rot(27, 13);

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.print("Started: ");
  Serial.println(WiFi.localIP());
  udp.begin(port);


  MOT_CR1_Winch.setMinDuty(20); // мин. ШИМ  ////////////////////////////////////////////////
  //MOT_CR1_Winch.reverse(1);      // реверс  ////////////////////////////////////////////////
  MOT_CR1_Winch.setDeadtime(1);  // deadtime  ////////////////////////////////////////////////

  MOT_CR2_Winch.setMinDuty(20); // мин. ШИМ  ////////////////////////////////////////////////
  //MOT_CR2_Winch.reverse(1);      // реверс  ////////////////////////////////////////////////
  MOT_CR2_Winch.setDeadtime(1);  // deadtime  ////////////////////////////////////////////////

  MOT_CR1_Arm.setMinDuty(20); // мин. ШИМ  ////////////////////////////////////////////////
  //MOT_CR1_Arm.reverse(1);      // реверс  ////////////////////////////////////////////////
  MOT_CR1_Arm.setDeadtime(1);  // deadtime  ////////////////////////////////////////////////

  MOT_CR2_Arm.setMinDuty(20); // мин. ШИМ  ////////////////////////////////////////////////
  //MOT_CR2_Arm.reverse(1);      // реверс  ////////////////////////////////////////////////
  MOT_CR2_Arm.setDeadtime(1);  // deadtime  ////////////////////////////////////////////////

  MOT_CR1_Rot.setMinDuty(20); // мин. ШИМ  ////////////////////////////////////////////////
  //MOT_CR1_Rot.reverse(1);      // реверс  ////////////////////////////////////////////////
  MOT_CR1_Rot.setDeadtime(1);  // deadtime  ////////////////////////////////////////////////

  MOT_CR2_Rot.setMinDuty(20); // мин. ШИМ  ////////////////////////////////////////////////
  //MOT_CR2_Rot.reverse(1);      // реверс  ////////////////////////////////////////////////
  MOT_CR2_Rot.setDeadtime(1);  // deadtime  ////////////////////////////////////////////////

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", hostname); });

  AsyncElegantOTA.begin(&server); // Start AsyncElegantOTA
  server.begin();
}

void loop()
{
  MOT_CR1_Rot.tick();
  MOT_CR1_Winch.tick();
  MOT_CR1_Arm.tick();

  MOT_CR2_Rot.tick();  
  MOT_CR2_Winch.tick();  
  MOT_CR2_Arm.tick();

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  static uint32_t tmr0 = millis();
  if (millis() - tmr0 > 25)
  {
    tmr0 = millis();
    int psize = udp.parsePacket();
    if (psize > 0)
    {
      char buf[32];
      int len = udp.read(buf, 32);
      IPAddress newIP;

      buf[len] = '\0'; // Парсим
      GParser data(buf);
      int ints[data.amount()];
      data.parseInts(ints);

      if (newIP.fromString(buf))
      {
        if (strcmp(newIP.toString().c_str(), ServIP.toString().c_str()))
        {
          Serial.print("Server IP changed: ");
          Serial.print(ServIP);
          Serial.print(" -- >> ");
          ServIP = newIP;
        }
        Serial.println(ServIP);
      }
      else if (buf[0] == '2')
      {
        int rot_val = abs(ints[4] - 127) <= 2 ? 0 : map(ints[4], 0, 255, -255, 255);
        int win_val = abs(ints[5] - 127) <= 2 ? 0 : map(ints[5], 0, 255, -255, 255);
        int arm_val = abs(ints[6] - 127) <= 2 ? 0 : map(ints[6], 0, 255, -255, 255);
        switch (ints[2])
        {
        case 0: //single mode
        if(ints[3] == 0)// crane 1
        {
            MOT_CR1_Rot.reverse(0);

            MOT_CR1_Rot.setSpeed(rot_val);
            MOT_CR1_Winch.setSpeed(win_val);
            MOT_CR1_Arm.setSpeed(arm_val);
        }
        else if(ints[3] == 1) // crane 2
        {
            MOT_CR2_Rot.setSpeed(rot_val);
            MOT_CR2_Winch.setSpeed(win_val);
            MOT_CR2_Arm.setSpeed(arm_val);
        }
        else
        {
            MOT_CR2_Rot.setSpeed(0);
            MOT_CR2_Winch.setSpeed(0);
            MOT_CR2_Arm.setSpeed(0);
        }
            break;
        
        default: // mirror mode
            MOT_CR1_Rot.reverse(1);

            MOT_CR1_Rot.setSpeed(rot_val);
            MOT_CR1_Winch.setSpeed(win_val);
            MOT_CR1_Arm.setSpeed(arm_val);

            MOT_CR2_Rot.setSpeed(rot_val);
            MOT_CR2_Winch.setSpeed(win_val);
            MOT_CR2_Arm.setSpeed(arm_val);
            break;
        }
      }
    }
  }

  static uint32_t tmr = millis();
  if (millis() - tmr > 25)
  {
    tmr = millis();
    char buf[32] = "_";
    strncat(buf, hostname, strlen(hostname));
    udp.beginPacket(ServIP, port);
    udp.printf(buf);
    udp.endPacket();
  }
}