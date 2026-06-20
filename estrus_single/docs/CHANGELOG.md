# Changelog

---

## [Unreleased] — 2026-06-20

### Breaking — Android API changes

#### `GET /api/config` + `POST /api/config`
| Old field | New field | Unit | Range |
|---|---|---|---|
| `min_baseline_samples` | `min_baseline_windows` | windows | 2–48 |
| `dirty_timeout_samples` | `dirty_timeout_min` | minutes | 10–480 |

- `partition_hours` valid values narrowed to `3,4,6,8,12,24` (1 and 2 removed).
- Default `partition_hours` changed from 1 → 3.
- Default `estrus_threshold_pct` changed from 6.0 → 75.0 (maps to z_threshold=3.0).
- New validation error strings: `"invalid min_baseline_windows cfg"`, `"invalid dirty_timeout_min cfg"`.

#### `GET /api/node/estrus`
| Field | Before | After |
|---|---|---|
| `baseline_samples` | count of raw samples | **renamed** `baseline_windows` — count of healthy partition windows |
| `threshold_pct` | mirrors `estrus_threshold_pct` from config | **always 100.0** |
| `deviation_pct` | `(current−baseline)/baseline × 100` | `(z / z_threshold) × 100`; ≥ 100 = threshold crossed |
| `current_rate` | `(sensor1 && sensor2) / total × 100` | `sensor2_state on_frac × 100` |
| `baseline_rate` | mean standing ratio from history | median `sensor2` on_frac from healthy windows |
| `is_estrus_window` | days 18–22 | **days 20–21** |

---

### Changed — Estrus model (estrus_model.cpp)
- Detection now uses a **1.5 h sliding circular buffer** of `sensor2_state` only (sensor1 no longer used for scoring).
- Features computed per window: `on_frac` (primary) and `rises_per_h` (0→1 edges, available for future use).
- **Quality gate**: window marked `untrusted` if any sample in the buffer had `sensor1_dirty` or `sensor2_dirty`; untrusted windows produce no estrus flag and are excluded from baseline.
- **Z-score detection**: `z = (on_frac − median) / normalized_MAD`. `z_threshold = estrus_threshold_pct / 100 × 4.0`.
- **Calendar gate**: estrus flag only raised on cycle days 20–21 from `injection_date`. If no injection date is set the calendar gate is bypassed.

### Changed — Baseline computation (storage_stats.cpp)
- Per-partition baseline now stores **median + normalized MAD** (MAD × 1.4826) instead of mean + sample count.
- Dirty windows (any row with `sensor1_dirty=1` or `sensor2_dirty=1`) excluded from baseline.
- `getCachedBaseline()` signature changed to return `medianRate`, `madRate`, `nWindows`.

### Changed — Config (config_runtime.h/.cpp)
- `min_baseline_windows` (uint8_t, 2–48, default 4) replaces `min_baseline_samples`.
- `dirty_timeout_min` (uint16_t, 10–480 min, default 240 = 4 h) replaces `dirty_timeout_samples`.
- NVS keys updated: `base_win`, `dirty_min` (old keys ignored on first load → defaults applied).

### Changed — Dirty detection (sens_proximity.cpp)
- Stuck-sensor threshold converted from samples to minutes: `threshold_samples = (dirty_timeout_min × 60) / record_interval_sec`.
- `updateGlobalStats(standing)` replaced by `updateSensor2(s2, d1, d2)`.

### Changed — System state (system_state.h/.cpp)
- `baseline_samples` (uint32_t) replaced by `baseline_windows` (uint16_t).

### Added — Prefill sliding window from CSV (estrus_model.cpp)
- `prefillSlidingWindow()` dipanggil satu kali di `setup()` setelah `triggerBaselineRecompute()`, sebelum task sensor dimulai.
- Membaca N record terakhir dari CSV hari ini (fallback ke kemarin jika kurang) lalu menyuntikkannya ke `slidingBuf` urutan oldest-first, sehingga buffer langsung penuh setelah reboot tanpa menunggu 1,5 jam data live.
- Record dengan `sensor1_dirty=1` atau `sensor2_dirty=1` dilewati (tidak diinjeksi).
- Buffer dialokasikan di heap (`malloc`) agar tidak memenuhi stack `setup()`.
- Log saat boot: `📂 prefill: injected=N skipped_dirty=M window=N/N`.

### Added — Window count di API (web_server.cpp)
- `GET /api/node/estrus` kini menyertakan dua field baru:
  - `window_count` — jumlah sampel yang sudah ada di buffer saat ini
  - `window_size` — kapasitas penuh buffer (1,5 jam ÷ `record_interval_sec`)
- Android dapat menampilkan progress warmup: `window_count / window_size`.
- `valid=true` dan `baseline_windows` terisi dapat muncul langsung setelah reboot jika data CSV tersedia.

### Fixed — NaN/Inf float di JSON (web_server.cpp)
- `handleLatest()`: `voltage`, `current`, `power`, `battery_percent` mengembalikan `"0.00"` (bukan `"null"`) saat nilai NaN/Inf.
- `handleLatest()`: `battery_days` mengembalikan `"0.0"` (bukan `"null"`) saat nilai NaN/Inf.
- `handleEstrus()`: `current_rate`, `baseline_rate`, `deviation_pct` mengembalikan `"0.0"` (bukan `"null"`) saat nilai NaN/Inf.

### Fixed — triggerBaselineRecompute() dipanggil setelah response (web_server.cpp)
- Sebelumnya `handleSetConfig()` memanggil `triggerBaselineRecompute()` inline di tengah proses, sehingga respons HTTP bisa timeout pada SD card lambat.
- Sekarang recompute ditunda ke akhir handler (setelah semua perubahan config diproses), hanya dipanggil jika `partitionChanged`, `retentionChanged`, atau `baselineWindowChanged`.

### Changed — API spec notes (docs/api-specs.md)
- `estrus_threshold_pct` documented as z_threshold alias: `z_threshold = val / 100 × 4.0`.
- `deviation_pct` in `/api/node/estrus` documented as `(z / z_threshold) × 100`.
- `threshold_pct` fixed at 100.0.
- `is_estrus_window` corrected to days 20–21.
- `partition_hours` minimum raised to 3 h; detection sliding window (1.5 h internal) documented separately.
- `min_baseline_windows` and `dirty_timeout_min` field notes added.
