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
- **Notes**: Sends `ap_password` plaintext over unencrypted HTTP (AP mode). `injection_date` is empty string `""` if not set.

#### POST `/api/config` — Update config
- **Request** (JSON body — all fields optional):
  | Parameter                | Type     | Range                  | Runtime Effect                          | Needs Restart |
  |--------------------------|----------|------------------------|-----------------------------------------|---------------|
  | node_id                  | string   | 1–8 chars, alphanumeric| Device ID                               | Yes           |
  | animal_id                | string   | 1–8 chars              | Animal tag                              | No            |
  | ap_password              | string   | 8–20 chars             | WiFi AP password                        | Yes           |
  | prox_low                 | int      | 0 or 1                 | Proximity sensor active level           | No            |
  | alarm_enabled            | int      | 0 or 1                 | Enable/disable buzzer                   | No            |
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
```
- **Side effects**: Calls `saveConfig()`, logs changes, applies runtime updates, sets `pendingRestart=true` if needed.
- **Notes**: All fields validated individually; a single bad field rejects the entire batch.

#### POST `/api/config/reset` — Factory reset
- **Response** `200 OK`: `{"reset":true}`
- **Side effect**: Calls `resetConfig()`

---

### DEVICE / MONITORING

#### GET `/api/node/latest` — Hardware snapshot
- **Response** `200 OK`:
```json
{
  "node_id": "NODE-XX",
  "animal_id": "ANIMAL-XX",
  "time": "2026-06-12T14:30:00",
  "sensor1": 1,
  "sensor2": 1,
  "sensor1_dirty": 0,
  "sensor2_dirty": 0,
  "voltage": 3700,
  "current": 150,
  "power": 55500,
  "battery_percent": 85,
  "battery_days": "120.5",
  "battery_date": "2026-10-10",
  "sd": 1,
  "rtc": 1,
  "sensor": 1,
  "wifi": 1,
  "buzzer": 0,
  "sensor_dirty": 0,
  "alarm": 0,
  "low_battery": 0
}
```

#### GET `/api/node/estrus` — Estrus model state
- **Response** `200 OK`:
```json
{
  "partition": 2,
  "current_rate": 85.3,
  "baseline_rate": 60.0,
  "deviation_pct": 42.2,
  "threshold_pct": 75.00,
  "baseline_samples": 150,
  "estrus": 0,
  "valid": true,
  "injection_date": "2026-06-01",
  "cycle_day": 20,
  "is_estrus_window": 1
}
```
- **Field notes**:
  - `current_rate` — `on_frac` of `sensor2_state` for the current detection window (0–1 scale × 100).
  - `baseline_rate` — median `on_frac` across prior healthy windows (× 100).
  - `deviation_pct` — `(z / z_threshold) * 100`; 100 = threshold reached, >100 = exceeded. Internally computed via robust z-score (median+MAD, see SPEC §4 Layer 2).
  - `threshold_pct` — always `100.0`; threshold is crossed when `deviation_pct ≥ 100`.
  - `injection_date` — mirrors `sysConfig.injection_date`; `""` if not set.
  - `cycle_day` — days elapsed since injection + 1 (day 1 = injection day). `0` if no injection date or RTC not available.
  - `is_estrus_window` — `1` if `cycle_day` is in the detection window (days 20–21); `0` otherwise. Always `0` when no injection date is set.

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
- **Response** `400`: `{"error":"invalid date"}`
- **Response** `503`: `{"error":"sd_busy"}` — SD mutex not acquired within 3 s
- **Response** `500`: `{"error":"sd"}` or `{"error":"oom"}`
- **Notes**: CSV files on SD are stored as `/data/YYYY-MM-DD.csv` (no node-id prefix). A missing file returns `200` with `"rows":[]` and `"has_next":false`.

#### GET `/api/node/health` — System health flags
- **Response** `200 OK`:
```json
{
  "sd": true,
  "rtc": true,
  "sensor": true,
  "sensor_dirty": false,
  "wifi": true,
  "alarm": false,
  "low_battery": false,
  "error": false
}
```
- **Notes**: `error` is the system fault flag (true when any critical subsystem is in fault state). Replaces the removed `/api/system` endpoint.

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

#### GET `/api/download` — Download CSV file
- **Query param**: `date=YYYY-MM-DD` (required)
- **Headers**: `Content-Type: text/csv`, `Content-Disposition: attachment; filename=NODE-XX-YYYY-MM-DD.csv`
- **Response** `400`: `Missing date`
- **Response** `404`: `File not found`
- **Response** `503`: `RTC not ready` or `SD not available`

---

### RTC

#### GET `/api/rtc` — Read device time
- **Response** `200 OK`:
```json
{
  "timestamp": "2026-06-12T14:30:00",
  "epoch": 1749739800,
  "lost_power": false,
  "ever_synced": true
}
```
- **Notes**: `epoch` is UTC (GMT+7 offset removed). `lost_power` reflects DS3231 power-loss flag.

#### POST `/api/rtc/sync` — Synchronize RTC time
- **Request** (JSON body):
```json
{"epoch": 1749739800}
```
- **Response** `200 OK`: `{"success":true}`
- **Response** `400`: `{"error":"invalid json"}` or `{"error":"invalid epoch"}`
- **Side effects**: Adjusts DS3231, sets `rtc_ever_synced=true`, resumes CSV write.

#### POST `/api/rtc/clear` — Reset RTC time and sync state *(dev only)*
- **Response** `200 OK`: `{"success":true}`
- **Side effects**: Calls `resetRTC()`, clears `rtc_ever_synced`, pauses CSV write.

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

## Removed Endpoints

| Endpoint | Reason |
|---|---|
| `GET /api/system` | Merged into `GET /api/node/health` — `error` field added there |

---

## Known Issues / TODOs

1. **Auth disabled** — all `isAuthenticated()` checks are commented out across 16 handlers.
2. **Manual JSON building** — 14+ handlers use string concatenation instead of ArduinoJson, prone to formatting errors.
3. **Config validation repetition** — 14 copy-pasted if-blocks in `handleSetConfig()`; a schema table would reduce ~200 lines.
4. **Boolean encoding inconsistency** — `/api/node/health` uses `true/false` strings; `/api/node/latest` uses `0/1` integers.
5. **PSRAM alloc per history request** — `handleHistory()` allocates/frees on every call; a static buffer would be more efficient.
