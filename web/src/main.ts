// App wiring: pick a source (in-browser simulator or a live server), render each
// frame to waveform + bands + mind-state + topography + 3D electrode views.

import { Brain3D } from "./brain3d";
import { drawBands, drawMindTimeline, drawWaveform } from "./chart";
import { DroneController } from "./drone";
import { mindState, trailingMean } from "./mind";
import { DEFAULT_BROKER, type MqttStatus, MqttPublisher } from "./mqtt";
import { type Frame, ServerSource, SimulatorSource, type Source } from "./source";
import { drawTopography } from "./topography";

const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;

const waveCanvas = $<HTMLCanvasElement>("wave");
const bandsCanvas = $<HTMLCanvasElement>("bands");
const topoCanvas = $<HTMLCanvasElement>("topo");
const brainCanvas = $<HTMLCanvasElement>("brain3d");
const mindTimelineCanvas = $<HTMLCanvasElement>("mind-timeline");
const statusEl = $("status");
const statusDot = $("status-dot");
const modeSel = $<HTMLSelectElement>("mode");
const urlInput = $<HTMLInputElement>("server-url");
const applyBtn = $<HTMLButtonElement>("apply");
const bandTable = $("band-values");
const meta = $("meta");
const topoBandSel = $<HTMLSelectElement>("topo-band");
const brainBandLabel = $("brain3d-band-label");

const mindLabel = $("mind-label");
const mindHint = $("mind-hint");
const mindCard = $("mind-card");
const focusFill = $("focus-fill");
const relaxFill = $("relax-fill");
const focusNum = $("focus-num");
const relaxNum = $("relax-num");
const mindMeta = $("mind-meta");

// Drone control DOM
const brokerInput = $<HTMLInputElement>("broker-url");
const topicInput = $<HTMLInputElement>("topic");
const mqttConnectBtn = $<HTMLButtonElement>("mqtt-connect");
const mqttDot = $("mqtt-dot");
const mqttStatusEl = $("mqtt-status");
const armToggle = $<HTMLButtonElement>("arm-toggle");
const estopBtn = $<HTMLButtonElement>("estop");
const cmdValue = $("cmd-value");
const cmdArm = $("cmd-arm");

const HINTS: Record<string, string> = {
  focused: "β > α: 認知活動が高い",
  relaxed: "α > β: α優位でリラックス",
  neutral: "α と β が拮抗",
};

let current: Source | null = null;
const brain = new Brain3D(brainCanvas);
brain.start();

// Rolling focus/relax history (client-side) for the timeline.
const MIND_HISTORY = 240; // ~24s at 10Hz
const mindHistory: { focus: number; relax: number }[] = [];

function setStatus(connected: boolean, detail: string): void {
  statusEl.textContent = detail;
  statusDot.className = connected ? "dot on" : "dot off";
}

// ---- Drone control (MQTT over WSS) ----------------------------------------

const mqtt = new MqttPublisher(onMqttStatus);
const drone = new DroneController({ publish: (payload) => mqtt.publish(controlTopic(), payload) });
let mqttConnected = false;

function controlTopic(): string {
  const base = topicInput.value.trim() || "stampfly/s3";
  return `${base}/control`;
}

function onMqttStatus(status: MqttStatus, detail: string): void {
  mqttConnected = status === "connected";
  mqttStatusEl.textContent = detail;
  mqttDot.className = mqttConnected ? "dot on" : "dot off";
  armToggle.disabled = !mqttConnected;
  estopBtn.disabled = !mqttConnected;
  mqttConnectBtn.textContent = mqttConnected ? "切断" : "接続";
  if (!mqttConnected) reflectArm(false); // lost link → treat as disarmed in UI
}

function reflectArm(armed: boolean): void {
  armToggle.textContent = armed ? "DISARM" : "ARM";
  armToggle.className = armed ? "arm-btn armed" : "arm-btn";
  cmdArm.textContent = armed ? "ARMED" : "DISARMED";
  cmdArm.className = armed ? "cmd-arm armed" : "cmd-arm disarmed";
}

function updateMind(f: Frame): void {
  const ms = mindState(f.bands);
  mindHistory.push({ focus: ms.focus, relax: ms.relax });
  if (mindHistory.length > MIND_HISTORY) mindHistory.shift();

  const focusSmooth = trailingMean(mindHistory.map((s) => s.focus), 3);
  const relaxSmooth = trailingMean(mindHistory.map((s) => s.relax), 3);

  mindLabel.textContent = ms.label;
  mindHint.textContent = HINTS[ms.status];
  mindCard.className = `card mind mind-${ms.status}`;
  focusFill.style.width = `${Math.max(0, Math.min(100, (focusSmooth / 2) * 100))}%`;
  relaxFill.style.width = `${Math.max(0, Math.min(100, relaxSmooth * 100))}%`;
  focusNum.textContent = focusSmooth.toFixed(2);
  relaxNum.textContent = relaxSmooth.toFixed(2);
  mindMeta.textContent = `16ch平均 · ${mindHistory.length} samples`;
  drawMindTimeline(mindTimelineCanvas, mindHistory);

  // Drive the drone from the smoothed focus. Only publishes if connected;
  // update() self-throttles and sends 0 while disarmed.
  if (mqttConnected) {
    const cmd = drone.update(focusSmooth);
    if (cmd) cmdValue.textContent = cmd.v.toFixed(2);
  }
}

function onFrame(f: Frame): void {
  drawWaveform(waveCanvas, f.raw);
  drawBands(bandsCanvas, f.bands);
  bandTable.innerHTML = Object.entries(f.bands)
    .map(
      ([k, v]) =>
        `<div class="bv"><span class="bk">${k}</span><span class="bd">${v.toExponential(2)}</span> <span class="unit">μV²</span></div>`,
    )
    .join("");
  meta.textContent = `${f.source} · ${f.channels}ch · ${f.srate}Hz · ${f.raw.length} pts`;

  const band = topoBandSel.value;
  const perCh = f.bandsPerCh?.[band] ?? [];
  drawTopography(topoCanvas, perCh);
  brain.setData(perCh);
  brainBandLabel.textContent = band;

  updateMind(f);
}

function switchSource(): void {
  current?.stop();
  const serverMode = modeSel.value === "server";
  urlInput.disabled = !serverMode;
  current = serverMode
    ? new ServerSource(urlInput.value.trim(), onFrame, setStatus)
    : new SimulatorSource(onFrame, setStatus);
  current.start();
}

// Default server URL depends on how the page is served:
// - HTTPS (GitHub Pages) → must use wss:// (browsers block ws:// from https).
//   Points at the Pi's tailscale-serve TLS endpoint. Change to your own host.
// - http/localhost (local dev) → the local server on ws://localhost:8000/ws.
const DEFAULT_WSS = "wss://pieeg.tail29b1d2.ts.net/ws";
const DEFAULT_WS_LOCAL = "ws://localhost:8000/ws";
urlInput.value = location.protocol === "https:" ? DEFAULT_WSS : DEFAULT_WS_LOCAL;

// URL params: ?mode=server&url=wss://host/ws  (override the default above)
const params = new URLSearchParams(location.search);
if (params.get("url")) urlInput.value = params.get("url")!;
if (params.get("mode") === "server") modeSel.value = "server";

applyBtn.addEventListener("click", switchSource);
modeSel.addEventListener("change", switchSource);

// ---- Drone control wiring --------------------------------------------------

// Topic base must match the firmware's TOPIC_BASE (default "stampfly/demo").
// Override via ?topic=stampfly/xyz if you reflash the drone with another base.
brokerInput.value = DEFAULT_BROKER;
topicInput.value = new URLSearchParams(location.search).get("topic") ?? "stampfly/demo";

mqttConnectBtn.addEventListener("click", () => {
  if (mqttConnected) {
    drone.stop(); // safety: publish 0 + disarm before dropping the link
    reflectArm(false);
    mqtt.disconnect();
  } else {
    mqtt.connect(brokerInput.value.trim());
  }
});

armToggle.addEventListener("click", () => {
  const next = !drone.isArmed();
  const cmd = drone.setArmed(next);
  reflectArm(next);
  cmdValue.textContent = cmd.v.toFixed(2);
});

estopBtn.addEventListener("click", () => {
  const cmd = drone.stop();
  reflectArm(false);
  cmdValue.textContent = cmd.v.toFixed(2);
});

reflectArm(false);

switchSource();
