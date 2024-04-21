#include "esp_camera.h"
#include <WiFi.h>
#include "AsyncTelegram2.h"
#include <WiFiClientSecure.h>

WiFiClientSecure client;

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#define LAMP_PIN 4

const char* ssid = "******";
const char* password = "******";
const char* token = "******";
int64_t userid = 111111;

#define MYTZ "MSK-3"

AsyncTelegram2 myBot(client);

int lampChannel = 7;          // a free PWM channel (some channels used by camera)
const int pwmfreq = 50000;    // 50K pwm frequency
const int pwmresolution = 9;  // duty cycle bit range
const int pwmMax = pow(2, pwmresolution) - 1;

// Lamp Control
void setLamp(int newVal) {
  if (newVal != -1) {
    // Apply a logarithmic function to the scale.
    int brightness = round((pow(2, (1 + (newVal * 0.02))) - 2) / 6 * pwmMax);
    ledcWrite(lampChannel, brightness);
    Serial.print("Lamp: ");
    Serial.print(newVal);
    Serial.print("%, pwm = ");
    Serial.println(brightness);
  }
}

// Setup

void startWifi() {
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.println("Connecting to wifi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
}

void configureCamera(camera_config_t* config) {
  config->ledc_channel = LEDC_CHANNEL_0;
  config->ledc_timer = LEDC_TIMER_0;
  config->pin_d0 = Y2_GPIO_NUM;
  config->pin_d1 = Y3_GPIO_NUM;
  config->pin_d2 = Y4_GPIO_NUM;
  config->pin_d3 = Y5_GPIO_NUM;
  config->pin_d4 = Y6_GPIO_NUM;
  config->pin_d5 = Y7_GPIO_NUM;
  config->pin_d6 = Y8_GPIO_NUM;
  config->pin_d7 = Y9_GPIO_NUM;
  config->pin_xclk = XCLK_GPIO_NUM;
  config->pin_pclk = PCLK_GPIO_NUM;
  config->pin_vsync = VSYNC_GPIO_NUM;
  config->pin_href = HREF_GPIO_NUM;
  config->pin_sccb_sda = SIOD_GPIO_NUM;
  config->pin_sccb_scl = SIOC_GPIO_NUM;
  config->pin_pwdn = PWDN_GPIO_NUM;
  config->pin_reset = RESET_GPIO_NUM;
  config->xclk_freq_hz = 20000000;
  config->frame_size = FRAMESIZE_XGA;
  config->pixel_format = PIXFORMAT_JPEG;
  config->grab_mode = CAMERA_GRAB_LATEST;
  config->fb_location = CAMERA_FB_IN_PSRAM;
  config->jpeg_quality = 10;
  config->fb_count = 2;

  pinMode(LAMP_PIN, OUTPUT);                       // set the lamp pin as output
  ledcSetup(lampChannel, pwmfreq, pwmresolution);  // configure LED PWM channel
  setLamp(0);                                      // set default value
  ledcAttachPin(LAMP_PIN, lampChannel);            // attach the GPIO pin to the channel
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  startWifi();

  camera_config_t config;
  configureCamera(&config);

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Init Telegram bot

  // Sync time with NTP
  configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
  client.setCACert(telegram_cert);

  myBot.setTelegramToken(token);

  Serial.println("Test Telegram connection... ");
  myBot.begin();

  char welcome_msg[64];
  snprintf(welcome_msg, 64, "BOT @%s online.", myBot.getBotName());
  myBot.sendTo(userid, welcome_msg);
}

void loop() {
  delay(10000);

  TBMessage msg;
  size_t len = sendPicture(msg);

  Serial.printf("Sent bytes: %d\n", len);

  if (len) {
    Serial.println("Picture sent successfull");
  } else {
    myBot.sendMessage(msg, "Error, picture not sent.");
  }
}

size_t sendPicture(TBMessage& msg) {
  setLamp(20);
  delay(500);
  // Take Picture with Camera;
  Serial.println("Camera capture requested");

  // Take picture with Camera and send to Telegram
  camera_fb_t* fb = esp_camera_fb_get();
  setLamp(0);
  if (!fb) {
    Serial.println("Camera capture failed");
    return 0;
  }

  // Serial.println("Frame buffer len");
  // Serial.println(fb->len);
  // Serial.println(fb->width);
  // Serial.println(fb->height);

  myBot.sendMessage(msg, "Sending photo");
  myBot.sendPhoto(msg, fb->buf, fb->len);

  // Clear buffer
  esp_camera_fb_return(fb);
  return fb->len;
}