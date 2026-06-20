# estrus-single-nodes

Hanya menggunakan 1 buah ESP32-S3 untuk project estrus.

# Perhatikan

Folder `www` untuk Dashboard, wajib copy folder ke sdcard.

# WiFi AP

SSID: `ESTRUS-NODE-{mac address}` | Pwd: `estrus123`

---

# Algoritma Deteksi Estrus

## Alur Umum

```
Sensor2 setiap N detik
        ↓
[Sliding Window 1.5 jam]
        ↓
  on_frac = jumlah_aktif / total_sampel
        ↓ ada dirty? → untrusted, berhenti
[Baseline per partisi waktu]
  median & MAD dari data N hari lalu
        ↓ hari < min_baseline_windows? → tidak valid, berhenti
[Robust Z-Score]
  z = (on_frac - median) / MAD_norm
        ↓
[Threshold]
  z_threshold = estrus_threshold_pct / 100 × 4.0
        ↓
[Calendar Gate]
  cycle_day hari ke-20 atau 21 sejak injeksi?
        ↓
  estrus = (z >= z_threshold) AND calendar_ok
```

---

## Step 1 — Sliding Window

Setiap `record_interval_sec` detik, satu sampel `sensor2_state` (boolean) masuk ke circular buffer.

```
window_size = (1.5 × 3600) / record_interval_sec
```

Contoh: interval 10 s → window = 540 sampel = 1,5 jam terakhir.

```
on_frac = jumlah_sampel_aktif / window_size
```

Contoh: sensor aktif 27 menit dalam 1,5 jam → `on_frac = 27/90 = 0.30`

Jika ada sampel **dirty** (sensor kotor atau stuck) di dalam window → `untrusted = true`, evaluasi berhenti.

---

## Step 2 — Baseline: Median + MAD per Partisi

Baseline dibangun dari file CSV harian dalam `retention_days`, bukan dari data real-time.

**Partisi waktu** — hari dibagi ke slot berdasarkan `partition_hours`:

| `partition_hours` | Jumlah slot | Contoh slot |
|---|---|---|
| 6 | 4 | 00–05, 06–11, 12–17, 18–23 |
| 4 | 6 | 00–03, 04–07, … |
| 1 | 24 | per jam |

Ini memisahkan pola perilaku siang/malam agar baseline tidak tercampur.

Untuk setiap file harian, per partisi dihitung `on_frac` satu hari penuh. Window yang mengandung dirty diabaikan. Dari kumpulan nilai itu:

```
baseline_median = median( on_frac_hari_1, on_frac_hari_2, ..., on_frac_hari_N )

MAD_raw  = median( |on_frac_i - baseline_median| )
MAD_norm = MAD_raw × 1.4826
```

Faktor **1.4826** adalah konstanta normalisasi agar MAD skala-nya setara dengan standar deviasi pada distribusi normal (Iglewicz & Hoaglin, 1993).

Floor `MAD_norm ≥ 0.0001` mencegah pembagian nol jika semua hari identik.

---

## Step 3 — Gate: `min_baseline_windows`

```
if N_hari_sehat < min_baseline_windows → valid = false
```

Jika hari sehat yang terkumpul kurang dari nilai ini, baseline dianggap belum representatif dan deteksi diblokir.

| Nilai | Efek |
|---|---|
| 2 | Model aktif cepat, baseline rapuh |
| **4 (default)** | Keseimbangan antara kecepatan dan ketepatan |
| 8–12 | Lebih stabil, butuh data lebih lama |

---

## Step 4 — Robust Z-Score

```
z = (on_frac_sekarang - baseline_median) / MAD_norm
```

Ini adalah **modified z-score** — menggunakan median dan MAD sebagai pengganti mean dan SD sehingga tahan terhadap outlier. Satu hari anomali tidak merusak baseline.

Interpretasi:
- `z = 0` — perilaku persis seperti baseline
- `z = 2` — aktivitas 2 MAD di atas normal
- `z = 3` — lonjakan signifikan (default threshold)

---

## Step 5 — Threshold: `estrus_threshold_pct`

```
z_threshold = estrus_threshold_pct / 100 × 4.0
```

| `estrus_threshold_pct` | `z_threshold` | Karakteristik |
|---|---|---|
| 25 | 1.0 | Sangat sensitif, false positive tinggi |
| 50 | 2.0 | Moderat |
| **75 (default)** | **3.0** | **Standar robust z-score** |
| 100 | 4.0 | Konservatif, hanya lonjakan ekstrem |

`deviation_pct` yang tampil di UI dan CSV:

```
deviation_pct = (z / z_threshold) × 100
```

Nilai **≥ 100%** berarti threshold terlampaui. Ini normalisasi agar dapat dibaca tanpa mengetahui nilai z mentah.

---

## Step 6 — Calendar Gate

Jika `injection_date` diset (format `YYYY-MM-DD`):

```
cycle_day = floor( (now - injection_date) / 86400 ) + 1
```

Estrus hanya bisa trigger jika `cycle_day` adalah **hari ke-20 atau 21** siklus (sesuai siklus estrus sapi). Jika `injection_date` kosong, gate ini dilewati — semua hari dianggap valid.

---

## Keputusan Final

```
estrus = (z >= z_threshold) AND calendar_ok
```

Kedua kondisi harus terpenuhi sekaligus.

---

## Parameter Konfigurasi

| Parameter | Default | Range | Fungsi |
|---|---|---|---|
| `record_interval_sec` | — | 10–3600 s | Interval pengambilan sampel sensor |
| `partition_hours` | — | 1–24 | Lebar slot partisi waktu untuk baseline |
| `retention_days` | 7 | 1–14 hari | Jumlah hari historis untuk baseline |
| `min_baseline_windows` | 4 | 2–48 | Minimum hari sehat sebelum deteksi aktif |
| `estrus_threshold_pct` | 75 | 0–100 | Sensitivitas threshold (75 → z ≥ 3.0) |
| `injection_date` | kosong | YYYY-MM-DD | Tanggal injeksi; mengaktifkan calendar gate |
