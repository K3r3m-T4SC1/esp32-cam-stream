# esp32-cam-stream

Bitirme projem için danışman hocamın verdiği pratik ödev. İstenen iki şey vardı: ESP32-CAM ile fotoğraf çekip WiFi üzerinden bilgisayarda görmek, sonra da fotoğrafları belli aralıklarla Firebase'e yüklemek. İkisini de yaptım. Bir de Firebase'deki son fotoğrafı indirip OpenCV ile açan küçük bir Python kodu ekledim.

## Kullandıklarım

- ESP32-CAM (AI Thinker uyumlu) + ESP32-CAM-MB programlama kartı
- OV2640 kamera
- Arduino IDE 2.3.10, esp32 kart paketi 3.3.12
- Firebase, ücretsiz Spark plan (Realtime Database + Authentication)
- Python, requests, OpenCV

## Klasörler

- Ana klasör: 1. kısım, kameradan canlı görüntü
- `firebase_upload/`: 2. kısım, fotoğrafları Firebase'e yükleyen kod
- `python/`: Firebase'deki son fotoğrafı indirip gösteren kod

## 1. kısım: WiFi üzerinden görüntü

Arduino IDE'ye ESP32 kart paketini kurup kartı tanıttım, kodu yükledim ve donanımın sağlam çalıştığını kontrol ettim. Sonunda kendi ağım üzerinden kameradan canlı görüntü ve fotoğraf alabildim.

Bu kısmın kodu Espressif'in CameraWebServer örneği. Değiştirdiğim şeyler kamera modelini AI Thinker yapmak ve WiFi bilgilerini `secrets.h` dosyasına almak oldu. Kart ağa bağlanınca seri monitöre IP adresini yazıyor, tarayıcıdan o adrese girince kontrol sayfası açılıyor. Sadece aynı ağdaki cihazlardan erişilebiliyor.

## 2. kısım: Firebase'e yükleme

Kart dakikada bir fotoğraf çekip Firebase Realtime Database'e yüklüyor. Burada web sayfası yok, kart kimseyi beklemeden fotoğrafı kendisi gönderiyor. Yani karta hangi kodu yüklersem o çalışıyor, ikisi aynı anda değil.

Normalde fotoğraf gibi dosyalar Firebase Storage'da tutulur. Ama Storage artık sadece ücretli planda (Blaze) açılabiliyor ve kart tanımlamak gerekiyor, benim kartım kabul edilmedi. O yüzden ücretsiz planda kalıp Realtime Database kullandım. Veritabanı sadece yazı tuttuğu için fotoğrafı Base64'e çevirip öyle kaydediyorum. Her fotoğraf `photos/<tarih_saat>` altında duruyor, içinde `image` (Base64), `size` ve `time` var.

Kodun akışı:

1. WiFi'ye bağlan, NTP'den saati al
2. Firebase Authentication'a ESP32 için açtığım kullanıcıyla giriş yap, token al (50 dakikada bir yeniliyor)
3. Fotoğraf çek, Base64'e çevir, REST API'ye PUT isteği at

Hazır Firebase kütüphanesi yok, istekler ESP32'nin kendi HTTPClient'ı ile atılıyor. Veritabanı kuralı sadece bu kullanıcının `photos` altına okuyup yazmasına izin veriyor.

## Karşılaştığım sorunlar

- WiFi şifresini başta direkt kodun içine yazmıştım. GitHub'a koymadan önce `secrets.h` dosyasına taşıdım ve `.gitignore`'a ekledim.
- Firebase Storage kart istedi, yukarıda yazdığım gibi Realtime Database'e geçtim.
- 2. kısımda seri monitör sürekli `cam_hal: FB-OVF` uyarısıyla doluyordu. Fotoğraflar yine yükleniyordu ama uyarı durmadı. Önce kamerayı WiFi bağlandıktan sonra başlatmayı, sonra kameranın bellek tamponunu büyütmeyi denedim, ikisi de işe yaramadı. Kameranın saat frekansını (XCLK) 20 MHz'den 10 MHz'e düşürünce uyarı tamamen gitti. Kameradan gelen veri hızı yarıya inince ESP32 yetişebildi.

## Nasıl çalıştırılır

**1. kısım**

1. `secrets_example.h` dosyasını kopyalayıp adını `secrets.h` yap, WiFi bilgilerini yaz
2. Arduino IDE'de kart: AI Thinker ESP32-CAM, Partition Scheme: Huge APP
3. Yükle, Serial Monitor'ü 115200'de aç, çıkan IP adresini tarayıcıda aç

**2. kısım**

1. Firebase'de proje aç, Realtime Database'i ve Authentication'da Email/Password girişini aç, ESP32 için bir kullanıcı ekle
2. Realtime Database kurallarını bu kullanıcının UID'sine göre ayarla:

```json
{
  "rules": {
    "photos": {
      ".read": "auth != null && auth.uid === 'KULLANICI_UID'",
      ".write": "auth != null && auth.uid === 'KULLANICI_UID'"
    }
  }
}
```

3. `firebase_upload/secrets_example.h` dosyasını `secrets.h` olarak kopyala, WiFi ve Firebase bilgilerini yaz
4. `firebase_upload.ino`'yu karta yükle, seri monitörde `Yuklendi: photos/...` satırları gelmeli

**Python**

1. `pip install requests opencv-python numpy`
2. `python/firebase_secrets_example.py` dosyasını `firebase_secrets.py` olarak kopyalayıp doldur
3. `python` klasöründe `python son_fotograf.py` çalıştır, son fotoğraf bir pencerede açılıyor

## Notlar

- `secrets.h` ve `firebase_secrets.py` GitHub'a yüklenmiyor, ikisi de `.gitignore`'da.
- Bir fotoğraf yaklaşık 11-12 KB. Dakikada bir yüklemede ücretsiz 1 GB'lık alan birkaç haftada dolar, uzun süre çalıştıracaksam aralığı artırmam ya da eski kayıtları silmem gerekecek.
- HTTPS bağlantılarında sunucu sertifikasını kontrol etmiyorum (`setInsecure`), basit kalsın diye böyle bıraktım.

## Kaynak

Ana klasördeki kamera sunucusu kodu Espressif'in arduino-esp32 paketindeki CameraWebServer örneğinden alındı.
