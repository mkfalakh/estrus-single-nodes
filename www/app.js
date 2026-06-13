// =============================================
// GLOBAL STATE
// =============================================
let latestData = null;
let configCache = null;

// =============================================
// ELEMENTS
// =============================================
const el = {
  nodeTitle: document.getElementById("nodeTitle"),

  estrusStatus: document.getElementById("estrusStatus"),
  estrusScore: document.getElementById("estrusScore"),

  batteryPercent: document.getElementById("batteryPercent"),
  batteryVoltage: document.getElementById("batteryVoltage"),
  batteryCurrent: document.getElementById("batteryCurrent"),
  batteryPower: document.getElementById("batteryPower"),
  batteryDays: document.getElementById("batteryDays"),
  batteryDate: document.getElementById("batteryDate"),

  act1: document.getElementById("act1"),
  act2: document.getElementById("act2"),
  totalAct: document.getElementById("totalAct"),

  rtcTime: document.getElementById("rtcTime"),

  sdStatus: document.getElementById("sdStatus"),
  rtcStatus: document.getElementById("rtcStatus"),
  sensorStatus: document.getElementById("sensorStatus"),
  alarmStatus: document.getElementById("alarmStatus"),

  alarmButton: document.getElementById("alarmButton"),
  ringAlarm: document.getElementById("ring"),
  iconAlarm: document.getElementById("icon-alarm"),
  textAlarm: document.getElementById("alarm-text"),

  configForm: document.getElementById("configForm"),
  historyBody: document.getElementById("historyBody"),
};

// =============================================
// API
// =============================================
async function api(url, options = {}) {
  const res = await fetch(url, {
    credentials: "include",
    ...options,
  });

  const text = await res.text();

  console.log("RAW RESPONSE:", text);

  let data = {};

  try {
    data = JSON.parse(text);
  } catch (e) {
    console.error("JSON PARSE ERROR:", e);

    console.error("INVALID JSON:", text);
  }

  if (!res.ok) {
    throw new Error(data.error || "Request failed");
  }

  return data;
}

// =============================================
// LOGIN CHECK
// =============================================
async function checkAuth() {
  try {
    await api("/api/check");

    document.body.classList.add("auth-ok");

    loadDashboard();
  } catch (e) {
    window.location.href = "/login.html";
  }
}

// =============================================
// LOGOUT
// =============================================
async function logoutSession() {
  if (!confirm("Keluar dari dashboard?")) return;

  try {
    await api("/api/logout", {
      credentials: "include",
    });
    window.location.href = "/login.html";
  } catch (e) {
    console.error(e);
  }
}

// =============================================
// LOAD DASHBOARD
// =============================================
async function loadDashboard() {
  await Promise.all([loadLatest(), loadConfig(), loadBuzzerStatus()]);

  setInterval(loadLatest, 3000);
  setInterval(loadBuzzerStatus, 2000);
}

// =============================================
// LOAD LATEST
// =============================================
async function loadLatest() {
  try {
    const data = await api("/api/node/latest");

    latestData = data;

    renderLatest(data);
  } catch (e) {
    console.error(e);
  }
}

// =============================================
// RENDER DASHBOARD
// =============================================
function renderLatest(data) {
  if (!data) return;

  // =========================================
  // NODE
  // =========================================
  if (el.nodeTitle) el.nodeTitle.innerText = data.node_id || "UNKNOWN NODE";

  document.title = data.node_id || "Estrus Monitor";

  // =========================================
  // RTC
  // =========================================
  if (el.rtcTime) el.rtcTime.innerText = data.time || "Gagal memuat waktu";

  // =========================================
  // ESTRUS
  // =========================================
  if (el.estrusStatus) {
    el.estrusStatus.innerText = data.estrus ? "ESTRUS" : "NORMAL";

    el.estrusStatus.className = data.estrus ? "status-estrus" : "status-normal";
  }

  if (el.estrusScore)
    el.estrusScore.innerText = Number(data.score || 0).toFixed(2);

  // =========================================
  // ACTIVITY
  // =========================================
  if (el.act1) el.act1.innerText = data.a1 ?? 0;

  if (el.act2) el.act2.innerText = data.a2 ?? 0;

  if (el.totalAct) el.totalAct.innerText = data.total ?? 0;

  // =========================================
  // BATTERY
  // =========================================
  if (el.batteryPercent)
    el.batteryPercent.innerText = `${Number(data.battery_percent || 0)} %`;

  if (el.batteryVoltage)
    el.batteryVoltage.innerText = `${Number(data.voltage || 0).toFixed(1)} V`;

  if (el.batteryCurrent)
    el.batteryCurrent.innerText = `${Number(data.current || 0).toFixed(1)} mA`;

  if (el.batteryPower)
    el.batteryPower.innerText = `${Number(data.power || 0).toFixed(1)} mW`;

  if (el.batteryDays)
    el.batteryDays.innerText = `${Number(data.battery_days || 0)}`;

  if (el.batteryDate) el.batteryDate.innerText = data.battery_date || "-";

  // =========================================
  // SYSTEM STATUS
  // =========================================
  updateBadge(el.sdStatus, data.sd);
  updateBadge(el.rtcStatus, data.rtc);
  updateBadge(el.sensorStatus, data.sensor);
}

// =============================================
// BADGE
// =============================================
function updateBadge(element, ok) {
  if (!element) return;

  // element.innerText = ok ? "OK" : "ERROR";

  element.className = ok ? "dot dot-active" : "dot dot-error";
}

// =============================================
// LOAD CONFIG
// =============================================
async function loadConfig() {
  try {
    const cfg = await api("/api/config/get");

    configCache = cfg;

    fillConfigForm(cfg);
  } catch (e) {
    console.error(e);
  }
}

// =============================================
// FILL CONFIG
// =============================================
function fillConfigForm(cfg) {
  setValue("nodeId", cfg.node_id);

  setValue("interval", cfg.interval);
  setValue("proxLow", cfg.prox_low ? 1 : 0);

  setValue("buzzer_enabled", cfg.buzzer_enabled ? 1 : 0);
  if (cfg.buzzer_enabled) {
    el.alarmStatus.className = "dot dot-active";
  } else {
    el.alarmStatus.className = "dot dot-inactive";
  }

  setValue("score", cfg.score);
  setValue("ratioTrigger", cfg.ratio_trigger);
  setValue("persist", cfg.persist);
  setValue("ema", cfg.ema);
  setValue("activityMin", cfg.activity_min);
  setValue("balanceMin", cfg.balance_min);

  console.log("CONFIG:", cfg);
}

function setValue(id, value) {
  const e = document.getElementById(id);

  if (!e) return;

  // prevent undefined/null
  if (value === undefined || value === null) {
    if (e.type === "number") {
      e.value = 0;
    } else {
      e.value = "";
    }

    return;
  }

  e.value = value;
}

// function setChecked(id, value) {
//   const e = document.getElementById(id);

//   if (e) e.checked = !!value;
// }

// =============================================
// VALIDATION
// =============================================
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

  const value = elm.value.trim();

  const regex = /^[A-Z0-9-]{6,8}$/;

  if (!regex.test(value)) {
    showError(elm, "⚠️ 6-8 chars, A-Z 0-9 - only. cth: NODE-01");

    return false;
  }

  clearError(elm);

  return true;
}

function validateRange(id, min, max) {
  const elm = document.getElementById(id);

  const value = parseFloat(elm.value);

  if (isNaN(value)) {
    showError(elm, "Invalid number");

    return false;
  }

  if (value < min || value > max) {
    showError(elm, `⚠️ Range ${min} - ${max}`);

    return false;
  }

  clearError(elm);

  return true;
}

function validateConfig() {
  let ok = true;

  ok &= validateNodeId();

  ok &= validateRange("interval", 1, 24);
  ok &= validateRange("score", 0.1, 5);
  ok &= validateRange("ratioTrigger", 0.1, 10);
  ok &= validateRange("persist", 1, 20);
  ok &= validateRange("ema", 0.01, 1);
  ok &= validateRange("activityMin", 1, 10000);
  ok &= validateRange("balanceMin", 0.01, 1);

  return !!ok;
}

// =============================================
// SAVE CONFIG
// =============================================
async function saveConfig() {
  if (!validateConfig()) {
    alert("Invalid configuration");

    return;
  }

  try {
    const params = new URLSearchParams();

    // =========================
    // NODE ID
    // =========================
    const nodeId = getValue("nodeId").trim();

    if (nodeId !== configCache.node_id) {
      params.append("node_id", nodeId);
    }

    // =========================
    // BASIC
    // =========================
    params.append("interval", getValue("interval"));

    params.append("buzzer_enabled", getValue("buzzer_enabled"));

    params.append("prox_low", getValue("proxLow"));

    // =========================
    // MODEL
    // =========================
    params.append("score", getValue("score"));

    params.append("ratio_trigger", getValue("ratioTrigger"));

    params.append("persist", getValue("persist"));

    params.append("ema", getValue("ema"));

    params.append("activity_min", getValue("activityMin"));

    params.append("balance_min", getValue("balanceMin"));

    // =========================
    // REQUEST
    // =========================
    const res = await fetch(`/api/config/set?${params}`, {
      credentials: "include",
    });

    const data = await res.json();

    if (!res.ok) {
      alert(data.error || "Gagal menyimpan konfigurasi");

      return;
    }

    // =========================
    // UPDATE CACHE
    // =========================
    configCache = {
      ...configCache,

      node_id: nodeId,

      interval: getValue("interval"),

      buzzer_enabled: getValue("buzzer_enabled"),

      prox_low: getValue("proxLow"),

      score: getValue("score"),

      ratio_trigger: getValue("ratioTrigger"),

      persist: getValue("persist"),

      ema: getValue("ema"),

      activity_min: getValue("activityMin"),

      balance_min: getValue("balanceMin"),
    };

    // =========================
    // RESTART INFO
    // =========================
    if (data.restart) {
      alert("Node ID berubah. Device restarting...");

      return;
    }

    alert("Berhasil menyimpan konfigurasi!");

    window.location.reload();
  } catch (e) {
    console.error(e);

    alert("Gagal menyimpan konfigurasi");
  }
}

// =============================================
// RESET CONFIG
// =============================================
async function resetConfig() {
  if (!confirm("Reset konfigurasi?")) return;

  try {
    await api("/api/config/reset");

    alert("Reset berhasil!");

    window.location.reload();
  } catch (e) {
    alert("Gagal mereset konfigurasi");
  }
}

// =============================================
// BUZZER STATUS BUTTON
// =============================================
async function loadBuzzerStatus() {
  try {
    const data = await api("/api/status/buzzer");
    if (!el.alarmButton) return;

    if (data.buzzer) {
      el.alarmButton.className = "alarm-btn alarm-active";
      el.alarmButton.disabled = false;
      el.ringAlarm.style.display = "block";
      el.textAlarm.innerText = "Status: Berbunyi";
      el.iconAlarm.innerHTML =
        '<path d="M18 8A6 6 0 0 0 6 8c0 7-3 9-3 9h18s-3-2-3-9"></path><path d="M13.73 21a2 2 0 0 1-3.46 0"></path>';
    } else {
      el.alarmButton.className = "alarm-btn alarm-inactive";
      el.alarmButton.disabled = true;
      el.ringAlarm.style.display = "none";
      el.textAlarm.innerText = "Status: Dimatikan";
      el.iconAlarm.innerHTML =
        '<path d="M13.73 21a2 2 0 0 1-3.46 0"></path><path d="M18.63 13A17.89 17.89 0 0 1 18 8"></path><path d="M6.26 6.26A5.86 5.86 0 0 0 6 8c0 7-3 9-3 9h14"></path><path d="M18 8a6 6 0 0 0-9.33-5"></path><line x1="1" y1="1" x2="23" y2="23"></line>';
    }
  } catch (e) {
    console.error(e);
  }
}

// =============================================
// STOP BUZZER
// =============================================
async function stopBuzzer() {
  try {
    await api("/api/buzzer/stop");

    loadBuzzerStatus();
  } catch (e) {
    console.error(e);
  }
}

// =============================================
// HISTORY
// =============================================
async function loadHistory(date) {
  try {
    const data = await api(`/api/node/history?date=${date}`);

    if (!data.rows || data.rows.length === 0) {
      alert("Tidak ada data untuk tanggal tersebut");
      return;
    }

    renderHistory(data.rows || []);
  } catch (e) {
    console.error(e);
  }
}

// =============================================
// RENDER HISTORY
// =============================================
function renderHistory(rows) {
  const tbody = document.getElementById("historyBody");

  if (!tbody) return;

  tbody.innerHTML = "";

  rows.forEach((row) => {
    const tr = document.createElement("tr");

    tr.innerHTML = `
      <td>${row.device_id ?? "-"}</td>
      <td>${row.animal_id ?? "-"}</td>
      <td>${row.timestamp ?? "-"}</td>
      <td>${row.sensor1_state ?? 0}</td>
      <td>${row.sensor2_state ?? 0}</td>
      <td>${row.deviation ?? 0}</td>
      <td>${row.estrus ?? 0}</td>
    `;

    tbody.appendChild(tr);
  });
}

// =============================================
// DOWNLOAD CSV
// =============================================
function downloadCSV(date) {
  window.location.href = `/api/download?date=${date}`;
}

// =============================================
// HELPERS
// =============================================
function getValue(id) {
  return document.getElementById(id)?.value || "";
}

// function getChecked(id) {
//   return document.getElementById(id)?.checked || false;
// }

function showRestartWarn() {
  document.getElementById("restart-warn").style.display = "block";
}

// =============================================
// AUTO VALIDATION
// =============================================
document.querySelectorAll("input").forEach((elm) => {
  elm.addEventListener("input", validateConfig);
});

// =============================================
// START
// =============================================
checkAuth(); // check login and load dashboard
