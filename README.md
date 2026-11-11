# Smart Room Monitor — ESP32

Firmware monitor ruangan berbasis **ESP32** (Arduino + FreeRTOS, PlatformIO). Perangkat membaca suhu, kelembapan, intensitas cahaya, dan gerakan; menampilkannya di OLED; menyalakan alarm lokal (LED + buzzer) saat suhu melewati ambang; serta mengirim telemetri dan menerima perintah ubah ambang lewat **MQTT**.

## Fitur

- Pembacaan **DHT22** (suhu, kelembapan), **LDR** (cahaya), dan **PIR** (gerakan).
- **Alarm suhu** dengan state machine `NORMAL → ALARM → ACKED`, histeresis 0,5 °C, dan tombol **ACK** (debounce 30 ms).
- **Tampilan lokal** OLED SSD1306 128×64: suhu, kelembapan, cahaya, gerakan, ambang, status alarm, status Wi-Fi/MQTT.
- **MQTT**: telemetri JSON tiap 5 detik, perintah ubah ambang, status online/offline lewat Last Will (retained).
- **Ambang suhu disimpan di NVS**, jadi tidak hilang saat perangkat restart.
- **Non-blocking**: setiap fungsi berjalan di task FreeRTOS sendiri; Wi-Fi dan MQTT reconnect dengan exponential backoff.
- **Penanganan error**: DHT22 gagal 3 kali beruntun dianggap fault (nilai basi tidak ditampilkan atau dikirim); OLED dicek ulang tiap 5 detik bila tidak terdeteksi.

## Daftar Komponen

| No | Komponen | Jumlah | Keterangan |
|----|----------|--------|------------|
| 1 | ESP32 DevKit (DOIT DevKit V1 / DevKit C) | 1 | Mikrokontroler utama |
| 2 | Sensor suhu & kelembapan DHT22 | 1 | Data digital 1-wire |
| 3 | Modul LDR (photoresistor, output analog `AO`) | 1 | Intensitas cahaya |
| 4 | Sensor gerak PIR | 1 | Output digital |
| 5 | OLED SSD1306 128×64, I2C (alamat `0x3C`) | 1 | Tampilan lokal |
| 6 | LED merah | 1 | Indikator alarm |
| 7 | LED hijau | 1 | Indikator normal |
| 8 | Buzzer aktif | 1 | Bunyi alarm |
| 9 | Push button | 1 | Tombol ACK |
| 10 | Resistor 220 Ω | 3 | Seri untuk LED merah, LED hijau, dan buzzer |
| 11 | Breadboard + kabel jumper | 1 set | |

## Tabel Pin

| Komponen | Pin komponen | GPIO ESP32 | Keterangan |
|----------|--------------|-----------|------------|
| DHT22 | SDA / DATA | **GPIO 4** | VCC ke 3V3, GND ke GND |
| LDR (modul) | AO | **GPIO 34** | ADC1, input-only, attenuation 11 dB (~0–3,3 V), rata-rata 4 sampel |
| PIR | OUT | **GPIO 27** | VCC ke 3V3, GND ke GND |
| LED hijau | Anoda (via R 220 Ω) | **GPIO 25** | Katoda ke GND |
| LED merah | Anoda (via R 220 Ω) | **GPIO 26** | Katoda ke GND |
| Buzzer | `+` (via R 220 Ω) | **GPIO 33** | Pin `−` ke GND |
| Push button | Pin 1 | **GPIO 14** | `INPUT_PULLUP`, pin 2 ke GND (aktif LOW) |
| OLED SSD1306 | SDA | **GPIO 21** | I2C, VCC ke 3V3, GND ke GND |
| OLED SSD1306 | SCL | **GPIO 22** | I2C |

Semua GND disatukan (common ground). DHT22, LDR, PIR, dan OLED disuplai dari 3V3.

> Pada hardware nyata, modul PIR HC-SR501 umumnya disuplai 5 V (pin VIN); outputnya 3,3 V sehingga aman untuk ESP32. Jika memakai DHT22 tanpa modul (4 pin), tambahkan resistor pull-up 10 kΩ pada jalur data.

Pin dapat diubah di [`include/config.h`](include/config.h).

## Skema Rangkaian

Skema rangkaian ada di [`diagram.json`](diagram.json) (proyek Wokwi). Buka file itu lewat ekstensi Wokwi di VS Code, atau salin isinya ke proyek ESP32 baru di [wokwi.com](https://wokwi.com).

## Cara Build dan Flash

### Prasyarat

- Semua library diunduh otomatis dari `platformio.ini`:
  `DHT sensor library`, `Adafruit Unified Sensor`, `Adafruit SSD1306`, `Adafruit GFX Library`, `PubSubClient`, `ArduinoJson` (v7).

### Langkah

1. Clone repo ini dan buka foldernya di VS Code.
2. Atur konfigurasi jaringan di [`include/secrets.h`](include/secrets.h) (lihat bagian berikut).
3. Build, flash, dan buka serial monitor:

```bash
pio run                      # build
pio run -t upload            # flash ke ESP32 (colok via USB)
pio device monitor           # serial monitor, 115200 baud
```

Di VS Code, ketiganya juga tersedia sebagai tombol **Build**, **Upload**, dan **Serial Monitor** di status bar PlatformIO.

### Konfigurasi `include/secrets.h`

| Makro | Nilai default | Keterangan |
|-------|---------------|------------|
| `WIFI_SSID` | `Wokwi-GUEST` | SSID Wi-Fi simulator Wokwi. Ganti dengan SSID jaringan Anda untuk hardware nyata |
| `WIFI_PASS` | `""` | Password Wi-Fi (kosong untuk Wokwi) |
| `MQTT_HOST` | `broker.hivemq.com` | Broker publik untuk pengujian |
| `MQTT_PORT` | `1883` | Tanpa TLS |
| `DEVICE_ID` | `room-rizqi-1234` | Harus **unik**; dipakai pada topik dan client ID |

### Menjalankan di Wokwi (simulator)

1. Jalankan `pio run` lebih dulu. `wokwi.toml` memakai hasil build di `.pio/build/esp32doit-devkit-v1/` (folder ini tidak di-commit).
2. Di VS Code dengan ekstensi **Wokwi Simulator**: tekan `F1` → **Wokwi: Start Simulator**.
3. Untuk menguji alarm, klik DHT22 di simulator lalu naikkan suhu di atas ambang (default 30 °C). Klik PIR untuk memicu gerakan. Tekan tombol hijau untuk ACK.

## MQTT

### Broker dan topik

- **Broker**: `broker.hivemq.com`, port `1883` (publik, tanpa autentikasi dan TLS).
- **Client ID**: `<DEVICE_ID>-<4 hex dari MAC>`, contoh `room-rizqi-1234-A1B2`.

| Topik | Arah | Isi | Catatan |
|-------|------|-----|---------|
| `smartroom/room-rizqi-1234/telemetry` | Publish | JSON telemetri | Tiap 5 detik, dan segera setelah terhubung atau ambang berubah |
| `smartroom/room-rizqi-1234/command` | Subscribe | JSON perintah | Mengubah ambang suhu |
| `smartroom/room-rizqi-1234/status` | Publish | `online` / `offline` | Retained; `offline` dikirim broker lewat Last Will (QoS 1) |

> Broker ini publik, jadi pastikan `DEVICE_ID` unik agar topik tidak bertabrakan dengan pengguna lain. Jika `DEVICE_ID` diubah, ganti juga bagian `room-rizqi-1234` pada topik di atas.

### Format telemetri

```json
{
  "device_id": "room-rizqi-1234",
  "temp_c": 27.4,
  "humidity": 61.2,
  "light_raw": 2048,
  "motion": false,
  "alarm": false,
  "uptime_s": 125,
  "threshold": 30.0,
  "sensor_ok": true
}
```

| Field | Tipe | Keterangan |
|-------|------|------------|
| `device_id` | string | ID perangkat |
| `temp_c` | number / `null` | Suhu (°C, 1 desimal); `null` jika DHT22 fault |
| `humidity` | number / `null` | Kelembapan (%RH, 1 desimal); `null` jika DHT22 fault |
| `light_raw` | integer | Nilai ADC mentah LDR (0–4095) |
| `motion` | boolean | `true` jika ada gerakan sejak sampel sebelumnya (PIR di-latch, disampling tiap 100 ms) |
| `alarm` | boolean | `true` hanya saat state `ALARM` (belum di-ACK) |
| `uptime_s` | integer | Lama perangkat menyala (detik) |
| `threshold` | number | Ambang suhu alarm saat ini (°C) |
| `sensor_ok` | boolean | `false` jika DHT22 fault |

### Perintah ubah ambang

Publish ke `smartroom/room-rizqi-1234/command`:

```json
{"threshold": 32.5}
```

- Rentang valid **10–60 °C**. Di luar rentang, JSON rusak, atau payload > 128 byte akan diabaikan dan dicatat di serial log.
- Jika valid, ambang disimpan ke NVS dan telemetri dikirim segera sehingga nilai baru langsung terlihat.

## Logika Alarm

```
NORMAL --(suhu > ambang)--------------------> ALARM
ALARM  --(tombol ACK)-----------------------> ACKED
ALARM / ACKED --(suhu <= ambang - 0,5 °C)---> NORMAL   (re-arm)
```

| State | LED hijau | LED merah | Buzzer | OLED (`Alarm:`) |
|-------|-----------|-----------|--------|-----------------|
| `NORMAL` | Menyala (jika DHT22 OK) | Mati | Mati | `NORMAL` |
| `ALARM` | Mati | Menyala terus | Beep 1 Hz | `ALARM!` |
| `ACKED` | Mati | Berkedip pelan (1 Hz) | Mati | `ACK` |

- Setelah di-ACK, alarm tidak berbunyi lagi sampai suhu turun ke `ambang − 0,5 °C` (histeresis), lalu naik melewati ambang lagi.
- Saat DHT22 fault, state alarm **ditahan**: tidak memicu alarm baru dan tidak membatalkan alarm yang sedang aktif. OLED menampilkan `ERR` untuk suhu dan kelembapan.

## Arsitektur Firmware

Setiap tugas berjalan sebagai task FreeRTOS terpisah. Data antar-task dibagi lewat modul `state` yang dilindungi mutex (RAII `Lock`), sehingga tidak ada task yang menyentuh variabel global task lain. Fungsi tunggu memakai `vTaskDelayUntil` / notifikasi task, tanpa `delay()` yang memblokir.

| Task | Core | Prioritas | Periode | Fungsi |
|------|------|-----------|---------|--------|
| `button` | 1 | 4 | 10 ms | Debounce (3 × 10 ms), kirim event ACK ke alarm lewat task notification |
| `alarm` | 1 | 3 | 100 ms | State machine, LED, buzzer |
| `sensors` | 1 | 2 | 100 ms (PIR), 5 s (DHT22 + LDR) | Pembacaan sensor dan validasi nilai |
| `net` | 0 | 2 | 50 ms | Wi-Fi, MQTT, telemetri, perintah, Last Will |
| `display` | 1 | 1 | 500 ms | Render OLED |
| `monitor` | 1 | 1 | 30 s | Log sisa stack (high-water mark) dan heap |

## Struktur Proyek

```
.
├── platformio.ini        # Board, framework, dan dependensi
├── wokwi.toml            # Konfigurasi simulator Wokwi
├── diagram.json          # Skema rangkaian (Wokwi)
├── include/
│   ├── config.h          # Pin, timing, ambang, prioritas dan stack task
│   ├── secrets.h         # Wi-Fi, broker MQTT, DEVICE_ID
│   ├── state.h           # State bersama antar-task (mutex)
│   ├── log.h             # Makro log serial
│   └── *.h               # Antarmuka tiap modul
└── src/
    ├── main.cpp          # Inisialisasi dan pembuatan task
    ├── state.cpp         # State bersama + ambang di NVS
    ├── sensors.cpp       # DHT22, LDR, PIR
    ├── alarm.cpp         # State machine alarm, LED, buzzer
    ├── button.cpp        # Debounce tombol ACK
    ├── display.cpp       # OLED SSD1306
    └── net.cpp           # Wi-Fi, MQTT, telemetri, perintah
```

## Dokumentasi

- **Proyek Wokwi**: _(isi tautan proyek Wokwi)_
- **Tangkapan layar rangkaian**: _(isi path gambar, mis. `docs/wokwi-circuit.png`)_
- **Video demo**: _(isi tautan video 2–3 menit)_

## Penulis

Rizqi Dian Anggara
