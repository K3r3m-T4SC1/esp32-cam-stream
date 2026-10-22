# Firebase Realtime Database'deki en son fotografi indirip OpenCV ile gosterir.
#
# Akis:
#   1) Firebase'e ESP32 kullanicisi ile giris yap, token al
#   2) photos altindaki en son kaydi iste
#   3) Base64 metni -> JPEG baytlari -> OpenCV resmi -> pencerede goster
#
# Gizli bilgiler firebase_secrets.py dosyasinda, o dosya GitHub'a yuklenmiyor.

import base64

import cv2
import numpy as np
import requests

from firebase_secrets import API_KEY, EMAIL, PASSWORD, DB_URL


def giris_yap():
    """Firebase Authentication'a giris yapar, token dondurur."""
    url = f"https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key={API_KEY}"
    cevap = requests.post(
        url,
        json={"email": EMAIL, "password": PASSWORD, "returnSecureToken": True},
    )
    cevap.raise_for_status()  # 200 degilse hatayi goster ve dur
    return cevap.json()["idToken"]


def son_kaydi_al(token):
    """photos altindaki en son kaydi (adi, icerigi) olarak dondurur."""
    cevap = requests.get(
        f"{DB_URL}/photos.json",
        # Kayitlari adina gore sirala (adlar zaman damgasi), sadece sondakini ver
        params={"auth": token, "orderBy": '"$key"', "limitToLast": 1},
    )
    cevap.raise_for_status()
    veri = cevap.json()
    if not veri:
        raise RuntimeError("Veritabaninda hic fotograf yok.")
    ad, kayit = list(veri.items())[0]
    return ad, kayit


def base64_to_resim(metin):
    """Base64 metni OpenCV'nin kullandigi resim matrisine cevirir."""
    jpg_baytlari = base64.b64decode(metin)
    dizi = np.frombuffer(jpg_baytlari, np.uint8)
    return cv2.imdecode(dizi, cv2.IMREAD_COLOR)


if __name__ == "__main__":
    token = giris_yap()
    print("Firebase'e giris yapildi.")

    ad, kayit = son_kaydi_al(token)
    print("Fotograf:", ad, "| boyut:", kayit["size"], "bayt")

    img = base64_to_resim(kayit["image"])
    print("Resim boyutu (yukseklik, genislik, kanal):", img.shape)

    cv2.imshow(f"ESP32-CAM - {ad}", img)
    print("Kapatmak icin pencere seciliyken bir tusa bas.")
    cv2.waitKey(0)
    cv2.destroyAllWindows()
