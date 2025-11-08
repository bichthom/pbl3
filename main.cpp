#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> 

// WiFi http://www.emqx.io/online-mqtt-client#/recent_connections/849a6e87-f8eb-4e1e-939e-de1d602964c2
const char *ssid = "Huet"; // Enter your WiFi name
const char *password = "000000001";  // Enter WiFi password

// MQTT Broker
const char *mqtt_broker =  "broker.emqx.io";
const char *topic = "NEWS";
const char *mqtt_username = "";
const char *mqtt_password = "";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
void callback(char* topic, byte* payload, unsigned int length);


// --- Chân thiết bị ---
#define SENSOR1_PIN   15
#define SENSOR2_PIN    4
#define LED_PIN        22
#define FAN_PIN        23
#define LM35_PIN      34
#define BUTTON_LIGHT_PIN  18
#define BUTTON_FAN_PIN    19

int peopleCount = 0;
float temperature = 0.0;
bool light_status = false;
bool fan_status = false;
bool manualLight = false;
bool manualFan = false;

unsigned long lastSend = 0;
unsigned long lastTempUpdate = 0;

void setup() {
 // Set software serial baud to 115200;
 Serial.begin(115200);
 // connecting to a WiFi network
 WiFi.begin(ssid, password);
 while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.println("Connecting to WiFi..");
 }
 Serial.println("Connected to the WiFi network");
 //connecting to a mqtt broker
 client.setServer(mqtt_broker, mqtt_port);
 client.setCallback(callback);
 while (!client.connected()) {
     String client_id = "esp32-client-";
     client_id += String(WiFi.macAddress());
     Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
     if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
         Serial.println("Public emqx mqtt broker connected");
     } else {
         Serial.print("failed with state ");
         Serial.print(client.state());
         delay(2000);
     }
 }
 // publish and subscribe
 client.publish(topic, "NEWS"); client.subscribe(topic);
  

  pinMode(SENSOR1_PIN, INPUT);
  pinMode(SENSOR2_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(BUTTON_LIGHT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_FAN_PIN, INPUT_PULLUP);
}

void handleControl() {

  // Nếu manual và phòng hết người → trả về AUTO
 // --- Biến trợ giúp (khai báo trên cùng file, ngoài hàm) ---
static unsigned long emptySince = 0;         // thời điểm bắt đầu thấy phòng trống
const unsigned long emptyConfirmMs = 5000;  // phải trống liên tục 5s mới restore AUTO
static int lastCount = 0;

if (lastCount == 0 && peopleCount > 0) {
  manualLight = false;
  manualFan = false;
  Serial.println("[AUTO] Có người vào -> bật lại chế độ tự động");
}
lastCount = peopleCount;

// --- Thay thế logic trả lại AUTO (thay chỗ if (peopleCount == 0) { ... }) ---
if (peopleCount == 0) {
  // bắt đầu đếm thời gian khi lần đầu thấy phòng trống
  if (emptySince == 0) {
    emptySince = millis();
  } else {
    // nếu đã trống ổn định trong khoảng thời gian xác nhận -> restore AUTO
    if (millis() - emptySince >= emptyConfirmMs) {
      if (manualLight || manualFan) {
        manualLight = false;
        manualFan = false;
        Serial.println("[AUTO] Phòng trống ổn định -> khôi phục chế độ tự động");
      }
      // giữ emptySince = 0 để không lặp thông báo
      emptySince = 0;
    }
  }
} else {
  // khi phát hiện có người, reset bộ đếm
  emptySince = 0;
}


  // ĐÈN
  if (!manualLight) {  // auto mode
    light_status = (peopleCount > 0);
  }
  // nếu manualLight = true → giữ nguyên light_status
  
  // QUẠT
  if (!manualFan) { // auto mode
    fan_status = (peopleCount > 0 && temperature > 20);
  }

  digitalWrite(LED_PIN, light_status ? HIGH : LOW);
  digitalWrite(FAN_PIN, fan_status ? HIGH : LOW);
}



void loop() {
 
  client.loop();
// --- Đếm người ---
  if (digitalRead(SENSOR1_PIN)==LOW && digitalRead(SENSOR2_PIN)==HIGH) {
    while(digitalRead(SENSOR2_PIN)==LOW) delay(10);
    peopleCount++;
    Serial.printf(">> Vào | %d\n", peopleCount);
    delay(300);
  }
  if (digitalRead(SENSOR2_PIN)==LOW && digitalRead(SENSOR1_PIN)==HIGH) {
    while(digitalRead(SENSOR1_PIN)==LOW) delay(10);
    peopleCount = max(0, peopleCount - 1);
    Serial.printf("<< Ra | %d\n", peopleCount);
    delay(300);
  }

  // --- Nút nhấn đèn ---
  if (digitalRead(BUTTON_LIGHT_PIN)==LOW) {
    manualLight = true;
    light_status = !light_status;
    Serial.println(light_status ? "Đèn ON" : "Đèn OFF");
    delay(300);
  }

  // --- Nút nhấn quạt ---
  if (digitalRead(BUTTON_FAN_PIN)==LOW) {
    manualFan = true;
    fan_status = !fan_status;
    Serial.println(fan_status ? "Quạt ON" : "Quạt OFF");
    delay(300);
  }

  // --- Đọc LM35 mỗi 1 giây ---
  if (millis() - lastTempUpdate > 1000) {
    lastTempUpdate = millis();
    int adcValue = analogRead(LM35_PIN);
    temperature = (adcValue * (3.3 / 4095.0)) * 100.0;
  }
   // --- AUTO MODE ---
 handleControl();



  // --- Gửi dữ liệu lên MQTT mỗi 3 giây ---
  if (millis() - lastSend > 3000) {
    lastSend = millis();

    String payload = "{";
    payload += "\"temperature\":" + String(temperature, 1) + ",";
    payload += "\"songuoi\":" + String(peopleCount) + ",";
    payload += "\"light\":\"" + String(light_status ? "ON" : "OFF") + "\",";
    payload += "\"fan\":\"" + String(fan_status ? "ON" : "OFF") + "\"}";
    
    client.publish(topic, payload.c_str());
    Serial.println("MQTT TX: " + payload);
  }
  }


void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  Serial.println("MQTT RX: " + msg);

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, msg);
  if (err) {
    Serial.println("⚠ JSON parse fail!");
    return;
  }

  // ===== ĐÈN =====
  if (doc["light"].is<String>()) {
    manualLight = true;
    String denStr = doc["light"].as<String>();

    if (denStr == "ON")      light_status = true;
    else if (denStr == "OFF") light_status = false;

    digitalWrite(LED_PIN, light_status ? HIGH : LOW);
    Serial.println(String("[MQTT] Đèn -> ") + (light_status ? "ON" : "OFF"));
  }

  // ===== QUẠT =====
  if (doc["fan"].is<String>()) {
    manualFan = true;
    String fanStr = doc["fan"].as<String>();

    if (fanStr == "ON")      fan_status = true;
    else if (fanStr == "OFF") fan_status = false;

    digitalWrite(FAN_PIN, fan_status ? HIGH : LOW);
    Serial.println(String("[MQTT] Quạt -> ") + (fan_status ? "ON" : "OFF"));
  }
}


  




