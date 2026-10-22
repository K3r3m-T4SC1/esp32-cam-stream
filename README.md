# esp32-cam-stream

Arduino IDE’ye ESP32 kart paketini kurup kartı tanıttım, kodu yükledim ve donanımın sağlam çalıştığını kontrol ettim. Sonunda kendi ağım üzerinden kameradan canlı görüntü ve fotoğraf alabildim.

## Kullandıklarım
- ESP32-CAM (AI Thinker uyumlu) + ESP32-CAM-MB programlama kartı
- OV2640 kamera
- Arduino IDE 2.3.10, esp32 kart paketi 3.3.12

## Nasıl çalıştırılır
1. `secrets_example.h` dosyasını kopyala, adını `secrets.h` yap, WiFi bilgilerini yaz
2. Arduino IDE'de kart: AI Thinker ESP32-CAM, Partition Scheme: Huge APP
3. Yükle, Serial Monitor'ü 115200'de aç, çıkan IP adresini tarayıcıda aç

## Kaynak
Kamera sunucusu kodu Espressif'in arduino-esp32 paketindeki CameraWebServer örneğinden alındı.

## Sonraki adım
Fotoğrafları periyodik olarak Firebase'e yüklemek.
