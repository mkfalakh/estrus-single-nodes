/* Refactored app.js — API_BASE aware and aligned with PRD final endpoints */
const API_BASE = window.API_BASE || "";

// State
let latestData = null;
let configCache = null;
let currentPage = 0;
let hasNextPage = false;

// Elements
const el = {
  nodeTitle: document.getElementById("nodeTitle"),
  animalIdLabel: document.getElementById("animalIdLabel"),
  rtcTimeHeader: document.getElementById("rtcTimeHeader"),
  ledBrightness: document.getElementById("ledBrightness"),
  ledBrightnessValue: document.getElementById("ledBrightnessValue"),
  freeSDCard: document.getElementById("freeSDCard"),
  usedSDCard: document.getElementById("usedSDCard"),
  logQueue: document.getElementById("logQueue"),
  sensorQueue: document.getElementById("sensorQueue"),
  csvRowsToday: document.getElementById("csvRowsToday"),
  retentionDayStat: document.getElementById("retentionDayStat"),

  // estrus
  partitionHours: document.getElementById("partitionHours"),
  currentRate: document.getElementById("currentRate"),
  baselineRate: document.getElementById("baselineRate"),
  deviationPct: document.getElementById("deviationPct"),
  estrusThreshold: document.getElementById("estrusThreshold"),
  baselineWindow: document.getElementById("baselineWindow"),
  baselineValid: document.getElementById("baselineValid"),
  estrusStatus: document.getElementById("estrusStatus"),
  cowCondition: document.getElementById("cowCondition"),
  injectionDate: document.getElementById("injectionDate"),
  cycleDay: document.getElementById("cycleDay"),
  windowCount: document.getElementById("windowCount"),
  windowSize: document.getElementById("windowSize"),
  estrusWindow: document.getElementById("estrusWindow"),

  // sensors
  sensor1: document.getElementById("sensor1"),
  sensor2: document.getElementById("sensor2"),
  sensor1Condition: document.getElementById("sensor1Condition"),
  sensor2Condition: document.getElementById("sensor2Condition"),

  // rtc modules
  rtcTime: document.getElementById("rtcTime"),
  deviceTime: document.getElementById("deviceTime"),
  rtcDrift: document.getElementById("rtcDrift"),
  rtcSyncStatus: document.getElementById("rtcSyncStatus"),
  rtcLostPower: document.getElementById("rtcLostPower"),
  syncRtcBtn: document.getElementById("syncRtcBtn"),

  // battery
  batteryPercent: document.getElementById("batteryPercent"),
  batteryDays: document.getElementById("batteryDays"),
  batteryVoltage: document.getElementById("batteryVoltage"),
  batteryCurrent: document.getElementById("batteryCurrent"),
  batteryPower: document.getElementById("batteryPower"),

  // status badges
  sdStatus: document.getElementById("sdStatus"),
  rtcStatus: document.getElementById("rtcStatus"),
  inaStatus: document.getElementById("inaStatus"),
  alarmStatus: document.getElementById("alarmStatus"),
  wifiStatus: document.getElementById("wifiStatus"),
  batteryStatus: document.getElementById("batteryStatus"),

  // alarm
  alarmButton: document.getElementById("alarmButton"),
  ringAlarm: document.getElementById("ring"),
  iconAlarm: document.getElementById("icon-alarm"),
  textAlarm: document.getElementById("alarm-text"),

  // config + history
  configForm: document.getElementById("configForm"),
  historyBody: document.getElementById("historyBody"),
  historyPage: document.getElementById("historyPage"),
  numberPage: document.getElementById("numberPage"),
  btnPrev: document.getElementById("btnPrev"),
  btnNext: document.getElementById("btnNext"),
  historyDate: document.getElementById("historyDate"),

  // update modal OTA
  footerFwVersion: document.getElementById("footerFwVersion"),
  footerWebVersion: document.getElementById("footerWebVersion"),
  modalFwVersion: document.getElementById("modalFwVersion"),
  modalWebVersion: document.getElementById("modalWebVersion"),
  firmwareFile: document.getElementById("firmwareFile"),
  webFile: document.getElementById("webFile"),
  updateProgress: document.getElementById("updateProgress"),
  updateStatus: document.getElementById("updateStatus"),
};

// API helpers
async function api(path, options = {}) {
  const separator = path.includes("?") ? "&" : "?";

  const url = `${API_BASE}${path}${separator}_=${Date.now()}`;

  const res = await fetch(url, {
    credentials: "include",
    cache: "no-store",
    headers: {
      Connection: "close",
    },
    ...options,
  });

  return res;
}

async function apiJson(path, options = {}) {
  try {
    const res = await api(path, options);

    const data = await res.json().catch(() => null);

    if (!res.ok) {
      const msg = data?.error || `Request failed (${res.status})`;

      throw new Error(msg);
    }

    return data;
  } catch (err) {
    // jangan spam console jika hanya timeout fetch
    if (err.name === "AbortError") {
      console.warn(`Request timeout: ${path}`);

      return null;
    }

    throw err;
  }
}

// function showLogin() {
//   // for WebView and browser, redirect to login
//   window.location.href = `${API_BASE}/login.html`;
// }

// showToast
function showToast(msg, type = "info") {
  const t = document.createElement("div");
  t.className = `toast toast-${type}`;
  t.style =
    "position:fixed;left:50%;top:20px;transform:translateX(-50%);background:#111;color:#fff;padding:12px 16px;border-radius:8px;z-index:9999;opacity:0.95;font-size:30px;";
  t.innerText = msg;
  document.body.appendChild(t);
  setTimeout(() => t.remove(), 4000);
}

// Auth
// async function checkAuth() {
//   try {
//     await apiJson(`/api/check`);
//     document.body.classList.add("auth-ok");
//     startDashboard();
//   } catch (e) {
//     showLogin();
//   }
// }

// async function logoutSession() {
//   if (!confirm("Keluar dari dashboard?")) return;
//   try {
//     await apiJson(`/api/logout`);
//     showLogin();
//   } catch (e) {
//     console.error(e);
//     showToast("Logout gagal", "error");
//   }
// }

// LOAD STORAGE
async function loadStorage() {
  try {
    const res = await apiJson("/api/storage");

    el.freeSDCard.textContent = `${Number(res.free_sd_mb || 0).toFixed(0)}`;
    el.usedSDCard.textContent = `${Number(res.used_sd_mb || 0).toFixed(0)}`;
    el.logQueue.textContent = res.log_queue || "0";
    el.sensorQueue.textContent = res.sensor_queue || "0";
    el.csvRowsToday.textContent = res.csv_rows_today || "0";
    el.retentionDayStat.textContent = res.retention_days || "0";
  } catch (err) {
    console.error(err);
  }
}

// LOAD VERSIONS
async function loadVersion() {
  try {
    const res = await apiJson("/api/node/device");

    el.footerFwVersion.textContent = res.firmware || "-";
    // el.footerWebVersion.textContent = res.web_version || "-";
    el.modalFwVersion.textContent = res.firmware || "-";
    // el.modalWebVersion.textContent = res.web_version || "-";
  } catch (err) {
    console.error(err);
  }
}

// UPLOAD FIRMWARE
async function uploadFirmware() {
  const firmwareFile = document.getElementById("firmwareFile").files[0];

  const manifestFile = document.getElementById("manifestFile").files[0];

  if (!firmwareFile) {
    showToast("Pilih firmware.bin", "error");

    return;
  }

  if (!manifestFile) {
    showToast("Pilih manifest.json", "error");

    return;
  }

  try {
    const manifest = await readManifest(manifestFile);

    const check = await apiJson("/api/update/check", {
      method: "POST",

      headers: {
        "Content-Type": "application/json",
      },

      body: JSON.stringify({
        firmware_version: manifest.firmware_version,

        // web_version: manifest.web_version,
      }),
    });

    if (check && check.firmware_same) {
      showToast("Versi firmware sama, gagal update", "warning");

      return;
    }
    if (!check.firmware_newer) {
      showToast("Versi firmware lebih lama, gagal update", "error");

      return;
    }

    showToast("Memeriksa Firmware...", "info");

    const fd = new FormData();

    fd.append("firmware", firmwareFile);

    window.otaUploading = true;

    const res = await fetch("/api/update/firmware", {
      method: "POST",
      body: fd,
    });

    if (!res.ok) {
      throw new Error("Upload failed");
    }

    el.updateProgress.textContent = "100%";

    el.updateStatus.textContent = "Success";

    showToast(
      "Firmware berhasil diupload. Restart dalam 3 detik...",
      "success",
    );
  } catch (err) {
    console.error(err);

    showToast("Firmware upload failed", "error");
  } finally {
    setTimeout(() => {
      window.otaUploading = false;
      location.reload();
    }, 10000);
  }
}

// async function uploadFirmware() {
//   const file = document.getElementById("firmwareFile").files[0];

//   if (!file) {
//     showToast("Pilih firmware.bin", "error");

//     return;
//   }

//   const fd = new FormData();
//   fd.append("firmware", file);

//   window.otaUploading = true;

//   try {
//     const res = await fetch("/api/update/firmware", {
//       method: "POST",
//       body: fd,
//     });

//     if (!res.ok) {
//       throw new Error("Upload failed");
//     }
//     el.updateProgress.textContent = "100%";
//     el.updateStatus.textContent = "Success";

//     showToast(
//       "Firmware berhasil diupload. Restart dalam 3 detik...",
//       "success",
//     );
//   } catch (err) {
//     console.error(err);

//     showToast("Firmware upload failed", "error");
//   } finally {
//     setTimeout(() => {
//       window.otaUploading = false;
//       location.reload();
//     }, 10000);
//   }
// }

// UPLOAD WEB DASHBOARD

async function uploadWeb() {
  const file = document.getElementById("webFile").files[0];

  if (!file) {
    showToast("Pilih www.zip", "error");

    return;
  }

  const fd = new FormData();

  fd.append("web", file);

  try {
    const res = await fetch("/api/update/web", {
      method: "POST",
      body: fd,
    });

    if (!res.ok) {
      throw new Error("Upload failed");
    }

    showToast("Dashboard berhasil diupload", "success");
  } catch (err) {
    console.error(err);

    showToast("Upload dashboard failed", "error");
  }
}

// UPDATE STATUS UPLOAD PROGRESS
async function updateStatusLoop() {
  if (window.otaUploading) {
    return;
  }

  try {
    const res = await apiJson("/api/update/status");

    el.updateProgress.textContent = `${res.progress || 0}%`;

    el.updateStatus.textContent = res.status || "-";
  } catch (err) {
    console.error(err);
  }
}

// CEK VERSION OTA FILES DENGAN MANIFEST.JSON
async function readManifest(file) {
  const text = await file.text();

  return JSON.parse(text);
}

// sync RTC Button Handler
async function syncRTC() {
  try {
    const epoch = Math.floor(Date.now() / 1000);

    const res = await apiJson("/api/rtc/sync", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        epoch,
      }),
    });

    showToast("RTC berhasil disinkronkan", "success");

    await loadRTC();
  } catch (err) {
    console.error(err);

    showToast("Gagal sinkronisasi RTC", "error");
  }
}

// load RTC
async function loadRTC() {
  try {
    const rtc = await apiJson("/api/rtc");

    if (!rtc) {
      showToast("Gagal memuat status RTC", "error");
      return;
    }

    // =====================
    // RTC NODE
    // =====================

    if (el.rtcTime) {
      el.rtcTime.innerText = rtc.timestamp || "--";
    }

    // =====================
    // DEVICE TIME
    // =====================

    const now = new Date();

    const deviceEpoch = Math.floor(now.getTime() / 1000);

    const deviceTime =
      now.getFullYear() +
      "-" +
      String(now.getMonth() + 1).padStart(2, "0") +
      "-" +
      String(now.getDate()).padStart(2, "0") +
      " " +
      String(now.getHours()).padStart(2, "0") +
      ":" +
      String(now.getMinutes()).padStart(2, "0") +
      ":" +
      String(now.getSeconds()).padStart(2, "0");

    if (el.deviceTime) {
      el.deviceTime.innerText = deviceTime;
    }

    // =====================
    // LOST POWER
    // =====================

    if (el.rtcLostPower) {
      el.rtcLostPower.innerText = rtc.lost_power ? "Ya" : "Tidak";
    }

    // =====================
    // DRIFT
    // =====================

    const rtcEpoch = Number(rtc.epoch || 0);

    const drift = Math.abs(deviceEpoch - rtcEpoch);

    if (el.rtcDrift) {
      if (rtcEpoch === 0) {
        el.rtcDrift.innerText = "--";
      } else {
        el.rtcDrift.innerText = `${drift} detik`;
      }
    }

    // =====================
    // STATUS + BUTTON
    // =====================

    let status = "Belum Sinkron";

    let disableButton = false;

    if (rtc.lost_power) {
      status = "RTC Lost Power";

      disableButton = false;
      el.syncRtcBtn.style.display = "block";
    } else if (!rtc.ever_synced) {
      status = "Belum Sinkron";

      disableButton = false;
      el.syncRtcBtn.style.display = "block";
    } else if (drift <= 60) {
      status = "Sinkron";

      disableButton = true;
      el.syncRtcBtn.style.display = "none";
    } else {
      status = `Perlu Sinkronisasi`;

      disableButton = false;
      el.syncRtcBtn.style.display = "block";
    }

    if (el.rtcSyncStatus) {
      el.rtcSyncStatus.innerText = status;
    }

    if (el.syncRtcBtn) {
      el.syncRtcBtn.disabled = disableButton;
    }
  } catch (err) {
    console.error(err);
  }
}

// DEVELOPMENT ONLY: Clear RTC data
async function clearRTC() {
  await apiJson("/api/rtc/clear", {
    method: "POST",
  });

  showToast("RTC state cleared", "info");

  await loadRTC();
}

// Start / Dashboard
let dashboardInterval = null;
let dashboardBusy = false;

async function startDashboard() {
  // hentikan polling lama
  if (dashboardInterval) {
    clearInterval(dashboardInterval);

    dashboardInterval = null;

    console.log("Dashboard interval restarted");
  }

  console.log("Dashboard started");

  try {
    // load data awal
    await loadVersion();
    await loadLatest();
    await loadEstrus();
    await loadConfig();
    await loadRTC();
    await loadStorage();
  } catch (err) {
    console.error(err);
  }

  dashboardInterval = setInterval(async () => {
    if (dashboardBusy) {
      return;
    }

    dashboardBusy = true;

    try {
      // load data terbaru setiap 10 detik
      console.log("Dashboard polling");

      await loadLatest();
      await loadEstrus();
      await loadRTC();
      await loadStorage();
    } catch (err) {
      if (err?.name !== "AbortError") {
        console.error(err);
      }
    } finally {
      dashboardBusy = false;
    }
  }, 10000);
}

// LATEST
async function loadLatest() {
  try {
    const data = await apiJson(`/api/node/latest`);
    latestData = data;
    renderLatest(data);
  } catch (e) {
    console.error(e);
  }
}

function renderLatest(data) {
  if (!data) return;

  // rtc/time
  const rtcEl = document.getElementById("rtcTimeHeader");
  if (rtcEl) rtcEl.innerText = data.time || "Gagal memuat waktu";

  // sensors
  if (el.sensor1) el.sensor1.innerText = data.sensor1 ? "AKTIF" : "OFF";
  if (el.sensor2) el.sensor2.innerText = data.sensor2 ? "AKTIF" : "OFF";

  if (el.sensor1Condition) {
    const s1Dirty = data.sensor1_dirty;
    const s1NoActivity = data.sensor1_no_activity;
    el.sensor1Condition.innerText = s1Dirty ? "KOTOR" : "NORMAL";
    el.sensor1Condition.innerText = s1NoActivity ? "⚠️ NO ACTIVITY" : "NORMAL";
  }

  if (el.sensor2Condition) {
    const s2Dirty = data.sensor2_dirty;
    const s2NoActivity = data.sensor2_no_activity;
    el.sensor2Condition.innerText = s2Dirty ? "KOTOR" : "NORMAL";
    el.sensor2Condition.innerText = s2NoActivity ? "⚠️ NO ACTIVITY" : "NORMAL";
  }

  if (el.cowCondition) {
    const isStanding = data.sensor1 || data.sensor2;
    const s1NoActivity = data.sensor1_no_activity;
    const s2NoActivity = data.sensor2_no_activity;

    if (isStanding && (!s1NoActivity || !s2NoActivity)) {
      el.cowCondition.innerText = "BERDIRI";
      el.cowCondition.className = "sapi-berdiri";
    } else if (!isStanding && (!s1NoActivity || !s2NoActivity)) {
      el.cowCondition.innerText = "REBAH";
      el.cowCondition.className = "sapi-rebah";
    } else {
      el.cowCondition.innerText = "⚠️ Sapi Sakit/Keluar Kandang/Mati";
      el.cowCondition.className = "sapi-rebah";
    }
  }

  // battery
  if (el.batteryPercent)
    el.batteryPercent.innerText = `${Number(data.battery_percent || 0).toFixed(0)} %`;
  if (el.batteryDays)
    el.batteryDays.innerText = `${Number(data.battery_days || 0)} Hari`;
  if (el.batteryVoltage)
    el.batteryVoltage.innerText = `${Number(data.voltage || 0).toFixed(2)} V`;
  if (el.batteryCurrent)
    el.batteryCurrent.innerText = `${Number(data.current || 0).toFixed(2)} mA`;
  if (el.batteryPower)
    el.batteryPower.innerText = `${Number(data.power || 0).toFixed(2)} mW`;

  // status badges
  updateBadge(el.sdStatus, data.sd);
  updateBadge(el.rtcStatus, data.rtc);
  updateBadge(el.inaStatus, data.ina);
  updateBadge(el.wifiStatus, data.wifi);
  updateBadge(el.alarmStatus, data.alarm);
  updateBadge(el.batteryStatus, !data.low_battery);

  // alarm UI
  if (el.alarmButton) {
    if (data.alarm) {
      el.alarmButton.className = "alarm-btn alarm-active";
      el.alarmButton.disabled = false;
      if (el.ringAlarm) el.ringAlarm.style.display = "block";
    } else {
      el.alarmButton.className = "alarm-btn alarm-inactive";
      el.alarmButton.disabled = true;
      if (el.ringAlarm) el.ringAlarm.style.display = "none";
    }
  }
}

// ESTRUS
async function loadEstrus() {
  try {
    const e = await apiJson(`/api/node/estrus`);
    console.log("load estrus: ", e);
    if (!e) return;

    if (el.partitionHours) el.partitionHours.innerText = `${e.partition}`;
    if (el.currentRate)
      el.currentRate.innerText = `${Number(e.current_rate ?? 0).toFixed(2)} %`;
    if (el.baselineRate)
      el.baselineRate.innerText = `${Number(e.baseline_rate ?? 0).toFixed(2)} %`;
    if (el.deviationPct)
      el.deviationPct.innerText = `${Number(e.deviation_pct ?? 0).toFixed(2)} %`;
    if (el.estrusThreshold)
      el.estrusThreshold.innerText = `${Number(e.threshold_pct ?? 0).toFixed(2)} %`;
    if (el.baselineWindow)
      el.baselineWindow.innerText = `${e.baseline_windows}`;
    if (el.windowCount) el.windowCount.innerText = `${e.window_count}`;
    if (el.windowSize) el.windowSize.innerText = `${e.window_size}`;

    if (el.injectionDate) el.injectionDate.innerText = `${e.injection_date}`;
    if (el.cycleDay) el.cycleDay.innerText = `${e.cycle_day}`;

    if (el.estrusWindow)
      el.estrusWindow.innerText = e.is_estrus_window ? "1" : "0";
    if (el.baselineValid)
      el.baselineValid.innerText = e.valid ? "Valid" : "Belum Valid";
    if (el.estrusStatus) {
      el.estrusStatus.innerText = e.estrus ? "BIRAHI" : "NORMAL";
      el.estrusStatus.className = e.estrus ? "status-estrus" : "status-normal";
    }
  } catch (err) {
    console.error(err);
  }
}

// CONFIG
async function loadConfig() {
  try {
    const cfg = await apiJson(`/api/config`);
    configCache = cfg || {};
    fillConfigForm(cfg || {});
  } catch (e) {
    console.error(e);
  }
}

function fillConfigForm(cfg) {
  setValue("nodeId", cfg.node_id);
  if (el.nodeTitle)
    el.nodeTitle.innerText = cfg.node_id || el.nodeTitle.innerText;

  setValue("animalId", cfg.animal_id);
  if (el.animalIdLabel)
    el.animalIdLabel.innerText = cfg.animal_id || el.animalIdLabel.innerText;

  setValue("apPassword", cfg.ap_password);
  setValue("proxLow", cfg.prox_low ? 1 : 0);
  setValue("alarmEnabled", cfg.alarm_enabled ? 1 : 0);
  if (el.ledBrightness) el.ledBrightness.value = cfg.led_brightness;
  if (el.ledBrightnessValue)
    el.ledBrightnessValue.textContent = cfg.led_brightness;

  setValue("recordCfg", cfg.record_interval_sec);
  setValue("retentionCfg", cfg.retention_days);
  setValue("partitionCfg", cfg.partition_hours);
  setValue("estrusCfg", cfg.estrus_threshold_pct);
  setValue("stopAfterAlarm", cfg.stop_after_alarm ? 1 : 0);
  setValue("baselineCfg", cfg.min_baseline_windows);
  setValue("dirtyCfg", cfg.dirty_timeout_min);
  setValue("noActivityCfg", cfg.no_activity_timeout_hours);
  setValue("injectionDateCfg", cfg.injection_date);

  setValue("currentThreshold", cfg.current_threshold);
  setValue("powerThreshold", cfg.power_threshold);
}

function setValue(id, value) {
  const e = document.getElementById(id);
  if (!e) return;
  if (value === undefined || value === null) {
    e.value = "";
    return;
  }
  e.value = value;
}

// VALIDATION
function showError(elm, msg) {
  elm.classList.add("invalid");
  let err = elm.parentElement.querySelector(".error-msg");
  if (!err) {
    err = document.createElement("div");
    err.className = "error-msg";
    elm.parentElement.appendChild(err);
  }
  err.innerText = msg;
}
function clearError(elm) {
  elm.classList.remove("invalid");
  const err = elm.parentElement.querySelector(".error-msg");
  if (err) err.remove();
}

function validateNodeId() {
  const elm = document.getElementById("nodeId");
  if (!elm) return true;
  const v = elm.value.trim();
  const regex = /^[a-zA-Z0-9-]{3,16}$/;
  if (!regex.test(v)) {
    showError(elm, "Invalid Node ID");
    return false;
  }
  clearError(elm);
  return true;
}
function validateAnimalId() {
  const elm = document.getElementById("animalId");
  if (!elm) return true;
  const v = elm.value.trim();
  const regex = /^[a-zA-Z0-9-]{3,16}$/;
  if (!regex.test(v)) {
    showError(elm, "Invalid Animal ID");
    return false;
  }
  clearError(elm);
  return true;
}
function validatePasswordAP() {
  const elm = document.getElementById("apPassword");
  if (!elm) return true;
  const v = elm.value.trim();
  if (v.length === 0) return true;
  const regex = /^[a-zA-Z0-9-]{8,20}$/;
  if (!regex.test(v)) {
    showError(elm, "8-20 chars");
    return false;
  }
  clearError(elm);
  return true;
}

function validateRange(id, min, max) {
  const elm = document.getElementById(id);
  if (!elm) return true;
  const val = parseFloat(elm.value);
  if (isNaN(val)) {
    showError(elm, "Invalid number");
    return false;
  }
  if (val < min || val > max) {
    showError(elm, `Range ${min}-${max}`);
    return false;
  }
  clearError(elm);
  return true;
}

function validateConfig() {
  let valid = true;

  if (!validateNodeId()) valid = false;
  if (!validateAnimalId()) valid = false;
  if (!validatePasswordAP()) valid = false;

  if (!validateRange(el.ledBrightness.value, 1, 255)) valid = false;
  if (!validateRange("recordCfg", 10, 3600)) valid = false;
  if (!validateRange("retentionCfg", 1, 14)) valid = false;
  if (!validateRange("partitionCfg", 3, 24)) valid = false;
  if (!validateRange("estrusCfg", 0.1, 100)) valid = false;
  if (!validateRange("baselineCfg", 2, 48)) valid = false;
  if (!validateRange("dirtyCfg", 10, 480)) valid = false;
  if (!validateRange("noActivityCfg", 1, 24)) valid = false;
  if (!validateRange("currentThreshold", 100, 150)) valid = false;
  if (!validateRange("powerThreshold", 400, 600)) valid = false;

  return valid;
}

// SAVE CONFIG
async function saveConfig() {
  const btnSave = document.getElementById("saveConfigBtn");

  if (!validateConfig()) {
    showToast("Invalid configuration", "error");

    return;
  }

  try {
    btnSave.disabled = true;

    const nodeId = (document.getElementById("nodeId")?.value || "").trim();

    const animalId = (document.getElementById("animalId")?.value || "").trim();

    const payload = {
      // DEVICE
      node_id: nodeId,
      animal_id: animalId,
      ap_password: document.getElementById("apPassword")?.value || "estrus123",
      prox_active_low: Number(document.getElementById("proxLow")?.value),
      alarm_enabled: Number(document.getElementById("alarmEnabled")?.value),
      led_brightness: Number(el.ledBrightness.value),

      // ESTRUS
      record_interval_sec: Number(document.getElementById("recordCfg")?.value),

      retention_days: Number(document.getElementById("retentionCfg")?.value),

      partition_hours: Number(document.getElementById("partitionCfg")?.value),

      estrus_threshold_pct: Number(document.getElementById("estrusCfg")?.value),

      stop_after_alarm: Number(
        document.getElementById("stopAfterAlarm")?.value,
      ),

      min_baseline_windows: Number(
        document.getElementById("baselineCfg")?.value,
      ),

      dirty_timeout_min: Number(document.getElementById("dirtyCfg")?.value),

      no_activity_timeout_hours: Number(
        document.getElementById("noActivityCfg")?.value,
      ),

      injection_date: document.getElementById("injectionDateCfg")?.value,

      // BATTERY
      current_threshold: Number(
        document.getElementById("currentThreshold")?.value,
      ),

      power_threshold: Number(document.getElementById("powerThreshold")?.value),
    };

    console.log("Saving config:", payload);

    const res = await apiJson("/api/config", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(payload),
    });

    if (res?.success) {
      showToast("Konfigurasi tersimpan", "success");

      if (res.restart) {
        showToast("Perangkat akan restart karena perubahan ID");
      }

      await loadConfig();

      return;
    }

    showToast("Gagal menyimpan konfigurasi", "error");
  } catch (err) {
    console.error(err);

    showToast("Gagal menyimpan konfigurasi", "error");
  } finally {
    btnSave.disabled = false;
  }
}

// ALARM STATUS
async function statusAlarm() {
  try {
    const status = await apiJson("/api/alarm/status");
    return status;
  } catch (e) {
    console.error("Failed to load alarm status", e);
    return null;
  }
}

// ALARM STOP
async function stopAlarm() {
  try {
    await apiJson("/api/alarm/stop", { method: "POST" });
    showToast("Alarm dihentikan", "success");
    await loadLatest();
  } catch (e) {
    console.error(e);
    showToast("Gagal menghentikan alarm", "error");
  }
}

// ALARM START
async function startAlarm() {
  try {
    await apiJson("/api/alarm/start", { method: "POST" });
    showToast("Alarm dihidupkan", "success");
    await loadLatest();
  } catch (e) {
    console.error(e);
    showToast("Gagal menghidupkan alarm", "error");
  }
}

// HISTORY with pagination
async function loadHistory(date, page = 0, limit = 10) {
  console.log("loadHistory called!");

  if (!date) {
    showToast("Pilih tanggal", "error");
    return;
  }
  try {
    const q = `?date=${encodeURIComponent(date)}&page=${page}&limit=${limit}`;
    const data = await apiJson(`/api/node/history${q}`);
    console.log("HISTORY:", data);

    if (!data || !data.rows || data.rows.length === 0) {
      showToast("Tidak ada data untuk tanggal tersebut", "info");
      el.historyBody.innerHTML = "";
      el.historyPage.hidden = true;
      el.numberPage.innerText = "-";
      el.btnPrev.hidden = true;
      el.btnPrev.disabled = true;
      el.btnNext.hidden = true;
      el.btnNext.disabled = true;
      hasNextPage = !!data.has_next;
      return;
    }

    renderHistory(data.rows || []);
    currentPage = Number(data.page ?? page);
    hasNextPage = !!data.has_next;
    el.historyPage.hidden = false;
    el.numberPage.innerText = currentPage + 1;
    el.btnPrev.hidden = currentPage === 0;
    el.btnNext.hidden = !hasNextPage;
  } catch (e) {
    console.error(e);
    showToast("Gagal memuat history", "error");
  }
}

function nextHistory() {
  if (!hasNextPage) return;
  currentPage = (currentPage || 0) + 1;
  const date = document.getElementById("historyDate")?.value;
  loadHistory(date, currentPage);
}

function prevHistory() {
  if ((currentPage || 0) === 0) return;
  currentPage = (currentPage || 0) - 1;
  const date = document.getElementById("historyDate")?.value;
  loadHistory(date, currentPage);
}

function renderHistory(rows) {
  const tbody = document.getElementById("historyBody");

  if (!tbody) return;

  tbody.innerHTML = "";

  rows.forEach((r) => {
    const tr = document.createElement("tr");

    tr.innerHTML = `
      <td>${r.device_id ?? "-"}</td>
      <td>${r.animal_id ?? "-"}</td>
      <td>${r.timestamp ?? "-"}</td>
      <td>${r.sensor1_state ?? 0}</td>
      <td>${r.sensor2_state ?? 0}</td>
      <td>${r.sensor1_dirty ?? 0}</td>
      <td>${r.sensor2_dirty ?? 0}</td>
      <td>${Number(r.deviation ?? 0).toFixed(2)}</td>
      <td>${r.estrus ? "1" : "0"}</td>
      <td>${Number(r.voltage ?? 0).toFixed(2)}</td>
      <td>${Number(r.current ?? 0).toFixed(2)}</td>
      <td>${Number(r.battery_pct ?? 0).toFixed(0)}%</td>
    `;

    tbody.appendChild(tr);
  });
}

// DOWNLOAD
async function downloadCSV() {
  const btn = document.getElementById("downloadCsvBtn");

  try {
    if (btn) btn.disabled = true;

    const res = await fetch("/api/download", {
      method: "GET",
      credentials: "same-origin",
      cache: "no-store",
    });

    if (!res.ok) {
      throw new Error(await res.text());
    }

    const blob = await res.blob();

    let filename = "retention.csv";

    const disposition = res.headers.get("Content-Disposition");

    if (disposition) {
      const match = disposition.match(/filename="?([^"]+)"?/i);

      if (match) {
        filename = match[1];
      }
    }

    const url = URL.createObjectURL(blob);

    const a = document.createElement("a");

    a.href = url;
    a.download = filename;

    document.body.appendChild(a);

    a.click();

    a.remove();

    URL.revokeObjectURL(url);

    showToast("CSV berhasil didownload", "success");
  } catch (err) {
    console.error(err);

    showToast("Download CSV gagal", "error");
  } finally {
    if (btn) btn.disabled = false;
  }
}

// Helpers
function bindEvents() {
  if (el.ledBrightness && el.ledBrightnessValue) {
    el.ledBrightness.addEventListener("input", () => {
      el.ledBrightnessValue.textContent = el.ledBrightness.value;
    });
  }

  // event lain nanti di sini...
}

function updateBadge(element, ok) {
  if (!element) return;
  element.className = ok ? "dot dot-active" : "dot dot-error";
}

function showRestartWarn() {
  const elw = document.getElementById("restart-warn");
  if (elw) elw.style.display = "block";
}

// Auto validation on inputs
document
  .querySelectorAll("input,select")
  .forEach((i) => i.addEventListener("input", validateConfig));

// Start
document.addEventListener("DOMContentLoaded", () => {
  // checkAuth();
  // panggil event dulu
  bindEvents();
  // lalu start dashboard
  startDashboard();
});
