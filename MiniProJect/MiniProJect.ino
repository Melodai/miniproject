#define ButtonOn D5
#define ButtonOff D6
#define SensorinParkDown D4
#define SensorinParkUp D3
#define SensorinSeaDown D2
#define SensorinSeaUp D1
#define Wather_Pump D7
#define LINE_TOKEN  "pBxbGXP8K2Ga9Zn8aOIKeolnR1l4ZZnZamP8ZfXDSQY"   // บรรทัดที่ 13 ใส่ รหัส TOKEN ที่ได้มาจากข้าง

#include <TridentTD_LineNotify.h>
#include <TaskScheduler.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>  // เปลี่ยนเป็น WiFiClientSecure
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

// สร้าง Scheduler
Scheduler runner;

// Update these with values suitable for your network.

const char* ssid = "Nebula";
const char* password = "teacherRoom";
const char* mqtt_server = "6555a54274d6454ab49e8eb9b94c325c.s1.eu.hivemq.cloud";
// เพิ่มตัวแปรสำหรับ MQTT credentials
const char* mqtt_username = "esp8266ptk";  // user HivvMQ Cluster
const char* mqtt_password = "Aa12341234";  // Pass HivvMQ Cluster


int parkDownStatus, parkUpStatus;  // ตัวเก็บค่าที่อ่านได้ จากเซนเซอร์ในสวน
int seaDownStatus, seaUpStatus;
bool Button_Status = false;
bool flag_autopump_on = false;
bool pump_working = false;  // ใช้เก็บค่าปั๊มทำงาน เพราะถ้าทำงานอยุ่แล้ว สั่งมาจากที่อื่นก็จะไม่ต้องสั่งซ้ำ
bool flag_trigBtn_start = false;
bool flag_send_pub_to_led_status = false;
bool flag_set_auotmatic_Check = false;
int dayOn_Select[] = { 0, 0, 0, 0, 0, 0, 0 };             // array เก็บวันตั้งค่า
long timeStart_Stop[] = { 0, 0 };                         // เก็บเวลา start stop
bool flag_timer_pump = false;                             // เก็บสถานะของ pump timer
bool flag_keep_timer_pump_working = false;                // เก็บสถานะของ pump เมื่อทำงานแล้วใน timer จะได้ไม่ทำซ้ำ
bool flag_send_set_led_today_working_pump_timer = false;  // เก็บ  flag เช็คว่าส่งสั่งปิด led today working pump ไปหรือยัง
bool flag_debug_SerialPrint = false;                      // สถานะสั่งเปิด  Debug ไว้ดู
int water_level_park = 0;                                 // ไว้เช็คไม่ให้มันส่งค่าเกจทำงานซ้ำ
int water_level_pub = 0;                                  // ไว้เช็คไม่ให้มันส่งค่าเกจทำงานซ้ำ

unsigned long lastNTPUpdate = 0;
const unsigned long NTP_INTERVAL = 3600000;  // 1 ชั่วโมง (60*60*1000 ms)

// ประกาศตัวแปรเพิ่มเติม
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
const long utcOffsetInSeconds = 25200;  // UTC+7 (Thailand)
WiFiClientSecure espClient;             // เปลี่ยนเป็น WiFiClientSecure
PubSubClient client(espClient);
//unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
//int value = 0;

//wi fi setup
void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());  // ป้องกันปัญหาการเชื่อมต่อ MQTT ด้วย client ID เดียวกัน

  // เริ่มต้น NTP Client
  timeClient.begin();
  timeClient.setTimeOffset(utcOffsetInSeconds);
  timeClient.update();
  lastNTPUpdate = millis();  // เก็บเวลาที่อัพเดท
  // แสดงเวลาครั้งแรก
  Serial.print("Current time: ");
  Serial.println(timeClient.getFormattedTime());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
// ส่วนหลักเลย ที่ใช้ในการรับค่าจาก Broker
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print(" : ");
  Serial.print(message);
  Serial.println("] ");

  String topicStr = String(topic);

  if (topicStr == "ptk/esp8266/btn") {
    if (message == "Btn_ON") {
      Button_Status = true;
      Serial.printf("btn on \n");
      if (client.connected())
        client.publish("ptk/esp8266/deug", "LED is ON");
    } else if (message == "Btn_OFF") {
      Button_Status = false;
      Serial.printf("btn off \n");
      if (client.connected())
        client.publish("ptk/esp8266/deug", "LED is OFF");
    }
  } else if (topicStr == "ptk/esp8266/set-debug") {
    if (message == "D_ON") {
      flag_debug_SerialPrint = true;
      Serial.printf("D_ON on \n");
      if (client.connected())
        client.publish("ptk/esp8266/deug", "Debug is ON");
    } else if (message == "D_OFF") {
      flag_debug_SerialPrint = false;
      Serial.printf("D_OF on \n");
      if (client.connected())
        client.publish("ptk/esp8266/deug", "Debug is ON");
    }
  } else if (topicStr == "ptk/esp8266/set-auto") {
    if (message == "Auto_ON") {
      flag_set_auotmatic_Check = true;
      Serial.printf("Auto_ON \n");
      if (client.connected())
        client.publish("ptk/esp8266/deug", "Auto is ON");
    } else if (message == "Auto_OFF") {
      flag_set_auotmatic_Check = false;
      Serial.printf("Auto_OFF \n");
      if (client.connected())
        client.publish("ptk/esp8266/deug", "Auto is OFF");
    }
  }
  // แก้การ parse เวลา
  else if (topicStr == "ptk/esp8266/timerstart") {
    // แปลงเวลาจาก "HH:MM:SS" เป็นนาที
    int hour = message.substring(0, 2).toInt();
    int minute = message.substring(3, 5).toInt();
    timeStart_Stop[0] = hour * 60 + minute;
    Serial.printf("timestart %s \n", message.c_str());
  } else if (topicStr == "ptk/esp8266/timerstop") {
    int hour = message.substring(0, 2).toInt();
    int minute = message.substring(3, 5).toInt();
    timeStart_Stop[1] = hour * 60 + minute;
    Serial.printf("timestop %s \n", message.c_str());
  }
  // เพิ่มวันที่เหลือ
  else if (topicStr == "ptk/esp8266/timer/mon") {
    if (message == "Mon_ON") {
      dayOn_Select[0] = 1;
      Serial.printf("Mon_ON \n");
    } else if (message == "Mon_OFF") {
      dayOn_Select[0] = 0;
      Serial.printf("Mon_OFF \n");
    }
  } else if (topicStr == "ptk/esp8266/timer/tues") {
    if (message == "Tues_ON") {
      dayOn_Select[1] = 1;
      Serial.printf("Tues_ON \n");
    } else if (message == "Tues_OFF") {
      dayOn_Select[1] = 0;
      Serial.printf("Tues_OFF \n");
    }
  } else if (topicStr == "ptk/esp8266/timer/wed") {
    if (message == "Wed_ON") {
      dayOn_Select[2] = 1;
      Serial.printf("Wed_ON \n");
    } else if (message == "Wed_OFF") {
      dayOn_Select[2] = 0;
      Serial.printf("Wed_OFF \n");
    }
  } else if (topicStr == "ptk/esp8266/timer/thurs") {
    if (message == "Thurs_ON") {
      dayOn_Select[3] = 1;
      Serial.printf("Thurs_ON \n");
    } else if (message == "Thurs_OFF") {
      dayOn_Select[3] = 0;
      Serial.printf("Thurs_OFF \n");
    }
  } else if (topicStr == "ptk/esp8266/timer/fri") {
    if (message == "Fri_ON") {
      dayOn_Select[4] = 1;
      Serial.printf("Fri_ON \n");
    } else if (message == "Fri_OFF") {
      dayOn_Select[4] = 0;
      Serial.printf("Fri_OFF \n");
    }
  } else if (topicStr == "ptk/esp8266/timer/sat") {
    if (message == "Sat_ON") {
      dayOn_Select[5] = 1;
      Serial.printf("Sat_ON \n");
    } else if (message == "Sat_OFF") {
      dayOn_Select[5] = 0;
      Serial.printf("Sat_OFF \n");
    }
  } else if (topicStr == "ptk/esp8266/timer/sun") {
    if (message == "Sun_ON") {
      dayOn_Select[6] = 1;
      Serial.printf("Sun_ON \n");
    } else if (message == "Sun_OFF") {
      dayOn_Select[6] = 0;
      Serial.printf("Sun_OFF \n");
    }
  }
}
// ในฟังก์ชัน reconnect
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    // เพิ่ม username/password ในการ connect
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
      //client.publish("ptk/test", "hello world");
      // Add Sub Topic เข้าไปสำหรับการ listen
      client.subscribe("ptk/esp8266/timerstart");
      client.subscribe("ptk/esp8266/timerstop");
      client.subscribe("ptk/esp8266/timer/mon");
      client.subscribe("ptk/esp8266/timer/tues");
      client.subscribe("ptk/esp8266/timer/wed");
      client.subscribe("ptk/esp8266/timer/thurs");
      client.subscribe("ptk/esp8266/timer/fri");
      client.subscribe("ptk/esp8266/timer/sat");
      client.subscribe("ptk/esp8266/timer/sun");
      client.subscribe("ptk/esp8266/set-debug");
      client.subscribe("ptk/esp8266/set-auto");
      client.subscribe("ptk/esp8266/water-level-park");
      client.subscribe("ptk/esp8266/water-level-pub");
      client.subscribe("ptk/esp8266/btn");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
void Check_Auto_Pump();
void Check_Btn_Pump();
void SensorRead();  // อ่านและทำการส่งค่าไปที่เกจ ของ แอพ
void Check_Timer_Pump();

// สร้าง Tasks
Task sensorReadTask(200, TASK_FOREVER, &SensorRead);      //  สำหรับ  Check Sensor
Task checkPumpTask(500, TASK_FOREVER, &Check_Auto_Pump);  // สำหรับ Pump ทำงานหลักเลย
Task btnTask(500, TASK_FOREVER, &Check_Btn_Pump);         // สำหรับสั่งปุ่มกดล้วน ๆ
Task timerTask(500, TASK_FOREVER, &Check_Timer_Pump);     // สำหรับ auto timer


void setup() {                       // <---- SetUp
  pinMode(Wather_Pump, OUTPUT);      // จำลองว่าเป็น ปั๊มน้ำ
  pinMode(SensorinSeaUp, INPUT);     //เซ็นเซอร์ในคลอง 2
  pinMode(SensorinSeaDown, INPUT);   //เซ็นเซอร์ในคลอง 1
  pinMode(SensorinParkUp, INPUT);    //เซ็นเซอร์ในสวน 2
  pinMode(SensorinParkDown, INPUT);  //เซ็นเซอร์ในสวน 1
  pinMode(ButtonOn, INPUT);          // เทสปุ่มกดเปิด
  pinMode(ButtonOff, INPUT);         // เทสปุ่มกดปิด
  Serial.begin(115200);
  setup_wifi();
  // เพิ่มการตั้งค่า SSL
  espClient.setInsecure();  // ไม่ตรวจสอบ certificate
  client.setServer(mqtt_server, 8883);
  client.setCallback(callback);

  // เพิ่ม tasks เข้าไปใน scheduler
  runner.addTask(checkPumpTask);
  runner.addTask(btnTask);
  runner.addTask(sensorReadTask);
  runner.addTask(timerTask);

  // เริ่มต้นการทำงานของ tasks
  checkPumpTask.enable();
  btnTask.enable();
  sensorReadTask.enable();
  timerTask.enable();
  LINE.setToken(LINE_TOKEN);

  Serial.println("ระบบเริ่มต้นทำงานแล้ว");
  LINE.notify("ระบบเริ่มต้นทำงานแล้ว");
}

// เช็คว่ามีน้ำในคลองพร้อมดูดออโต้หรือเปล่า
bool CheckWaterFull_Pub() {
  if (seaUpStatus == 1 && seaDownStatus == 0) {
    return true;
  }

  return false;
}

// เช็คว่าน้ำเต็มสวน หรือ น้ำหมดคลอง
bool CheckWaterStopPump() {
  if (seaDownStatus == 1 || parkUpStatus == 1) {
    return true;
  }
  return false;
}
void updateNTPTime() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastNTPUpdate >= NTP_INTERVAL) {
    timeClient.update();
    lastNTPUpdate = currentMillis;
  }
}

void open_pump() {
  if (!pump_working && client.connected()) {
    digitalWrite(Wather_Pump, HIGH);
    client.publish("ptk/esp8266/status", "Led_ON", true);
    client.publish("ptk/esp8266/btn", "Btn_ON", true);
    LINE.notify("ปั๊มทำงาน");
    Serial.println("ปั๊มทำงาน");
  }
  // set flag false เพื่อให้ สามารถส่งข้อความได้ หากปั๊มหยุด
  Serial.printf("พยามสั่งปั๊ม \n");
  flag_send_pub_to_led_status = false;
  pump_working = true;
}
void off_pump() {
  digitalWrite(Wather_Pump, LOW);
  pump_working = false;
  if (!flag_send_pub_to_led_status && client.connected()) {
    client.publish("ptk/esp8266/status", "Led_OFF", true);
    client.publish("ptk/esp8266/btn", "Btn_OFF", true);
    flag_send_pub_to_led_status = true;  // set เป็น true ไม่ให้ส่งซ้ำ เปลือง data
    LINE.notify("ปั๊มหยุดทำงาน");
    Serial.println("ปั๊มไม่ทำงาน");
  }
}
bool Check_Pump_Working() {  // ฟังก์ชันหลักในการเช็คการทำงานของปั๊มเลย จะถูกเรียกใน Task AutoPump เท่านั้น

  // ถ้าเซนเซอร์บน ในสวนน้ำเต็ม หรือ น้ำในคลองหมด
  if (CheckWaterStopPump()) {
    flag_autopump_on = false;  // set flag false ให้ ปั๊มหยุดทำงาน
    LINE.notify("ปั๊มหยุดทำงาน");
    Serial.printf("น้ำหมดละจ้า \n");
    return false;
  }
  // ถ้าถึงเวลาในการปั๊มน้ำ เงื่อนไขต่าง ๆ อยู่ในฟังก์ชัน
  else if (flag_timer_pump) {
    Serial.printf("ทำงานตามเวลาอยู่ครับพี่ \n");
    return true;
  }
  // ถ้าปุ่มถูกกด
  else if (flag_trigBtn_start) {
    Serial.printf("ทำงานตามที่กดปุ่มมาอยู่ครับนาย \n");

    return true;
    // ถ้าเซนเซอร์ตัวล่างในสวน ไม่มีน้ำ และ เซนเซอร์ตัวบนในสวนไม่มีน้ำ และ มีน้ำในแหล่งน้ำคลอง
  } else if ((parkDownStatus == 1 && parkUpStatus == 0 && CheckWaterFull_Pub())) {
    Serial.printf("ทำงานอัตโนมัติตามเงื่อนไขอยู่เลย \n");
    flag_autopump_on = true;  // เก็บ flag เพื่อให้ปั๊มทำงานต่อเนื่องจนน้ำเต็ม
    return true;              // return true  ให้ปั๊มทำงาน
  }                           // ถ้ายังมี flag true ให้ปั๊มทำงานต่อไป
  else if (flag_autopump_on) {
    Serial.printf(" flag ทำงานอัตโนมัติ \n");
    return true;
  }

  return false;
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  runner.execute();
}

int getCurrentTimeMinutes() {
  updateNTPTime();  // เช็คว่าถึงเวลาอัพเดทหรือยัง
  return timeClient.getHours() * 60 + timeClient.getMinutes();
}

int getCurrentDay() {
  updateNTPTime();
  int currentDay = timeClient.getDay();           // 0 = Sunday
  return (currentDay == 0) ? 6 : currentDay - 1;  // แปลงให้ จันทร์=0, อาทิตย์=6
}


const char* dayTH[] = { "จันทร์", "อังคาร", "พุธ", "พฤหัส", "ศุกร์", "เสาร์", "อาทิตย์" };
// ฟังก์ชัน debug แยกออกมา
void debugTimerInfo(int currentDay, int currentTimeMinutes, int startTimeMinutes, int stopTimeMinutes) {
  Serial.println("=== Timer Debug Info ===");
  Serial.printf("Current Day: %d (%s)\n", currentDay, dayTH[currentDay]);
  Serial.printf("Day Status: %s\n", dayOn_Select[currentDay] ? "ON" : "OFF");
  Serial.printf("Current Time: %02d:%02d\n", currentTimeMinutes / 60, currentTimeMinutes % 60);
  Serial.printf("Start Time: %02d:%02d\n", startTimeMinutes / 60, startTimeMinutes % 60);
  Serial.printf("Stop Time: %02d:%02d\n", stopTimeMinutes / 60, stopTimeMinutes % 60);
  Serial.printf("Timer Flag: %s\n", flag_timer_pump ? "Active" : "Inactive");
  Serial.println("=====================");
}

void Check_Timer_Pump() {
  if (client.connected()) {


    // ใช้ฟังก์ชันใหม่
    int currentTimeMinutes = getCurrentTimeMinutes();
    int currentDay = getCurrentDay();

    // แปลงเวลา start/stop เป็นนาที
    int startTimeMinutes = timeStart_Stop[0];
    int stopTimeMinutes = timeStart_Stop[1];

    // เช็คว่าวันนี้เปิดใช้งานไหม
    if (dayOn_Select[currentDay] == 1) {

      // เช็คว่าอยู่ในช่วงเวลาที่กำหนดไหม และในคลองมีน้ำไหม และในสวนน้ำเต็มหรือยัง
      if (currentTimeMinutes >= startTimeMinutes && currentTimeMinutes < stopTimeMinutes
          && seaDownStatus == 0 && parkUpStatus == 0 && !flag_keep_timer_pump_working) {
        flag_timer_pump = true;
        Serial.printf("On time \n");
        if (!flag_send_set_led_today_working_pump_timer) {
          client.publish("ptk/esp8266/timer/today-working", "Today_ON", true);
          flag_send_set_led_today_working_pump_timer = true;  // set เป็น true จะได้ไม่ส่งซ้ำ เปลือง data และ reset ตอนทำงานในครั้งถัดไป
        }
        // ถ้าเป็นทำงานอยู่และ น้ำเต็มสวน หรือน้ำหมดคลอง ให้หยุดซะ
      } else if (flag_timer_pump && CheckWaterStopPump()) {
        Serial.printf("Off time \n");
        flag_keep_timer_pump_working = true;  // หมายความว่าได้ทำงานกับการตั้งเวลาไปแล้ว และจำไม่กลับมาทำอีกในวันนั้น
        flag_timer_pump = false;
      }
    } else {
      if (flag_send_set_led_today_working_pump_timer) {
        flag_send_set_led_today_working_pump_timer = false;  //  reset flag ให้สามารถเขา้ else ได้ในตอนทำงานแหละ
        client.publish("ptk/esp8266/timer/today-working", "Today_OFF", true);
      }
      flag_timer_pump = false;               // flag นี้ถูกนำไปใช้ที่ ฟังก์ชันเช็คการทำงานของปั๊มหลัก ๆ
      flag_keep_timer_pump_working = false;  // reset flag เพื่อให้วันอื่น สามารถ set time สั่งปั๊มได้ใหม่
    }

    if (flag_debug_SerialPrint)
      debugTimerInfo(currentDay, currentTimeMinutes, startTimeMinutes, stopTimeMinutes);
  }
}

void Check_Auto_Pump() {  // Task เช็ค autoPump Task หลักในการสั่งเปิดปั๊มครับ
  if (client.connected()) {
    if (flag_set_auotmatic_Check) {  // ถ้าไม่เปิดตรงนี้จะไม่มีการทำใด ๆ ทั้งสั้น ตั้งค่าจาก แอพ เท่านั้น

      if (Check_Pump_Working()) {
        open_pump();  //เปิดปั้ม
        //Serial.printf("true \n");

      } else {
        off_pump();  //ปิดปั้ม
        //Serial.printf("false \n");
      }
    } else {
      if (!flag_send_pub_to_led_status && client.connected()) {
        client.publish("ptk/esp8266/status", "Led_OFF", true);
        client.publish("ptk/esp8266/btn", "Btn_OFF", true);
        flag_send_pub_to_led_status = true;  // set เป็น true ไม่ให้ส่งซ้ำ เปลือง data
        Serial.println("ปั๊มไม่ทำงาน");
        pump_working = false;
      }
    }
  }
}

void SensorRead() {  // Task อ่าน sensor

  if (client.connected()) {
    parkDownStatus = digitalRead(SensorinParkDown);
    parkUpStatus = digitalRead(SensorinParkUp);
    seaDownStatus = digitalRead(SensorinSeaDown);
    seaUpStatus = digitalRead(SensorinSeaUp);
    /*
  Serial.printf("SeaUp = %d SeaDown = %d ParkUp = %d ParkDown = %d \n", 
                seaUpStatus, 
                seaDownStatus, 
                parkUpStatus, 
                parkDownStatus );
                */

    if (client.connected()) {
      if (seaUpStatus == 1) {  // น้ำเต็มคลอง
        if (water_level_pub != 3)
          client.publish("ptk/esp8266/water-level-pub", "2", true);

        if (flag_debug_SerialPrint)
          Serial.printf("น้ำเต็มคลอง \n");
        water_level_pub = 3;
      } else if (seaDownStatus == 0) {  // น้ำครึ่งคลอง
        if (water_level_pub != 2)
          client.publish("ptk/esp8266/water-level-pub", "1", true);

        if (flag_debug_SerialPrint)
          Serial.printf("น้ำครึ่งคลอง \n");
        water_level_pub = 2;
      } else if (seaDownStatus == 1) {  // น้ำหมดคลองแล้ว
        if (water_level_pub != 1)
          client.publish("ptk/esp8266/water-level-pub", "0", true);

        if (flag_debug_SerialPrint)
          Serial.printf("น้ำหมดคลองแล้ว \n");
        water_level_pub = 1;
      }

      if (parkUpStatus == 1) {  // น้ำเต็มสวน
        if (water_level_park != 3)
          client.publish("ptk/esp8266/water-level-park", "2", true);

        if (flag_debug_SerialPrint)
          Serial.printf("น้ำเต็มสวน \n");
        water_level_park = 3;
      } else if (parkDownStatus == 0) {  // น้ำครึ่งสวน
        if (water_level_park != 2)
          client.publish("ptk/esp8266/water-level-park", "1", true);

        if (flag_debug_SerialPrint)
          Serial.printf("น้ำครึ่งสวน \n");
        water_level_park = 2;
      } else if (parkDownStatus == 1) {  // น้ำหมดสวนแล้ว ฮัลโหลล
        if (water_level_park != 1)
          client.publish("ptk/esp8266/water-level-park", "0", true);

        if (flag_debug_SerialPrint)
          Serial.printf("น้ำหมดสวนแล้ว ฮัลโหลล \n");
        water_level_park = 1;
      }
    }
  }
}

void Check_Btn_Pump() {  //Task Check Btn
  if (client.connected()) {
    // เช็คว่ามีการกดปุ่ม และ ยังมีน้ำในคลอง และ น้ำยังไม่เต็มสวน
    //Serial.printf("CheckWaterStopPump() = %d \n" , CheckWaterStopPump());
    if (Button_Status && !CheckWaterStopPump()) {
      printf("Retrun true Her \n");
      flag_trigBtn_start = true;
      // ถ้าปุ่มเปิดแล้ว แต่น้ำเต็มสวน หรือน้ำหมดคลอง
    } else if (flag_trigBtn_start && CheckWaterStopPump()) {
      printf("Retrun flase Her \n");
      flag_trigBtn_start = false;
      // ถ้าไม่มีการกดปุ่ม
    } else if (!Button_Status) {
      flag_trigBtn_start = false;
      // ถ้ากดปุ่มแล้วถูกหยุด จะ set ปุ่มให้เด้งกลับไปปิด
    } else if (Button_Status && !flag_trigBtn_start) {
      Button_Status = false;
      Serial.printf("btn off เพราะ ไม่มีน้ำ \n");
      if (client.connected())
        client.publish("ptk/esp8266/deug", "LED is OFF");
      client.publish("ptk/esp8266/btn", "Btn_OFF", true);
    }
  }
}
