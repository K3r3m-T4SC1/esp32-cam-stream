// Ornek dosya. Bu GitHub'a yukleniyor, icinde gercek bilgi yok.
// Kullanmak icin bu dosyanin bir kopyasini "secrets.h" adiyla olusturup doldur.

#pragma once

// WiFi
#define WIFI_SSID              "ag_adin"
#define WIFI_PASSWORD          "sifren"

// Firebase (Project settings -> Web app -> apiKey)
#define FIREBASE_API_KEY       "AIza..."

// Firebase Authentication'da ESP32 icin acilan kullanici
#define FIREBASE_USER_EMAIL    "esp32cam@ornek.local"
#define FIREBASE_USER_PASSWORD "esp32_sifresi"

// Realtime Database adresi (sonunda / olmadan)
#define FIREBASE_DB_URL        "https://PROJE-ID-default-rtdb.europe-west1.firebasedatabase.app"
