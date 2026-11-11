# Catatan Desain — Smart Room Monitor

## Keputusan desain

**1. FreeRTOS, bukan `loop()` + `millis()`.**
- *Debugging lebih mudah*: tiap fungsi (tombol, alarm, sensor, jaringan, OLED) terisolasi di task bernama sendiri. Task `monitor` mencatat sisa stack dan heap tiap 30 detik, jadi masalah memori terlihat sebelum menjadi crash.
- *Tanpa `delay()` yang memblokir*: setiap task menunggu lewat `vTaskDelayUntil` atau task notification. Satu pekerjaan lambat (mis. reconnect Wi-Fi) tidak menahan yang lain, dan periode tetap stabil tanpa drift.
- *Berbasis prioritas*: scheduler bersifat preemptive. Tombol ACK (prioritas 4) > alarm (3) > sensor dan jaringan (2) > OLED dan monitor (1). Pada `millis()` semua pekerjaan berbagi satu loop berurutan, sehingga respons tombol bergantung pada pekerjaan terlama dalam satu putaran.

**2. Pemisahan core.** Wi-Fi dan MQTT di core 0 (bersama stack Wi-Fi bawaan ESP32), logika lokal di core 1. Alarm dan tombol tetap responsif meski jaringan lambat atau putus.

**3. State bersama lewat satu modul + mutex.** Task tidak menyentuh variabel global task lain, sehingga tidak ada data race. Akses dibungkus RAII (`Lock`) agar mutex selalu dilepas.

**4. ACK lewat task notification.** Alarm task dibangunkan langsung saat tombol ditekan, tanpa polling flag. Tombol di-debounce 3 × 10 ms dan hanya bereaksi pada tepi turun, jadi menahan tombol tidak mengulang ACK.

**5. State machine alarm dengan histeresis.** `NORMAL → ALARM → ACKED`, re-arm saat suhu ≤ ambang − 0,5 °C agar alarm tidak berkedip-kedip di sekitar ambang. Saat sensor fault, state ditahan (tidak memicu dan tidak membatalkan alarm).

**6. Data sensor tidak pernah basi.** DHT22 divalidasi rentangnya; gagal 3 kali beruntun dianggap fault, OLED menampilkan `ERR`, dan telemetri mengirim `null`, bukan angka lama.

**7. MQTT yang tahan gangguan.** Reconnect non-blocking dengan exponential backoff, Last Will `offline` (retained), telemetri dikirim tepat sekali per sampel baru, dan perintah divalidasi (JSON, panjang, rentang 10–60 °C). Ambang disimpan di NVS hanya bila berubah, untuk menghemat siklus tulis flash.

## Masalah yang ditemui

- **DHT22 butuh ±2 detik setelah power-up** sebelum bacaan valid → sensor task menunggu 2 detik di awal.
- **Gerakan PIR singkat bisa terlewat** di antara sampel 5 detik → PIR disampling tiap 100 ms dan di-*latch* sampai telemetri berikutnya.
- **ADC ESP32 berisik** → LDR dirata-rata dari 4 sampel, dengan attenuation 11 dB untuk rentang ~0–3,3 V.
- **Broker publik dipakai bersama** sehingga topik bisa bertabrakan → `DEVICE_ID` unik, dan client ID ditambah potongan MAC.

## Yang akan diperbaiki jika ada waktu lebih

1. **Watchdog**: task watchdog agar task yang macet memicu reset otomatis.
2. **Pengujian otomatis**: fungsi `step()` alarm sudah murni, sehingga mudah diuji dengan `pio test` tanpa hardware.
3. **Keandalan data**: buffer telemetri saat offline, QoS 1, dan timestamp NTP (saat ini hanya `uptime_s`).
4. **Kalibrasi sensor**: konversi LDR ke lux dan kalibrasi ADC ESP32 (saat ini nilai mentah 0–4095).
5. **Provisioning Wi-Fi** (WiFiManager atau BLE) dan **OTA update**, agar tidak perlu flash ulang untuk ganti jaringan atau firmware.
