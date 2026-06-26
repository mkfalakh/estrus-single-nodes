# Estrus Detection & Evaluation Spec — ESP32-S3

Reference for implementing/evaluating the estrus model in Claude Code.
Calibrated from sample `2026-06-19.csv` (animal `SAPI-00`, device `NODE-D9B4DC`).
Rule: estrus is NOT computed from the calendar. Calendar only sets the alert window. Detection = activity deviation vs the animal's own baseline.

## 1. Domain facts
- Estrus cycle: mean 21 d, normal range 18–24 d.
- Injection = cycle day 0 (sync). Next estrus ≈ one full cycle later.
- Estrus sign: activity up, more standing/mounting, less lying.

## 2. CSV schema
`device_id, animal_id, timestamp, sensor1_state, sensor2_state, sensor1_dirty, sensor2_dirty, deviation, estrus, voltage, current, battery_pct`

| Column | Meaning |
|---|---|
| `sensor1_state` 0/1 | standing posture (~75% of day) |
| `sensor2_state` 0/1 | mounting / standing-heat — PRIMARY estrus signal |
| `sensor{1,2}_dirty` 0/1 | sensor dirty/stuck flag |
| `deviation` float | anomaly score (~z-score); 0 normal, rises at estrus |
| `estrus` 0/1 | device flag |
| `current` float | rises when animal active |

Calibration from real data:
- `sensor2_state` on-fraction baseline ≈ 0.25; at estrus ≈ 0.66–0.81 (~3×).
- `estrus=1` iff `deviation > 0`. Device threshold ≈ `deviation ≥ 0.5`.
- Recording interval default 30 s but VARIES (sample = 10 s). Auto-detect from mode of timestamp diffs. Never hardcode.

## 3. Data quality — handle BEFORE detection (all seen in real file)
1. Overlapping exports: file may hold >1 export covering same time range. Detect via points where `timestamp` decreases in original file order. De-overlap procedure:
   ```
   activity(seg) = count(sensor2_state 0→1 edges) / duration_hours
   de-overlap: keep argmax(activity) among all segments covering the same time range
   ```
   No quality-gate pre-filter here — using the algorithm's own stuck/dirty classification to select raw data would be circular when the goal is independent evaluation. Rising-edge rate is intrinsically immune: a stuck sensor (frozen value) produces **zero** transitions regardless of duration, so it always loses `argmax` without any explicit filtering. Real activity produces many transitions (~75.8/h at estrus vs ~1/h calm vs 0/h stuck), making the criterion valid for both sensor conditions.
   NEVER last-write-wins (it hid the estrus peak). The quality gate (§4 Layer 3) is applied **after** de-overlap, on the selected data only.
2. Glued rows: two records joined without newline (`...97.33NODE-D9B4DC,...`). Fix: insert newline before mid-line `device_id` pattern.
3. Stuck sensor: `sensor2` pinned to 1 ~57 min with constant `current` → mark `untrusted`, not "always standing = estrus".

## 4. Algorithm (4 layers)
`LOAD → DE-OVERLAP → QUALITY GATE → FEATURES → SCORE → CALENDAR → flag`

Layer 1 — features per partition window (from `sensor2_state`):
- `on_frac` = active-time fraction (strongest discriminator)
- `rises_per_h` = rising edges (0→1) per hour

Layer 2 — robust z-score vs previous healthy windows:
```
med = median(baseline)
mad = median(|baseline - med|) * 1.4826
z   = (current - med) / mad
```
Use median+MAD (not mean+SD). Candidate if `z >= z_threshold` (start 3.0, tune).

Layer 3 — quality gate (run BEFORE scoring):
Mark window `untrusted` if any `*_dirty=1` OR sensor stuck same value ≥ `dirty_timeout` (default 4 h → samples = `4*3600/interval_s`). Untrusted windows: no estrus flag, excluded from baseline.

Layer 4 — calendar gate (prior). Promote candidate to estrus only inside alert window:
```
inj          = injection_timestamp   # device METADATA, NOT from CSV
expected     = inj + 21 d
alert_window = [inj + 18 d, inj + 24 d]
day_of_cycle(t) = (t - inj) / 1 d
```

## 5. Partition window (critical pitfall)
Options: 24,12,8,6,4,3,2,1 h. Default 3 h is the WORST for short events.
- Estrus starts 20:30 (<1 h); clock-aligned 18:00–21:00 bucket is 90% calm → signal diluted → z ≈ 0 → missed.
- Size AND phase/alignment both matter.

Rules:
- Detection: sliding window 1–2 h, step 15–30 min (event never split).
- Reporting/baseline: larger windows (3–24 h) ok.
- Evaluation: sweep all sizes, pick max F1 across many cycles (not one good day).

## 6. Operational test cases
Injection `01/06/2026 11:00`, partition 3 h, CSV retention 3 d.
Observed estrus in sample: `19/06/2026 20:30` = **day 18** (not 20/21 — see §7).

Case A — record continuously since injection:
- At detection, CSV holds last 3 d (~16–19/06). Baseline ≈24 windows (2 calm days) → robust. Expect stable detection, low FP.

Case B — recording starts 18/06 11:00:
- At detection only ~1 d 9 h data. Baseline ≈8–11 windows, some still in progress → fragile.
- Robust-z (needs ≥4 healthy windows) only valid after ~12 h. Before that: absolute threshold + calendar.
- Cycle day STILL computable from `injection_timestamp` metadata though recording began 18/06. Calendar anchor ≠ data anchor.
- Expect: estrus still detected (large spike) but lower confidence; biased if day-1 anomalous (device fitting, pen-move stress).

Retention 3 d implication:
- Enough for daily detection (~2 d baseline + current day).
- NOT enough for long-term adaptive threshold (7–14 d). Fix: store DAILY SUMMARIES (median & MAD per partition per day) as metadata, not raw CSV → adaptive baseline survives the 3-day purge.

## 7. Calendar correction (test the assumption)
```
injection      01/06/2026 11:00
estrus observed 19/06/2026 20:30
delta          18 d 9 h  → DAY 18, not 20/21
day 20 = 21/06   day 21 = 22/06
```
Day 18 is within normal range (18–24) → valid, just early. Possibilities to test: (a) this animal's cycle ~18 d, (b) example injection date differs, (c) "20–21 d" is only the mean.
Action: predict a WINDOW (days 18–24), not a single day; let sensor deviation set the hour.

## 8. Validation
Ground truth (required, independent of algorithm): visual standing-heat, AI success, or progesterone test. Without it you only compare the model to itself.

Metrics (per animal per cycle):
```
sensitivity = TP/(TP+FN)   # PRIMARY: a miss = lose 1 cycle (~21 d)
specificity = TN/(TN+FP)
precision   = TP/(TP+FP)
F1          = 2*P*R/(P+R)
onset_error_h = |detected_onset - true_onset|   # AI window only ~12–18 h
```

Robustness tests:
- Sweep partition size → max F1 across cycles.
- Truncate to 3 d (Case A) and 1 d (Case B) → check if decision changes.
- Cross-validate across animals (don't overfit one cow/one day).

## 9. Implementation checklist
- [ ] Robust parser: fix glued rows, parse timestamp, drop invalid.
- [ ] Detect overlap segments via decreasing timestamp in original order.
- [ ] De-overlap: keep non-`untrusted`/max-activity segment (NOT last-write-wins).
- [ ] Auto-detect recording interval (mode of timestamp diffs).
- [ ] Quality gate: dirty flag + stuck detection (`≥ dirty_timeout`), before scoring.
- [ ] Features `on_frac` & `rises_per_h` per window (size + step params for sliding).
- [ ] Robust z (median+MAD), baseline = prior healthy windows, configurable `min_baseline`.
- [ ] `injection_timestamp` as separate config/metadata.
- [ ] Calendar alert gate (days 18–24) as final gate.
- [ ] Eval module: confusion matrix + sensitivity/specificity/precision/F1 + onset_error.
- [ ] Partition sweep + retention test (truncate 3 d / 1 d).
- [ ] Store daily summaries (median+MAD per partition) for retention-proof baseline.

## 10. Default params
```yaml
interval_s: auto
partition_detect_h: 1.5      # sliding window for DETECTION
partition_step_min: 30
partition_report_h: 3        # reporting/baseline only
dirty_timeout_h: 4
z_threshold: 3.0             # tune via eval
min_baseline_window: 4
cycle_days: {low: 18, expect: 21, high: 24}
activity_col: sensor2_state
```
