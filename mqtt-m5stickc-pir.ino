#include <M5StickC.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "config.h"

WiFiClient wifi_client;
PubSubClient mqtt_client(mqtt_host, mqtt_port, nullptr, wifi_client);

void msg(const std::string &str)
{
  M5.Lcd.setCursor(5, 5, 1);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(2);

  M5.Lcd.printf("%s", str.c_str());
  Serial.println(str.c_str());
}

void reboot() {
  M5.Lcd.fillScreen(RED);
  msg("REBOOT!!!!!");
  delay(3000);
  ESP.restart();
}

void setup() {
  M5.begin();
  M5.Lcd.setRotation(3);
  M5.Axp.ScreenBreath(9); // 7-15

  pinMode(36, INPUT_PULLUP);

  // Wifi
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    switch (count) {
      case 0:
        msg("|");
        break;
      case 1:
        msg("/");
        break;
      case 2:
        msg("-");
        break;
      case 3:
        msg("\\");
        break;
    }
    count = (count + 1) % 4;
  }
  msg("WiFi connected!");
  delay(1000);

  // MQTT
  bool rv = false;
  if (mqtt_use_auth == true) {
    rv = mqtt_client.connect(mqtt_client_id, mqtt_username, mqtt_password);
  }
  else {
    rv = mqtt_client.connect(mqtt_client_id);
  }
  if (rv == false) {
    msg("mqtt connecting failed...");
    reboot();
  }
  msg("MQTT connected!");
  delay(1000);

  // configTime
  configTime(9 * 3600, 0, "ntp.nict.jp");
  struct tm t;
  if (!getLocalTime(&t)) {
    msg("getLocalTime() failed...");
    delay(1000);
    reboot();
  }
  msg("configTime() success!");
  delay(1000);

  M5.Lcd.fillScreen(BLACK);

  M5.Lcd.setCursor(5, 5);
  M5.Lcd.print("PIR sensor");
}

int old_status = 0;

void loop() {
  if (!mqtt_client.connected()) {
    Serial.println("MQTT disconnected...");
    reboot();
  }
  mqtt_client.loop();

  // read PIR pin
  int now_status = digitalRead(36);

  if (now_status == 1 && now_status != old_status) {
    struct tm t;
    if (!getLocalTime(&t)) {
      reboot();
    }
    mqtt_publish(t);

    M5.Lcd.fillRect(140, 60, 20, 20, GREEN);
    delay(500);
    M5.Lcd.fillRect(140, 60, 20, 20, BLACK);

    char buf[128];
    snprintf(buf, 128, "%04d/%02d/%02d", 1900 + t.tm_year, t.tm_mon, t.tm_mday);
    M5.Lcd.setCursor(20, 30, 1);
    M5.Lcd.print(buf);

    snprintf(buf, 128, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    M5.Lcd.setCursor(20, 50, 1);
    M5.Lcd.print(buf);
  }

  old_status = now_status;
}

void mqtt_publish(const struct tm &t) {
  char buf[128];
  snprintf(buf, 128, "{\"t\":\"%04d-%02d-%02dT%02d:%02d:%02d+09:00\"}",
           1900 + t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

  Serial.println(buf);
  mqtt_client.publish(mqtt_publish_topic, buf);

  std::string sub_topic = "";
  sub_topic += mqtt_publish_topic;
  sub_topic += "/last_update_t";
  mqtt_client.publish(sub_topic.c_str(), buf, true);
}
