#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266WiFi.h>
#include <ThingSpeak.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200);  // GMT+7 for Jakarta, Indonesia

#define WIFI_SSID "POHARIN D177B_plus"
#define WIFI_PASSWORD "AsampaiZ"
#define API_KEY "AIzaSyDx7mrbc4leZbB9bIN2eW9kGCzw2ZAu0ac"
#define DATABASE_URL "https://komunikasi-data-1f4ca-default-rtdb.asia-southeast1.firebasedatabase.app"

//THINGSPEAK
const char* server = "api.thingspeak.com";
const long unsigned int CHANNEL_ID = 2474972;
const char* apiKey = "TGV422YJ3IVZY879";

//FIREBASE
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;
unsigned long sendDataPrevMillis = 0;
unsigned long count = 0;
String msg;
String dt[7];
String receiver;

int ppm6, ppm4, ppm138, ppm135, k;
//void sendThing(float ppm135, int k, String receivedTime, int rssi);

WiFiClient client;

// deklarasi variabel global
String receivedTime;

// deklarasi fungsi
int scanSpecificWiFiNetwork();
long rssi = WiFi.RSSI();

void setup() {
  Serial.begin(115200);
  delay(10);

  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Inisialisasi Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Menghubungkan ke Firebase
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase connection successful");
    signupOK = true;
  } else {
    Serial.printf("Firebase connection failed: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Inisialisasi NTP client
  timeClient.begin();
  timeClient.setTimeOffset(25200);  // GMT+7 for Jakarta, Indonesia
}

void loop() {
  // Mengambil nilai saat ini
  timeClient.update();

  // Mengambil nilai RSSI
  rssi = WiFi.RSSI();

  // Update nilai di firebase dan timestamp
  if (Firebase.ready() && signupOK) {
    Firebase.RTDB.setInt(&fbdo, "WiFi/RSSI", rssi);  // Simpan nilai RSSI ke Firebase
  }

  // Baca data yang dikirim dari ATmega2560
  while (Serial.available() > 0) {
    msg = Serial.readString();

    int j = 0;
    dt[j] = "";

    for (int i = 1; i < msg.length(); i++) {
      if ((msg[i] == '#') || (msg[i] == ',')) {
        j++;
        dt[j] = "";
      } else {
        dt[j] += msg[i];
      }
    }

    // Convert received strings to float with specific decimal precision
    float ppm6_float = dt[0].toFloat();
    float ppm4_float = dt[1].toFloat();
    float ppm138_float = dt[2].toFloat();
    float ppm135_float = dt[3].toFloat();
    k = dt[4].toInt();  // sampling ke-

    sendThing(ppm135_float, k, receiver, rssi);

    // // Convert float to String with specific decimal precision
    // String ppm4_str = String(ppm4_float, 5); // 3 decimal places

    // Dapatkan waktu saat data diterima
    String receivedTime = timeClient.getFormattedTime();

    // Update nilai di Firebase dan timestamp
    if (Firebase.ready() && signupOK) {
      Firebase.RTDB.setFloat(&fbdo, "MQ6 Sensor/Value", ppm6_float);
      Firebase.RTDB.setFloat(&fbdo, "MQ4 Sensor/Value", ppm4_float);
      Firebase.RTDB.setFloat(&fbdo, "MQ138 Sensor/Value", ppm138_float);
      Firebase.RTDB.setFloat(&fbdo, "MQ135 Sensor/Value", ppm135_float);
      Firebase.RTDB.setString(&fbdo, "Sampling ke-", k);
      Firebase.RTDB.setString(&fbdo, "Receiver/Time", receivedTime);
    }
  }
}

    void sendThing(float ppm135_float, int k, String receivedTime, int rssi) {
      if (client.connect(server, 80)) {
        String postStr = apiKey;
        postStr += "&field1=" + String(ppm135_float) + "&field2=" + String(rssi) + "&field3=" + String(k) + "\r\n\r\n";

        client.print("POST /update HTTP/1.1\n");
        client.print("Host: api.thingspeak.com\n");
        client.print("Connection: close\n");
        client.print(String("X-THINGSPEAKAPIKEY: ") + apiKey + "\n");
        client.print("Content-Type: application/x-www-form-urlencoded\n");
        client.print("Content-Length: ");
        client.print(postStr.length());
        client.print("\n\n");
        client.print(postStr);

        Serial.print("MQ135: " + String(ppm135_float) + " | RSSI: " + String(rssi) + " | Sampling: " + String(k));
        Serial.println("%. Sent to ThingSpeak.");
      }
      
      delay(500);
      client.stop();
      Serial.println("Waiting...");
      
      // ThingSpeak perlu delay 15 detik untuk setiap update
      delay(1000);
    }
