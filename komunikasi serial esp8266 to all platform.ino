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

#define WIFI_SSID "bangg?"
#define WIFI_PASSWORD "hwvv1213"
#define API_KEY "AIzaSyBnPADOix0EWKu29eysInzDlbA5Rvv9Lz8"
#define DATABASE_URL "https://bismillah-ambil-data-beneran-default-rtdb.asia-southeast1.firebasedatabase.app"

//THINGSPEAK
// WiFiClient klien;
const char* server = "api.thingspeak.com";
const long unsigned int CHANNEL_ID = 2474972; // Replace with your ThingSpeak Channel ID
const char* apiKey = "TGV422YJ3IVZY879"; // Replace with your ThingSpeak API Key

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
void sendThing(int MQ135, int k, String receivedTime, int rssi);

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

  /* connection to firebase */
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase connection successful");
    signupOK = true;
  } else {
    Serial.printf("Firebase connection failed: %s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback;  //see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Initialize NTP client
  timeClient.begin();
  timeClient.setTimeOffset(25200);  // GMT+7 for Jakarta, Indonesia

  // client.setInsecure();
  // ThingSpeak.begin(klien);
}

void loop() {
  // Get current time
  timeClient.update();

  // Get RSSI value
  rssi = WiFi.RSSI();

  // Update Firebase with value & timestamp
  if (Firebase.ready() && signupOK) {
    Firebase.RTDB.setInt(&fbdo, "WiFi/RSSI", rssi);  // Simpan nilai RSSI ke Firebase
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

    // Extract MQ & sampling value
    MQ6 = dt[0].toInt();
    MQ4 = dt[1].toInt();
    MQ138 = dt[2].toInt();
    MQ135 = dt[3].toInt();
    k = dt[4].toInt();  // sampling ke-

    sendThing(MQ135, k, receiver, rssi);

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
}

    void sendThing(int MQ135, int k, String receivedTime, int rssi) {
      if (client.connect(server, 80)) {
        String postStr = apiKey;
        postStr += "&field1=" + String(MQ135) + "&field2=" + String(rssi) + "&field3=" + String(k) + "\r\n\r\n";

        client.print("POST /update HTTP/1.1\n");
        client.print("Host: api.thingspeak.com\n");
        client.print("Connection: close\n");
        client.print(String("X-THINGSPEAKAPIKEY: ") + apiKey + "\n");
        //client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
        client.print("Content-Type: application/x-www-form-urlencoded\n");
        client.print("Content-Length: ");
        client.print(postStr.length());
        client.print("\n\n");
        client.print(postStr);

        Serial.print("MQ135: " + String(MQ135) + " | RSSI: " + String(rssi) + " | Sampling: " + String(k));
        Serial.println("%. Sent to ThingSpeak.");
      }
      
      delay(500);
      client.stop();
      Serial.println("Waiting...");
      
      // ThingSpeak needs a minimum 15 sec delay between updates.
      delay(1000);
    }
