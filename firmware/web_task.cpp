/*
 * EdgeGaze — web_task.cpp
 * Connects to WiFi, serves the dashboard HTML on port 80, and streams
 * live inference results to the browser via Server-Sent Events (SSE).
 *
 * Endpoints:
 *   GET /          → full dashboard HTML page
 *   GET /events    → SSE stream  (text/event-stream)
 *   GET /latest    → JSON snapshot (for polling fallback)
 *   GET /status    → JSON system info (heap, uptime, etc.)
 */

#include "config.h"
#include "web_task.h"
#include <WiFi.h>
#include <WebServer.h>

static WebServer server(WEB_PORT);

// Latest result for /latest endpoint
static InferenceResult g_latest_result = {};
static bool            g_has_result    = false;

// SSE client list — simple single-client model (browser reconnects automatically)
static WiFiClient sse_client;
static bool       sse_active = false;

// ── Embedded dashboard HTML ──────────────────────────────────────────────────
// Stored in flash (PROGMEM) to save DRAM.
// The actual HTML is in web_dashboard/index.html — copy it here as a raw string.
// For development, serve from SPIFFS; for simplicity we embed it.

static const char DASHBOARD_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>EdgeGaze · Live Emotion</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&family=Syne:wght@400;700;800&display=swap');

  :root {
    --bg: #0a0a0f;
    --panel: #12121a;
    --border: #1e1e2e;
    --happy:   #f9e04b;
    --angry:   #f05c5c;
    --sad:     #6ab0f5;
    --neutral: #a0a0b8;
    --text:    #e0e0f0;
    --muted:   #555570;
    --glow-happy:   rgba(249,224,75,0.18);
    --glow-angry:   rgba(240,92,92,0.18);
    --glow-sad:     rgba(106,176,245,0.18);
    --glow-neutral: rgba(160,160,184,0.10);
  }

  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

  body {
    font-family: 'Syne', sans-serif;
    background: var(--bg);
    color: var(--text);
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 2rem 1rem;
    overflow-x: hidden;
  }

  /* ── Scanline texture overlay ── */
  body::before {
    content: '';
    position: fixed;
    inset: 0;
    background: repeating-linear-gradient(
      0deg,
      transparent,
      transparent 2px,
      rgba(255,255,255,0.012) 2px,
      rgba(255,255,255,0.012) 4px
    );
    pointer-events: none;
    z-index: 9999;
  }

  header {
    width: 100%;
    max-width: 700px;
    display: flex;
    align-items: baseline;
    gap: 1rem;
    margin-bottom: 2.5rem;
    border-bottom: 1px solid var(--border);
    padding-bottom: 1rem;
  }
  header h1 {
    font-size: 1.8rem;
    font-weight: 800;
    letter-spacing: -0.03em;
  }
  header span.chip {
    font-family: 'Space Mono', monospace;
    font-size: 0.65rem;
    padding: 3px 8px;
    border: 1px solid var(--border);
    border-radius: 3px;
    color: var(--muted);
    letter-spacing: 0.1em;
    text-transform: uppercase;
  }
  .dot-live {
    width: 7px; height: 7px;
    border-radius: 50%;
    background: #4cff91;
    display: inline-block;
    margin-right: 4px;
    animation: pulse-live 1.4s ease-in-out infinite;
  }
  @keyframes pulse-live {
    0%,100% { opacity: 1; transform: scale(1); }
    50%      { opacity: 0.4; transform: scale(0.7); }
  }

  /* ── Main card ── */
  .card-main {
    width: 100%;
    max-width: 700px;
    background: var(--panel);
    border: 1px solid var(--border);
    border-radius: 16px;
    padding: 2.5rem;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 2rem;
    transition: box-shadow 0.6s ease;
    position: relative;
    overflow: hidden;
  }

  /* Glow ring behind card — changes colour with emotion */
  .card-main::before {
    content: '';
    position: absolute;
    inset: -2px;
    border-radius: 18px;
    opacity: 0;
    transition: opacity 0.8s ease, background 0.8s ease;
    pointer-events: none;
  }
  .card-main.happy::before   { background: var(--glow-happy);   opacity: 1; }
  .card-main.angry::before   { background: var(--glow-angry);   opacity: 1; }
  .card-main.sad::before     { background: var(--glow-sad);     opacity: 1; }
  .card-main.neutral::before { background: var(--glow-neutral); opacity: 1; }

  /* ── Emoji display ── */
  .emoji-wrap {
    position: relative;
    width: 160px;
    height: 160px;
  }
  .emoji-face {
    font-size: 110px;
    line-height: 160px;
    text-align: center;
    width: 160px;
    display: block;
    transition: transform 0.5s cubic-bezier(.34,1.56,.64,1), filter 0.5s ease;
    will-change: transform;
    user-select: none;
  }
  .emoji-face.pop {
    animation: pop 0.45s cubic-bezier(.34,1.56,.64,1);
  }
  @keyframes pop {
    0%   { transform: scale(0.7) rotate(-8deg); }
    60%  { transform: scale(1.15) rotate(3deg); }
    100% { transform: scale(1) rotate(0deg); }
  }

  /* Orbit ring around emoji */
  .orbit {
    position: absolute;
    inset: 0;
    border-radius: 50%;
    border: 2px solid transparent;
    animation: orbit-spin 6s linear infinite;
    transition: border-color 0.6s ease;
  }
  @keyframes orbit-spin { to { transform: rotate(360deg); } }
  .happy   .orbit { border-top-color: var(--happy);   border-right-color: var(--happy); }
  .angry   .orbit { border-top-color: var(--angry);   border-right-color: var(--angry); animation-duration: 2s; }
  .sad     .orbit { border-top-color: var(--sad);     border-right-color: var(--sad);   animation-duration: 8s; }
  .neutral .orbit { border-top-color: var(--neutral); border-right-color: var(--neutral); animation-duration: 12s; }

  /* ── Emotion label ── */
  .emotion-label {
    font-size: 3rem;
    font-weight: 800;
    letter-spacing: -0.04em;
    line-height: 1;
    transition: color 0.5s ease;
  }
  .happy   .emotion-label { color: var(--happy); }
  .angry   .emotion-label { color: var(--angry); }
  .sad     .emotion-label { color: var(--sad); }
  .neutral .emotion-label { color: var(--neutral); }

  /* ── Confidence bar ── */
  .conf-section {
    width: 100%;
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
  }
  .conf-header {
    display: flex;
    justify-content: space-between;
    font-family: 'Space Mono', monospace;
    font-size: 0.72rem;
    color: var(--muted);
    text-transform: uppercase;
    letter-spacing: 0.1em;
  }
  .conf-bar-bg {
    width: 100%;
    height: 6px;
    background: var(--border);
    border-radius: 3px;
    overflow: hidden;
  }
  .conf-bar-fill {
    height: 100%;
    border-radius: 3px;
    transition: width 0.4s ease, background 0.5s ease;
  }
  .happy   .conf-bar-fill { background: var(--happy); }
  .angry   .conf-bar-fill { background: var(--angry); }
  .sad     .conf-bar-fill { background: var(--sad);   }
  .neutral .conf-bar-fill { background: var(--neutral); }

  /* ── All-class score grid ── */
  .scores-grid {
    width: 100%;
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 0.75rem;
  }
  .score-cell {
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 0.9rem 0.5rem 0.75rem;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 0.35rem;
    transition: border-color 0.4s ease;
  }
  .score-cell.active { border-color: currentColor; }
  .score-cell.active.happy   { color: var(--happy); }
  .score-cell.active.angry   { color: var(--angry); }
  .score-cell.active.sad     { color: var(--sad); }
  .score-cell.active.neutral { color: var(--neutral); }
  .score-emoji { font-size: 1.5rem; line-height: 1; }
  .score-name  { font-family: 'Space Mono', monospace; font-size: 0.6rem; color: var(--muted); text-transform: uppercase; letter-spacing: 0.08em; }
  .score-val   { font-family: 'Space Mono', monospace; font-size: 0.85rem; font-weight: 700; }
  .score-minibar { width: 100%; height: 3px; background: var(--border); border-radius: 2px; overflow: hidden; }
  .score-minibar-fill { height: 100%; border-radius: 2px; background: currentColor; transition: width 0.4s ease; }

  /* ── Stats row ── */
  .stats-row {
    width: 100%;
    display: flex;
    gap: 1rem;
    font-family: 'Space Mono', monospace;
    font-size: 0.68rem;
    color: var(--muted);
    border-top: 1px solid var(--border);
    padding-top: 1.2rem;
  }
  .stat { display: flex; flex-direction: column; gap: 2px; }
  .stat-val { color: var(--text); font-size: 0.85rem; font-weight: 700; }

  /* ── History sparkline ── */
  .history-section { width: 100%; }
  .history-title { font-family: 'Space Mono', monospace; font-size: 0.65rem; color: var(--muted); text-transform: uppercase; letter-spacing: 0.1em; margin-bottom: 0.5rem; }
  .sparkline-wrap { display: flex; gap: 3px; align-items: flex-end; height: 32px; }
  .spark-bar {
    flex: 1;
    border-radius: 2px 2px 0 0;
    transition: height 0.3s ease, background 0.3s ease;
    min-height: 3px;
  }

  /* ── Connection banner ── */
  .connection-banner {
    width: 100%;
    max-width: 700px;
    margin-top: 1rem;
    padding: 0.75rem 1.25rem;
    border-radius: 10px;
    border: 1px solid var(--border);
    font-family: 'Space Mono', monospace;
    font-size: 0.7rem;
    color: var(--muted);
    display: flex;
    align-items: center;
    gap: 0.5rem;
  }
  .connection-banner.connected { border-color: #4cff9144; color: #4cff91; }
  .connection-banner.error     { border-color: #f05c5c44; color: var(--angry); }

  footer {
    margin-top: 2rem;
    font-family: 'Space Mono', monospace;
    font-size: 0.6rem;
    color: var(--muted);
    text-align: center;
    letter-spacing: 0.05em;
  }
</style>
</head>
<body>

<header>
  <h1>EdgeGaze</h1>
  <span class="chip">ESP32-CAM</span>
  <span class="chip">TinyML INT8</span>
  <span class="chip"><span class="dot-live"></span>live</span>
</header>

<div class="card-main neutral" id="card">

  <div class="emoji-wrap neutral" id="emojiWrap">
    <div class="orbit"></div>
    <span class="emoji-face" id="emojiFace" role="img" aria-label="emotion face">😐</span>
  </div>

  <div class="emotion-label" id="emotionLabel">NEUTRAL</div>

  <div class="conf-section">
    <div class="conf-header">
      <span>Confidence</span>
      <span id="confPct">—</span>
    </div>
    <div class="conf-bar-bg">
      <div class="conf-bar-fill" id="confBar" style="width:0%"></div>
    </div>
  </div>

  <div class="scores-grid" id="scoresGrid">
    <!-- Injected by JS -->
  </div>

  <div class="stats-row">
    <div class="stat">
      <span>Inference time</span>
      <span class="stat-val" id="statInfMs">—</span>
    </div>
    <div class="stat">
      <span>Updates</span>
      <span class="stat-val" id="statUpdates">0</span>
    </div>
    <div class="stat">
      <span>ESP32 uptime</span>
      <span class="stat-val" id="statUptime">—</span>
    </div>
    <div class="stat">
      <span>Free heap</span>
      <span class="stat-val" id="statHeap">—</span>
    </div>
  </div>

  <div class="history-section">
    <div class="history-title">Recent history (last 30)</div>
    <div class="sparkline-wrap" id="sparkline"></div>
  </div>

</div>

<div class="connection-banner" id="banner">
  Connecting to event stream…
</div>

<footer>EdgeGaze v1.0 · FreeRTOS · TensorFlow Lite Micro · ESP32-CAM</footer>

<script>
const CLASSES = [
  { name: 'HAPPY',   emoji: '😄', color: '#f9e04b', cls: 'happy'   },
  { name: 'ANGRY',   emoji: '😠', color: '#f05c5c', cls: 'angry'   },
  { name: 'SAD',     emoji: '😢', color: '#6ab0f5', cls: 'sad'     },
  { name: 'NEUTRAL', emoji: '😐', color: '#a0a0b8', cls: 'neutral' },
];

const SPARK_COLORS = ['#f9e04b','#f05c5c','#6ab0f5','#a0a0b8'];

let updateCount = 0;
let lastClass   = -1;
let history     = [];
let startTime   = Date.now();

// ── Build score cells ────────────────────────────────────────────────────────
const grid = document.getElementById('scoresGrid');
CLASSES.forEach((c, i) => {
  grid.innerHTML += `
    <div class="score-cell" id="cell-${i}">
      <span class="score-emoji">${c.emoji}</span>
      <span class="score-name">${c.name}</span>
      <span class="score-val" id="sv-${i}">0%</span>
      <div class="score-minibar"><div class="score-minibar-fill" id="smb-${i}" style="width:0%"></div></div>
    </div>`;
});

// ── Apply result to UI ───────────────────────────────────────────────────────
function applyResult(r) {
  const card     = document.getElementById('card');
  const emojiWrap= document.getElementById('emojiWrap');
  const face     = document.getElementById('emojiFace');
  const label    = document.getElementById('emotionLabel');
  const bar      = document.getElementById('confBar');
  const pct      = document.getElementById('confPct');
  const infMs    = document.getElementById('statInfMs');
  const updates  = document.getElementById('statUpdates');

  const cls  = CLASSES[r.class_idx];
  const conf = (r.confidence * 100).toFixed(1);

  // Swap emotion class
  ['happy','angry','sad','neutral'].forEach(c => {
    card.classList.remove(c);
    emojiWrap.classList.remove(c);
  });
  card.classList.add(cls.cls);
  emojiWrap.classList.add(cls.cls);

  // Emoji pop animation on class change
  if (r.class_idx !== lastClass) {
    face.classList.remove('pop');
    void face.offsetWidth;  // reflow
    face.classList.add('pop');
    face.textContent = cls.emoji;
    face.setAttribute('aria-label', cls.name);
    lastClass = r.class_idx;
  }

  label.textContent = cls.name;
  bar.style.width   = conf + '%';
  pct.textContent   = conf + '%';
  infMs.textContent = r.inference_ms + ' ms';

  updateCount++;
  updates.textContent = updateCount;

  // Per-class scores
  r.scores.forEach((s, i) => {
    const pct = (s * 100).toFixed(1);
    document.getElementById('sv-' + i).textContent  = pct + '%';
    document.getElementById('smb-' + i).style.width = pct + '%';
    const cell = document.getElementById('cell-' + i);
    cell.classList.toggle('active', i === r.class_idx);
    ['happy','angry','sad','neutral'].forEach(c => cell.classList.remove(c));
    if (i === r.class_idx) cell.classList.add(CLASSES[i].cls);
  });

  // History sparkline
  history.push(r.class_idx);
  if (history.length > 30) history.shift();
  renderSparkline();
}

function renderSparkline() {
  const wrap = document.getElementById('sparkline');
  wrap.innerHTML = '';
  history.forEach(ci => {
    const bar = document.createElement('div');
    bar.className = 'spark-bar';
    bar.style.background = SPARK_COLORS[ci];
    bar.style.height = (16 + ci * 5) + 'px';  // simple height variation by class
    wrap.appendChild(bar);
  });
}

// ── System stats polling ─────────────────────────────────────────────────────
async function fetchStats() {
  try {
    const r = await fetch('/status');
    if (!r.ok) return;
    const d = await r.json();
    document.getElementById('statUptime').textContent =
      Math.floor(d.uptime_ms / 1000) + 's';
    document.getElementById('statHeap').textContent =
      Math.floor(d.free_heap / 1024) + ' KB';
  } catch {}
}
setInterval(fetchStats, 3000);

// ── SSE connection ───────────────────────────────────────────────────────────
function connectSSE() {
  const banner = document.getElementById('banner');
  banner.textContent = 'Connecting…';
  banner.className   = 'connection-banner';

  const es = new EventSource('/events');

  es.onopen = () => {
    banner.textContent = '⬤ Connected — live stream active';
    banner.className   = 'connection-banner connected';
  };

  es.addEventListener('result', e => {
    try {
      const r = JSON.parse(e.data);
      applyResult(r);
    } catch(err) { console.error('Parse error', err); }
  });

  es.onerror = () => {
    banner.textContent = '✕ Connection lost — reconnecting…';
    banner.className   = 'connection-banner error';
    es.close();
    setTimeout(connectSSE, 3000);
  };
}

connectSSE();
fetchStats();
</script>
</body>
</html>
)rawhtml";

// ── Route handlers ────────────────────────────────────────────────────────────
static void handleRoot() {
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "text/html", DASHBOARD_HTML);
}

static void handleLatest() {
    if (!g_has_result) {
        server.send(200, "application/json", "{\"status\":\"waiting\"}");
        return;
    }
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"class_idx\":%d,\"confidence\":%.3f,\"inference_ms\":%u,"
        "\"scores\":[%.3f,%.3f,%.3f,%.3f]}",
        g_latest_result.class_idx,
        g_latest_result.confidence,
        g_latest_result.inference_ms,
        g_latest_result.scores[0],
        g_latest_result.scores[1],
        g_latest_result.scores[2],
        g_latest_result.scores[3]);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", buf);
}

static void handleStatus() {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"uptime_ms\":%lu,\"free_heap\":%u,\"min_heap\":%u}",
        millis(),
        esp_get_free_heap_size(),
        esp_get_minimum_free_heap_size());
    server.send(200, "application/json", buf);
}

static void handleNotFound() {
    server.send(404, "text/plain", "Not found");
}

// ── SSE stream helper ─────────────────────────────────────────────────────────
static void sendSSEResult(WiFiClient& client, const InferenceResult& r) {
    char data[256];
    snprintf(data, sizeof(data),
        "{\"class_idx\":%d,\"confidence\":%.3f,\"inference_ms\":%u,"
        "\"scores\":[%.3f,%.3f,%.3f,%.3f]}",
        r.class_idx, r.confidence, r.inference_ms,
        r.scores[0], r.scores[1], r.scores[2], r.scores[3]);

    client.print("event: result\n");
    client.print("data: ");
    client.print(data);
    client.print("\n\n");
}

// ── Main web task ─────────────────────────────────────────────────────────────
void webTask(void* pvParams) {
    Serial.println("[WEB] Task started");

    // Connect WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.printf("[WEB] Connecting to %s", WIFI_SSID);

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
        if (millis() - t0 > 20000) {
            Serial.println("\n[WEB] WiFi timeout — rebooting");
            ESP.restart();
        }
    }
    Serial.printf("\n[WEB] Connected! IP: %s\n", WiFi.localIP().toString().c_str());

    server.on("/",       handleRoot);
    server.on("/latest", handleLatest);
    server.on("/status", handleStatus);

    // SSE endpoint — upgrade connection and keep it open
    server.on("/events", [&]() {
        WiFiClient client = server.client();
        client.print("HTTP/1.1 200 OK\r\n");
        client.print("Content-Type: text/event-stream\r\n");
        client.print("Cache-Control: no-cache\r\n");
        client.print("Connection: keep-alive\r\n");
        client.print("Access-Control-Allow-Origin: *\r\n");
        client.print("\r\n");
        client.print(": EdgeGaze SSE stream\n\n");

        sse_client = client;
        sse_active = true;
        Serial.println("[WEB] SSE client connected");

        // This call returns immediately; client tracked externally
        server.client().setTimeout(0);
    });

    server.onNotFound(handleNotFound);
    server.begin();
    Serial.printf("[WEB] HTTP server on port %d\n", WEB_PORT);

    InferenceResult result;
    uint32_t last_ping = 0;

    for (;;) {
        server.handleClient();

        // Pull latest result
        if (xQueuePeek(g_result_queue, &result, 0) == pdTRUE) {
            g_latest_result = result;
            g_has_result    = true;

            // Push to SSE client if connected
            if (sse_active && sse_client.connected()) {
                sendSSEResult(sse_client, result);
            } else if (sse_active) {
                sse_active = false;
                Serial.println("[WEB] SSE client disconnected");
            }
        }

        // SSE keepalive comment every 15s
        if (sse_active && sse_client.connected()) {
            uint32_t now = millis();
            if (now - last_ping > 15000) {
                sse_client.print(": ping\n\n");
                last_ping = now;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));  // 20hz web loop
    }
}