# Asansör Simülasyon Sistemi

Bu proje, Arduino tabanlı 4 katlı bir asansör simülasyonudur. Proje; motor kontrolü, ekran arayüzleri, durum makinesi (state machine) ve buton sinyallerinin yazılımsal olarak filtrelenmesi (debounce) gibi temel gömülü sistem konseptlerini içermektedir.

## Temel Özellikler

- **Kat Kontrolü:** İç ve dış çağrı butonlarıyla 4 kat arası tam bağımsız çalışma imkanı.
- **Durum Makinesi Mimarisi:** Asansörün bekleme, yukarı/aşağı hareket ile kapı açılıp kapanma süreçleri güvenli ve belirli durumlara ayrılarak yönetilir.
- **Motor Entegrasyonu:**
  - Kabin hareketi için **Stepper (Adım)** motoru,
  - Kapı kontrol mekanizması için **Servo** motoru.
- **Görsel Bildirimler:**
  - **TM1637 7-Segment Ekran:** Hedef ve mevcut katın gerçek zamanlı gösterimi.
  - **SSD1306 OLED Ekran:** Asansörün çalışma ve işlem durumunun takibi.
- **Sinyal Kararlılığı:** Butonlarda oluşabilecek mekanik titreşimleri engellemek amacıyla gelişmiş debounce algoritmaları.

## Kullanılan Teknolojiler ve Kütüphaneler

- **Platform:** Atmel AVR / Arduino Uno
- **Geliştirme Ortamı:** PlatformIO, Wokwi (Simülasyon Desteği)
- **Öne Çıkan Kütüphaneler:**
  - `Servo` & `Stepper`
  - `Adafruit GFX` & `Adafruit SSD1306`
  - `TM1637`

## Dosya Yapısı

- `src/main.cpp`: Asansör mantığını barındıran temel C++ kod dosyası.
- `platformio.ini`: PlatformIO bağımlılıkları ve geliştirme kartı konfigürasyonu.
- `wokwi.toml` / `diagram.json`: Donanım prototipleme ve şema yapısının tanımlandığı Wokwi simülasyon dosyaları.

## Başlangıç

Proje bir PlatformIO projesidir ve gereksinim duymadan entegre çevre birimleriyle lokal olarak derlenebilir veya simüle edilebilir.

1. VS Code veya tercih ettiğiniz IDE üzerinde *PlatformIO* eklentisini kurun.
2. Proje dizininde (files klasörü) terminali açarak gerekli bağımlılıkları indirmek ve kodları derlemek için kurulumları yapın veya doğrudan PlatformIO menüsünden **Build** (Derle) işlemine tıklayın.
3. Hazır devre şemasıyla projenizi Wokwi simülasyon aracılığıyla ekstra donanıma gereksinim duymadan çalıştırıp test edin.
