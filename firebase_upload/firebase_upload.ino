// ESP32-CAM: belli araliklarla fotograf cekip Firebase Realtime Database'e yukler.
//
// Akis:
//   1) Kamerayi baslat, WiFi'ye baglan, internetten saati al (NTP)
//   2) Firebase'e e-posta/sifre ile giris yap, token al
//   3) Her UPLOAD_INTERVAL_MS'de bir: fotograf cek -> Base64'e cevir -> veritabanina yaz
//
// Gizli bilgiler (WiFi, Firebase) secrets.h dosyasinda, o dosya GitHub'a yuklenmiyor.

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>  // HTTPS (sifreli) baglanti icin
#include <HTTPClient.h>        // HTTP istekleri (POST, PUT) gondermek icin
#include <time.h>              // saat/tarih islemleri
#include "esp_camera.h"
#include "base64.h"            // ESP32 paketinin kendi Base64 cevirici fonksiyonu
#include "secrets.h"

// ---------------- Ayarlar ----------------

// Kac milisaniyede bir fotograf yuklensin. 60000 ms = 1 dakika.
// Dakikada bir yuklersek 1 GB'lik ucretsiz alan yaklasik 2-3 haftada dolar.
const unsigned long UPLOAD_INTERVAL_MS = 60000;

// Firebase'in verdigi token 1 saat gecerli. Biz 50 dakikada bir yeniliyoruz.
const unsigned long TOKEN_LIFETIME_MS = 50UL * 60UL * 1000UL;

// Turkiye saati UTC+3, yani 3 * 3600 saniye
const long GMT_OFFSET_SEC = 3 * 3600;

// ---------------- AI Thinker ESP32-CAM kamera pinleri ----------------
// Kameranin ESP32'nin hangi pinlerine bagli oldugu. Bu kartta sabit.
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

// ---------------- Program boyunca tutulan degiskenler ----------------
String idToken = "";            // Firebase'den aldigimiz giris anahtari
unsigned long tokenTime = 0;    // token'i ne zaman aldik (millis cinsinden)
unsigned long lastUpload = 0;   // son yuklemeyi ne zaman yaptik
bool firstUpload = true;        // acilistaki ilk yuklemeyi beklemeden yapmak icin


// Kamerayi baslatir. Basarili olursa true dondurur.
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;           // kameraya verilen saat sinyali: 20 MHz
  config.pixel_format = PIXFORMAT_JPEG;     // kamera direkt JPEG versin
  // Kamera surucusu tampon (buffer) boyutunu burada verilen cozunurluge gore ayiriyor.
  // VGA verirsek tampon kucuk kaliyor, bazi kareler sigmayinca "FB-OVF" uyarisi cikiyor.
  // Bu yuzden hazir ornekteki gibi en buyuk cozunurlukle (UXGA) baslatip
  // asagida sensor ayarindan VGA'ya dusuruyoruz. Tampon buyuk kaliyor, kare hep sigiyor.
  config.frame_size = FRAMESIZE_UXGA;
  config.jpeg_quality = 12;                 // 0-63 arasi, kucuk sayi = daha kaliteli
  config.fb_count = 2;                      // hafizada iki kare tut
  config.fb_location = CAMERA_FB_IN_PSRAM;  // kareleri kartin ek bellegine (PSRAM) koy
  // GRAB_LATEST: kamera surekli yeni kare ceker, eskisinin uzerine yazar.
  // Boylece istedigimiz an en guncel kareyi aliriz ve tampon tasmaz (FB-OVF uyarisi cikmaz).
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Kamera baslatilamadi, hata: 0x%x\n", err);
    return false;
  }

  // Asil kullanacagimiz cozunurluk: VGA (640x480), bir fotograf ~20-40 KB
  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_VGA);

  Serial.println("Kamera hazir.");
  return true;
}


// WiFi'ye baglanir, baglanana kadar bekler.
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(false);  // WiFi uyku modunu kapat, baglanti daha kararli olur
  Serial.print("WiFi'ye baglaniliyor");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi baglandi, IP: ");
  Serial.println(WiFi.localIP());
}


// Internetteki saat sunucularindan (NTP) dogru saati alir.
// ESP32'nin pili yok, her acilista saati sifirdan ogrenmesi gerekiyor.
void syncTime() {
  configTime(GMT_OFFSET_SEC, 0, "pool.ntp.org", "time.google.com");
  struct tm t;
  Serial.print("Saat aliniyor");
  while (!getLocalTime(&t)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" tamam.");
}


// Su anki zamani "20261022_193000" seklinde yaziya cevirir.
// Bunu hem fotografin adi hem de zaman damgasi olarak kullaniyoruz.
String makeTimestamp() {
  struct tm t;
  if (!getLocalTime(&t)) {
    return String(millis());  // saat alinamazsa en azindan benzersiz bir sayi olsun
  }
  char buf[20];
  strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &t);
  return String(buf);
}


// Firebase Authentication'a e-posta ve sifre ile giris yapar, token alir.
bool signIn() {
  WiFiClientSecure client;
  // Not: setInsecure() sunucunun sertifikasini kontrol etmiyor.
  // Baglanti yine sifreli ama karsi tarafin gercekten Google oldugunu dogrulamiyoruz.
  // Ogrenme projesi icin basitlik adina boyle birakildi.
  client.setInsecure();

  HTTPClient http;
  String url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=";
  url += FIREBASE_API_KEY;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  // Gonderdigimiz JSON: {"email":"...","password":"...","returnSecureToken":true}
  String body = "{\"email\":\"";
  body += FIREBASE_USER_EMAIL;
  body += "\",\"password\":\"";
  body += FIREBASE_USER_PASSWORD;
  body += "\",\"returnSecureToken\":true}";

  int code = http.POST(body);
  String response = http.getString();
  http.end();

  if (code != 200) {
    // 200 = basarili. Degilse Firebase'in hata mesajini yazdir (icinde sifre yok).
    Serial.printf("Firebase girisi basarisiz, HTTP kodu: %d\n", code);
    Serial.println(response);
    return false;
  }

  // Cevabin icinde "idToken": "eyJ..." gibi bir kisim var, onu cekip aliyoruz.
  int keyPos = response.indexOf("\"idToken\"");
  if (keyPos < 0) {
    Serial.println("Cevapta idToken bulunamadi.");
    return false;
  }
  int colonPos = response.indexOf(':', keyPos);
  int start = response.indexOf('"', colonPos) + 1;
  int end = response.indexOf('"', start);
  idToken = response.substring(start, end);
  tokenTime = millis();

  Serial.println("Firebase'e giris yapildi.");
  return true;
}


// Fotograf ceker, Base64'e cevirir ve veritabaninda photos/<zaman> altina yazar.
bool uploadPhoto() {
  // GRAB_LATEST modunda bu her zaman en guncel kare.
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Fotograf cekilemedi.");
    return false;
  }

  // JPEG baytlarini Base64 metne cevir. Boyut yaklasik %33 buyur.
  size_t jpgSize = fb->len;
  String imageB64 = base64::encode(fb->buf, fb->len);
  esp_camera_fb_return(fb);  // kameranin bellegini geri ver, yoksa sonraki kare cekilemez

  String stamp = makeTimestamp();

  // Veritabanina yazacagimiz JSON:
  // {"time":"20261022_193000","size":28541,"image":"/9j/4AAQ..."}
  String body = "{\"time\":\"";
  body += stamp;
  body += "\",\"size\":";
  body += String(jpgSize);
  body += ",\"image\":\"";
  body += imageB64;
  body += "\"}";
  imageB64 = "";  // artik gerek yok, bellegi bosalt

  // Adres: <veritabani>/photos/<zaman>.json?auth=<token>
  // Sondaki ".json" Realtime Database REST API'sinin kurali.
  String url = FIREBASE_DB_URL;
  url += "/photos/";
  url += stamp;
  url += ".json?auth=";
  url += idToken;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  // PUT = "bu adrese bu veriyi koy" demek
  int code = http.PUT(body);
  String response = http.getString();
  http.end();

  if (code == 200) {
    Serial.printf("Yuklendi: photos/%s (%u bayt JPEG)\n", stamp.c_str(), (unsigned)jpgSize);
    return true;
  }

  Serial.printf("Yukleme basarisiz, HTTP kodu: %d\n", code);
  Serial.println(response);
  if (code == 401) {
    // 401 = yetkisiz. Token eskimis olabilir, bir dahaki seferde yeniden giris yapsin.
    idToken = "";
  }
  return false;
}


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("ESP32-CAM Firebase yukleyici basliyor...");

  // Once WiFi ve saat. Kamerayi en son baslatiyoruz ki
  // WiFi beklerken kamera bosuna kare uretip uyari basmasin.
  connectWiFi();
  syncTime();

  if (!initCamera()) {
    // Kamera yoksa yapacak bir sey yok, burada dur.
    while (true) delay(1000);
  }
}


void loop() {
  // WiFi koptuysa tekrar baglan
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Yukleme zamani geldi mi? (millis = kart acildigindan beri gecen milisaniye)
  if (firstUpload || millis() - lastUpload >= UPLOAD_INTERVAL_MS) {
    firstUpload = false;
    lastUpload = millis();

    // Token yoksa ya da eskidiyse yeniden giris yap
    if (idToken == "" || millis() - tokenTime > TOKEN_LIFETIME_MS) {
      if (!signIn()) {
        return;  // giris olmadiysa bu turu atla, bir sonraki turda tekrar dener
      }
    }

    uploadPhoto();
  }

  delay(100);
}
