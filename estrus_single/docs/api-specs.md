# Web Server API Specifications

ESP32 Web Server — RESTful JSON API + static file serving.

---

## Endpoints

### AUTH

#### GET `/api/check` — Session validation
- **Auth**: None (always responds)
- **Request**: HTTP cookie `ESPSESSIONID=<sid>`
- **Response** `200 OK`:
```json
{"auth":true,"node_id":"NODE-XX"}
```
- **Session expired / missing**: same response with `"auth":false`
- **Notes**: Auth check is commented out; returns node ID regardless of auth state.

#### GET `/api/login` — Authenticate
- **Request** (query params):
  | Param | Type   | Required | Notes                         |
  |-------|--------|----------|-------------------------------|
  | user  | string | Yes      | Must match `USER` constant    |
  | pass  | string | Yes      | Password, hashed before check |
- **Response** `200 OK`:
```json
{"success":true}            // login successful
{"success":false}           // invalid credentials
```
- **Response** `500`: `{"error":"session penuh"}`
- **Side effect**: Sets `Set-Cookie: ESPSESSIONID=<sid>; Path=/; SameSite=Lax; Max-Age=3600`

#### GET `/api/logout` — Destroy session
- **Response** `200 OK`:
```json
{"success":true}
```
- **Side effect**: Clears cookie (`Max-Age=0`)

---

### CONFIG

#### GET `/api/config` — Read current config
- **Response** `200 OK`:
```json
{
  "node_id": "NODE-XX",
  "animal_id": "ANIMAL-XX",
  "ap_password": "wifi_pass_here",
  "prox_low": 1,
  "alarm_enabled": 1,
  "led_brightness": 128,
  "no_activity_timeout_hours": 6,
  "record_interval_sec": 60,
  "retention_days": 7,
  "partition_hours": 6,
  "estrus_threshold_pct": 75.00,
  "stop_after_alarm": 0,
  "min_baseline_windows": 4,
  "dirty_timeout_min": 240,
  "current_threshold": 100.00,
  "power_threshold": 400.00,
  "injection_date": "2026-06-01"
}
```
- **Notes**: Sends `ap_password` plaintext over unencrypted HTTP (AP mode). `injection_date` is empty string `""` if not set. `battery_pct` is no longer returned here (moved to `/api/node/latest`).

#### POST `/api/config` — Update config
- **Request** (JSON body — all fields optional):
  | Parameter                | Type     | Range                  | Runtime Effect                          | Needs Restart |
  |--------------------------|----------|------------------------|-----------------------------------------|---------------|
  | node_id                  | string   | 1–8 chars, alphanumeric| Device ID                               | Yes           |
  | animal_id                | string   | 1–8 chars              | Animal tag                              | No            |
  | ap_password              | string   | 8–20 chars             | WiFi AP password                        | Yes           |
  | prox_low                 | int      | 0 or 1                 | Proximity sensor active level           | No            |
  | alarm_enabled            | int      | 0 or 1                 | Enable/disable buzzer                   | No            |
  | led_brightness           | int      | 1–255                  | LED brightness (PWM)                    | No            |
  | no_activity_timeout_hours| int      | 1–24                   | Hours of no activity before dirty mark  | No*           |
  | record_interval_sec      | int      | 10–3600                | CSV write interval                      | No            |
  | retention_days           | int      | 1–14                   | File cleanup age                        | No*           |
  | partition_hours          | int      | Divisor of 24, min 3 (3,4,6,8,12,24) | Time bucket for reporting/baseline windows. Detection uses a fixed internal sliding window (1.5 h, step 30 min) regardless of this value. | No |
  | estrus_threshold_pct     | float    | 0–100                  | Estrus trigger % — **reinterpreted internally as `z_threshold`**: `z_threshold = estrus_threshold_pct / 100 * 4.0` (max z=4). Default 75 → z=3.0 (SPEC §10 default); lower % = more sensitive. See SPEC §4 Layer 2. | No |
  | stop_after_alarm         | int      | 0 or 1                 | Auto-stop vs acknowledge               | No            |
  | min_baseline_windows     | int      | 2–48                   | Min healthy windows required before z-score baseline is valid | No |
  | dirty_timeout_min        | int      | 10–480                 | Minutes a sensor must be stuck at same value before marked dirty/untrusted | No |
  | current_threshold        | float    | 100–150                | Current anomaly %                      | No            |
  | power_threshold          | float    | 400–600                | Power anomaly %                        | No            |
  | injection_date           | string   | `"YYYY-MM-DD"` or `""`| Hormone injection date; empty to clear | No            |

- **Notes on `injection_date`**: Used to synchronize/shorten the natural 21-day reproductive cycle. Estrus typically shows ~day 20–21 from injection. Format must be exactly `YYYY-MM-DD` (length 10, dashes at positions 4 and 7); send `""` to clear.

- **Response** `200 OK`:
```json
{"success":true,"restart":false,"message":"config updated"}
```
- **Response** `400`: Validation error, e.g.:
```json
{"error":"invalid node_id cfg"}
{"error":"invalid ap_password"}
{"error":"invalid record_interval_sec cfg"}
{"error":"invalid injection_date, use YYYY-MM-DD"}
{"error":"invalid led_brightness cfg"}
{"error":"invalid no_activity_timeout_hours cfg"}
```
- **Side effects**: Calls `saveConfig()`, logs changes, applies runtime updates, sets `pendingRestart=true` if needed.
- **Notes**: All fields validated individually; a single bad field rejects the entire batch. `no_activity_timeout_hours` and `dirty_timeout_min` changes trigger `resetDirtyDetection()`. `partition_hours`, `retention_days`, or `min_baseline_windows` changes trigger `triggerBaselineRecompute()`.

#### POST `/api/config/reset` — Factory reset
- **Response** `200 OK`: `{"reset":true}`
- **Side effect**: Calls `resetConfig()`

---

### DEVICE / MONITORING

#### GET `/api/node/latest` — Hardware snapshot
- **Response** `200 OK`:
```json
{
  "time": "2026-06-12T14:30:00",
  "sensor1": 1,
  "sensor2": 1,
  "sensor1_dirty": 0,
  "sensor2_dirty": 0,
  "sensor1_no_activity": 0,
  "sensor2_no_activity": 0,
  "voltage": 3700.00,
  "current": 150.00,
  "power": 55500.00,
  "battery_percent": 85.00,
  "battery_days": "120.5",
  "battery_date": "2026-10-10",
  "sd": 1,
  "rtc": 1,
  "ina": 1,
  "alarm": 0,
  "wifi": 1,
  "low_battery": 0
}
```
- **Field notes**:
  - `time` — `"invalid"` if `SYS.rtc_ok` is false.
  - `sensor1_no_activity` / `sensor2_no_activity` — `1` if sensor has been inactive for `no_activity_timeout_hours` hours.
  - `voltage`, `current`, `power`, `battery_percent` — float with 2 decimal places; `"0.00"` if NaN/Inf.
  - `battery_days` — string `"0.0"` if NaN/Inf.
  - `ina` — INA219 power monitor status (`1` = OK, `0` = fault). Replaces old `sensor` and `buzzer` fields.

#### GET `/api/node/estrus` — Estrus model state
- **Response** `200 OK`:
```json
{
  "partition": 2,
  "current_rate": 85.3,
  "baseline_rate": 60.0,
  "deviation_pct": 42.2,
  "threshold_pct": 100.0,
  "baseline_windows": 150,
  "estrus": 0,
  "valid": 0,
  "injection_date": "2026-06-01",
  "cycle_day": 20,
  "is_estrus_window": 1,
  "window_count": 8,
  "window_size": 1.5
}
```
- **Field notes**:
  - `current_rate` — `on_frac` of `sensor2_state` for the current detection window (0–1 scale × 100).
  - `baseline_rate` — median `on_frac` across prior healthy windows (× 100).
  - `deviation_pct` — `(z / z_threshold) * 100`; 100 = threshold reached, >100 = exceeded. Internally computed via robust z-score (median+MAD, see SPEC §4 Layer 2).
  - `threshold_pct` — always `100.0`; threshold is crossed when `deviation_pct ≥ 100`.
  - `baseline_windows` — count of healthy windows in the baseline (renamed from `baseline_samples`).
  - `valid` — `1` if `baseline_windows >= min_baseline_windows`, else `0` (changed from boolean).
  - `injection_date` — mirrors `sysConfig.injection_date`; `""` if not set.
  - `cycle_day` — days elapsed since injection + 1 (day 1 = injection day). `0` if no injection date or RTC not available.
  - `is_estrus_window` — `1` if `cycle_day` is in the detection window (**days 18–21**; changed from 20–21); `0` otherwise. Always `0` when no injection date is set.
  - `window_count` — total number of sliding windows maintained.
  - `window_size` — size of each sliding window in hours (fixed 1.5h).

#### GET `/api/node/history` — CSV data page
- **Query params**:
  | Param   | Default | Range     | Description        |
  |---------|---------|-----------|--------------------|
  | date    | —       | YYYY-MM-DD| Required           |
  | page    | 0       | ≥ 0       | Zero-based offset  |
  | limit   | 10      | 1–20      | Rows per page      |
- **Response** `200 OK`:
```json
{
  "date": "2026-06-12",
  "page": 0,
  "limit": 10,
  "count": 10,
  "rows": [{"device_id":"NODE-XX","animal_id":"...","timestamp":"...","sensor1_state":0,...}],
  "has_next": true
}
```
- **Response** `400`: `{"error":"invalid date","field":"date","reason":"must be YYYY-MM-DD"}`
- **Response** `503`: `{"error":"sd_busy"}` — SD mutex not acquired within 3 s
- **Response** `500`: `{"error":"sd"}` or `{"error":"oom"}`
- **Notes**: CSV files on SD are stored as `/data/YYYY-MM-DD.csv` (no node-id prefix). A missing file returns `200` with `"rows":[]` and `"has_next":false`.

#### GET `/api/node/health` — System health flags
- **Status**: ⚠️ **DISABLED** — handler is commented out in firmware. Route is **not registered**.
- **Notes**: Previously replaced `/api/system`. Currently not available; use `/api/node/latest` for health indicators.

#### GET `/api/node/device` — Device identity
- **Response** `200 OK`:
```json
{
  "node_id": "NODE-XX",
  "animal_id": "ANIMAL-XX",
  "ap_ssid": "ESTRUS-NODE-XX",
  "mac": "AA:BB:CC:DD:EE:FF",
  "firmware": "v1.0.0"
}
```

#### GET `/ping` — Liveness check
- **Response** `200 OK`: `OK` (text/plain)

#### GET `/` — Web UI
- **Response**: Serves `/www/index.html` from SD card
- **Static files**: `.html` → text/html, `.css` → text/css, `.js` → application/javascript

---

### CONTROL

#### GET `/api/alarm/status` — Alarm detail
- **Response** `200 OK`:
```json
{
  "alarm_active": false,
  "alarm_estrus": false,
  "alarm_fault": false,
  "alarm_fault_muted": false,
  "alarm_ack": false,
  "stop_after_alarm": false
}
```

#### POST `/api/alarm/stop` — Stop alarm
- **Response** `200 OK`: `{"success":true}`
- **Behavior**: If `stop_after_alarm==true` → acknowledges alarm; else → stops buzzer immediately.

#### POST `/api/alarm/start` — Resume alarm
- **Response** `200 OK`: `{"success":true}`
- **Behavior**: Clears `alarm_ack` and `fault_alarm_muted` so alarm can sound again.

---

### DATA

#### GET `/api/download` — Stream retention CSV
- **No query params required** — uses `RTC now` and `sysConfig.retention_days` to determine the date window.
- **Behavior**: Streams all CSV files within the configured retention window (oldest → newest), filtered to the device's `node_id` and `animal_id`, deduplicated to the **latest record per minute** (chronological order).
- **Headers**: `Content-Disposition: attachment; filename=NODE-XX-retention.csv`
- **Response** `200 OK`: `text/csv` stream
  ```csv
  device_id,animal_id,timestamp,sensor1_state,sensor2_state,sensor1_dirty,sensor2_dirty,deviation,estrus,voltage,current,battery_pct
  ...
  ```
- **Response** `503`: `RTC not ready` or `SD not available`
- **Notes**: Reads files in chunks (32-line send buffer) with SD mutex release between chunks to avoid starving other tasks.

---

### RTC

#### GET `/api/rtc` — Read device time
- **Response** `200 OK`:
```json
{
  "timestamp": "2026-06-12T14:30:00",
  "epoch": 1749739800,
  "lost_power": false,
  "ever_synced": true,
  "drift_seconds": 0
}
```
- **Notes**: `epoch` is UTC (GMT+7 offset removed). `lost_power` reflects DS3231 power-loss flag. `drift_seconds` — accumulated RTC drift since last sync.

#### POST `/api/rtc/sync` — Synchronize RTC time
- **Request** (JSON body):
```json
{"epoch": 1749739800}
```
- **Response** `200 OK`: `{"success":true}`
- **Response** `400`: `{"error":"invalid json"}` or `{"error":"invalid epoch"}`
- **Side effects**: Adjusts DS3231 (applies GMT+7 offset internally), sets `rtc_ever_synced=true`, saves sync state to NVS, resumes CSV write.

#### POST `/api/rtc/clear` — Reset RTC time and sync state *(dev only)*
- **Response** `200 OK`: `{"success":true}`
- **Side effects**: Calls `resetRTC()`, clears `rtc_ever_synced`, saves sync state to NVS, pauses CSV write.

---

### SYSTEM

#### GET `/api/storage` — SD card & queue stats
- **Response** `200 OK`:
```json
{
  "retention_days": 7,
  "csv_rows_today": 1440,
  "free_sd_mb": 4.2,
  "used_sd_mb": 0.8,
  "log_queue": 0,
  "sensor_queue": 3
}
```
- **If SD not ready**: `{"sd":false}`

---

### UPDATE / OTA

#### POST `/api/update/check` — Pre-flight version compare
- **Purpose**: Compare an uploaded build's versions against the device before streaming the actual binary. Does NOT upload firmware.
- **Request** (JSON body):
```json
{
  "firmware_version": "1.1.0",
  "web_version": "1.0.0"
}
```
  | Field            | Type   | Required | Notes                                  |
  |------------------|--------|----------|----------------------------------------|
  | firmware_version | string | No       | SemVer `MAJOR.MINOR.PATCH`; `""` if omitted |
  | web_version      | string | No       | SemVer `MAJOR.MINOR.PATCH`; `""` if omitted |
- **Response** `200 OK`:
```json
{
  "success": true,
  "firmware_same": false,
  "firmware_newer": true,
  "web_same": true,
  "web_newer": false
}
```
  - `*_same` — exact string match vs device constant (`FIRMWARE_VERSION` / `WEB_VERSION`).
  - `*_newer` — incoming SemVer strictly greater (major→minor→patch precedence).
- **Response** `400`: `{"success":false}` — missing body or invalid JSON.

#### POST `/api/update/firmware` — OTA firmware upload
- **Request**: `multipart/form-data` file upload — the compiled `.bin`.
```
POST /api/update/firmware HTTP/1.1
Content-Type: multipart/form-data; boundary=----xYz

------xYz
Content-Disposition: form-data; name="firmware"; filename="estrus-1.1.0.bin"
Content-Type: application/octet-stream

<binary .bin payload>
------xYz--
```
- **Behavior**: Version is read from the binary's `esp_app_desc_t` header on the first write chunk and compared to `FIRMWARE_VERSION`. If not newer, `Update.abort()` is called and the OTA fails — note the device only responds **after the full file has streamed** (no early rejection mid-upload).
- **Response** `200 OK`: `{"success":true,"restart":true}` — flash succeeded; device reboots ~3 s later.
- **Response** `500`: `{"success":false}` — write failed, or firmware not newer than current.

#### GET `/api/update/status` — OTA progress poll
- **Response** `200 OK`:
```json
{
  "updating": false,
  "progress": 0,
  "status": ""
}
```
  - `updating` — `true` while an upload is in flight.
  - `progress` — `0–100` (% of `upload.totalSize` written).
  - `status` — `""` (idle, before first upload) | `"uploading"` | `"success"` | `"failed"`.

#### POST `/api/update/web` — Web dashboard OTA *(disabled)*
- Currently commented out in firmware; route is **not registered**. Calling it falls through to static file handling / `404`.

---

## Removed / Disabled Endpoints

| Endpoint | Reason |
|---|---|
| `GET /api/system` | Merged into `GET /api/node/health` — `error` field added there |
| `GET /api/node/health` | ⚠️ Handler commented out in current firmware; use `/api/node/latest` instead |
| `GET /api/download?date=YYYY-MM-DD` | Changed to retention-window streaming; no `?date=` param needed |

---

## Known Issues / TODOs

1. **Auth disabled** — all `isAuthenticated()` checks are commented out across handlers.
2. **Manual JSON building** — handlers use string concatenation instead of ArduinoJson, prone to formatting errors.
3. **Config validation repetition** — copy-pasted if-blocks in `handleSetConfig()`; a schema table would reduce ~200 lines.
4. **Boolean encoding inconsistency** — `/api/node/latest` uses `0/1` integers; some fields return strings for NaN handling.
5. **PSRAM static buffer** — `handleHistory()` uses static `lines[MAX_LIMIT][160]` buffer (PSRAM fallback commented out).
