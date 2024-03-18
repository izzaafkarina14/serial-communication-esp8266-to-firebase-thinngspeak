#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266WiFi.h>
#include <ThingSpeak.h>
#include <ThingerESP8266.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200); // GMT+7 for Jakarta, Indonesia

#define WIFI_SSID "bangg?"
#define WIFI_PASSWORD "hwvv1213"
#define API_KEY "AIzaSyBnPADOix0EWKu29eysInzDlbA5Rvv9Lz8"
#define DATABASE_URL "https://bismillah-ambil-data-beneran-default-rtdb.asia-southeast1.firebasedatabase.app"

// Set your Thinger.io credentials here
#define USERNAME "adiis04"
#define DEVICE_ID "RSSI"
#define DEVICE_CREDENTIAL "XwBs!HJCCZNOQ$An"

ThingerESP8266 thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);

const char* server = "api.thingspeak.com";
unsigned long myChannelNumber = 2474972;
String myWriteAPIKey = "81F3RHUX25S67I9B";

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;
unsigned long sendDataPrevMillis = 0;
unsigned long count = 0;
String msg;
String dt[7];
String receiver;

int MQ6, MQ4, MQ138, MQ135, k;

WiFiClientSecure client;

// deklarasi variabel global
String receivedTime;

// deklarasi fungsi
int scanSpecificWiFiNetwork();
bool shouldSendData();
void sendData(int MQ6, int MQ4, int MQ138, int MQ135, int k);
void waitForWiFi();
void sendToThinger(int MQ6, String receivedTime, int k, long rssi);
void sendToThingSpeak(int MQ135, String receivedTime, int k, long rssi);
long rssi = WiFi.RSSI();

void setup() {
  Serial.begin(115200);

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

  /* connection to firebase */
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase connection successful");
    signupOK = true;
  } else {
    Serial.printf("Firebase connection failed: %s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Initialize NTP client
  timeClient.begin();
  timeClient.setTimeOffset(25200); // GMT+7 for Jakarta, Indonesia
}

void loop() {
  // Get current time
  timeClient.update();

  // Get RSSI value
  rssi = WiFi.RSSI();

  // Update Firebase with value & timestamp
  if (Firebase.ready() && signupOK) {
    Firebase.RTDB.setInt(&fbdo, "WiFi/RSSI", rssi); // Simpan nilai RSSI ke Firebase
  } 

  // Baca data yang dikirim dari Arduino
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

    // Extract MQ4 value
    MQ6 = dt[0].toInt();
    MQ4 = dt[1].toInt();
    MQ138 = dt[2].toInt();
    MQ135 = dt[3].toInt();
    k = dt[4].toInt(); // sampling ke-

    // Dapatkan waktu saat data diterima
    String receivedTime = timeClient.getFormattedTime();

    // Update Firebase with value & timestamp
    if (Firebase.ready() && signupOK) {
      Firebase.RTDB.setFloat(&fbdo, "MQ6 Sensor/Value", MQ6);
      Firebase.RTDB.setFloat(&fbdo, "MQ4 Sensor/Value", MQ4);
      Firebase.RTDB.setFloat(&fbdo, "MQ138 Sensor/Value", MQ138);
      Firebase.RTDB.setString(&fbdo, "MQ135 Sensor/Value", MQ135);
      Firebase.RTDB.setString(&fbdo, "Sampling ke-", k);
      Firebase.RTDB.setString(&fbdo, "Receiver/Time", receivedTime);
    }  
  }

  // Send data to Thinger.io
    sendToThinger(MQ6, receivedTime, k, rssi);

    // Send data to ThingSpeak
    sendToThingSpeak(MQ135, receivedTime, k, rssi);

  delay(5000); // Delay 5 seconds before next iteration
}

void sendToThinger(int MQ6, String receivedTime, int k, long rssi) {
  thing.handle();

  // Membuat objek pson untuk menyimpan data
  protoson::pson data;
  data["MQ6"] = MQ6;
  data["ReceivedTime"] = receivedTime;
  data["Sampling_ke"] = k;
  data["RSSI"] = rssi;

  // Mengirim data ke endpoint yang sesuai di Thinger.io
  thing.call_endpoint("updateData", data);
}

void sendToThingSpeak(int MQ135, String receivedTime, int k, int rssi) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = "http://api.thingspeak.com/update?api_key=" + myWriteAPIKey;
    url += "&field1=" + String(MQ135);
    url += "&field2=" + receivedTime;
    url += "&field3=" + String(k);
    url += "&field4=" + String(rssi);

    Serial.print("Sending data to ThingSpeak: ");
    Serial.println(url);

    http.begin(client, serverName); // HTTP -> url

    int httpCode = http.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("Data sent to ThingSpeak successfully");
        Serial.println(payload);
      }
    } else {
      Serial.println("Error sending data to ThingSpeak");
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected. Unable to send data to ThingSpeak.");
  }
}
