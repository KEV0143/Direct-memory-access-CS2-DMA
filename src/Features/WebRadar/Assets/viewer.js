"use strict";

const TEAM_T = 2;
const TEAM_CT = 3;
const DOT_SIZE_BASELINE = 1.3;

const KNOWN_MODELS = new Set([
  "ctm_sas",
  "tm_phoenix"
]);

const KNOWN_ICONS = new Set([
  "ak47",
  "aug",
  "awp",
  "bizon",
  "c4",
  "cz75a",
  "deagle",
  "decoy",
  "defuser",
  "elite",
  "famas",
  "fiveseven",
  "flashbang",
  "g3sg1",
  "galilar",
  "gauge",
  "glock",
  "health",
  "hegrenade",
  "incgrenade",
  "kevlar",
  "kevlar_helmet",
  "knife",
  "m249",
  "m4a1",
  "m4a1_silencer",
  "mac10",
  "mag7",
  "molotov",
  "mp5sd",
  "mp7",
  "mp9",
  "negev",
  "nova",
  "p2000",
  "p250",
  "p90",
  "revolver",
  "sawedoff",
  "scar20",
  "sg556",
  "smokegrenade",
  "ssg08",
  "taser",
  "tec9",
  "ump45",
  "usp_silencer",
  "xm1014"
]);

const DEFAULT_SETTINGS = {
  dotSize: 1.0,
  bombSize: 0.7,
  viewConeOpacity: 0.22,
  showAllNames: false,
  showEnemyNames: true,
  showViewCones: false,
  showSelf: true,
  showCt: true,
  showT: true,
  showAlive: true,
  showDead: true
};

const STORAGE_KEY = "kevq_webradar_clau_style_v4";
const SNAPSHOT_BUFFER_LIMIT = 10;
const UI_RENDER_INTERVAL_MS = 40;
const INITIAL_RENDER_DELAY_MS = 18;

const S = {
  payload: null,
  renderPayload: null,
  pendingPayload: null,
  currentMap: "",
  currentTransport: "idle",
  connectionState: "idle",
  mapDataCache: new Map(),
  mapsIndex: null,
  markers: new Map(),
  rotations: new Map(),
  snapshots: [],
  rosterCards: {
    t: new Map(),
    ct: new Map()
  },
  lastKnownPositions: new Map(),
  lastKnownBombPosition: null,
  lastKnownBombPositionAt: 0,
  lastKnownBombMode: "",
  lastKnownBombMap: "",
  bombTimerSuppressUntil: 0,
  bombUi: {
    active: false,
    defusing: false,
    bombLeft: 0,
    bombTotal: 40,
    defuseLeft: 0,
    defuseTotal: 10,
    lastUpdateAt: 0
  },
  bombMarker: null,
  settings: loadSettings(),
  reconnectDelay: 400,
  rttSamples: [],
  rttMedianMs: 0,
  arrivalJitterMs: 0,
  renderDelayMs: INITIAL_RENDER_DELAY_MS,
  serverClockOffsetMs: NaN,
  minClockOffsetMs: Number.POSITIVE_INFINITY,
  lastFrameAt: 0,
  frameGapMs: 8,
  rosterSignatures: {
    t: "",
    ct: ""
  },
  socket: null,
  eventSource: null,
  pollingTimer: 0,
  uiRenderTimer: 0,
  renderHandle: 0,
  animationHandle: 0,
  animationClock: 0,
  statusPrimed: false,
  overlayContext: null,
  layoutDirty: true,
  cachedOverlay: null,
  cachedFontStr: "",
  labelMeasureCache: new Map()
};

const $ = id => document.getElementById(id);
const VIEW_PARAMS = new URLSearchParams(window.location.search);
const IS_ONLY_RADAR = VIEW_PARAMS.get("view") === "radar";
const C4_ICON = new Image();
C4_ICON.decoding = "async";
C4_ICON.src = "./assets/icons/c4.svg";
C4_ICON.addEventListener("load", () => drawOverlayCanvas());

if (IS_ONLY_RADAR) {
  document.body.classList.add("only-radar");
}

const el = {
  connectionPill: $("connection-pill"),
  connectionText: $("connection-text"),
  metaMap: $("meta-map"),
  metaRtt: $("meta-rtt"),
  filterSelf: $("filter-self"),
  filterCt: $("filter-ct"),
  filterT: $("filter-t"),
  filterAlive: $("filter-alive"),
  filterDead: $("filter-dead"),
  settingsToggle: $("settings-toggle"),
  settingsPopover: $("settings-popover"),
  onlyRadarLaunch: $("only-radar-launch"),
  dotSize: $("setting-dot-size"),
  dotSizeValue: $("setting-dot-size-value"),
  bombSize: $("setting-bomb-size"),
  bombSizeValue: $("setting-bomb-size-value"),
  viewConeOpacity: $("setting-view-cone-opacity"),
  viewConeOpacityValue: $("setting-view-cone-opacity-value"),
  allyNames: $("setting-ally-names"),
  enemyNames: $("setting-enemy-names"),
  viewCones: $("setting-view-cones"),
  terroristList: $("terrorist-list"),
  counterTerroristList: $("counter-terrorist-list"),
  terroristCount: $("terrorist-count"),
  counterTerroristCount: $("counter-terrorist-count"),
  radarMapName: $("radar-map-name"),
  radarEntities: $("radar-entities"),
  radarFocus: $("radar-focus"),
  footerTCount: $("footer-t-count"),
  footerCtCount: $("footer-ct-count"),
  footerVisibleCount: $("footer-visible-count"),
  mapShell: $("map-shell"),
  mapBackground: $("map-background"),
  mapImage: $("map-image"),
  radarOverlay: $("radar-overlay"),
  radarStage: $("radar-stage"),
  radarMessage: $("radar-message"),
  bombStatus: $("bomb-status"),
  bombCarrierRow: $("bomb-carrier-row"),
  bombCarrierName: $("bomb-carrier-name"),
  bombTimeRow: $("bomb-time-row"),
  bombTimeBar: $("bomb-time-bar"),
  bombTimeLeft: $("bomb-time-left"),
  bombTimeProgress: $("bomb-time-progress"),
  defuseTimerRow: $("defuse-timer-row"),
  defuseTimeLeft: $("defuse-time-left"),
  defuseTimeProgress: $("defuse-time-progress")
};

function loadSettings() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (raw) {
      return sanitizeSettings(JSON.parse(raw));
    }

    const legacyRawV3 = localStorage.getItem("kevq_webradar_clau_style_v3");
    if (legacyRawV3) {
      const legacySettings = JSON.parse(legacyRawV3);
      return sanitizeSettings({
        ...legacySettings,
        dotSize: Number(legacySettings?.dotSize ?? DEFAULT_SETTINGS.dotSize) / DOT_SIZE_BASELINE
      });
    }

    const legacyRawV2 = localStorage.getItem("kevq_webradar_clau_style_v2");
    if (legacyRawV2) {
      const legacySettings = JSON.parse(legacyRawV2);
      return sanitizeSettings({
        ...legacySettings,
        dotSize: Number(legacySettings?.dotSize ?? DEFAULT_SETTINGS.dotSize) / DOT_SIZE_BASELINE
      });
    }

    return { ...DEFAULT_SETTINGS };
  } catch {
    return { ...DEFAULT_SETTINGS };
  }
}

function saveSettings() {
  S.settings = sanitizeSettings(S.settings);
  localStorage.setItem(STORAGE_KEY, JSON.stringify(S.settings));
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function sanitizeSettings(settings) {
  const source = settings && typeof settings === "object" ? settings : {};
  return {
    dotSize: clamp(Number(source.dotSize ?? DEFAULT_SETTINGS.dotSize), 0.25, 2),
    bombSize: clamp(Number(source.bombSize ?? DEFAULT_SETTINGS.bombSize), 0.15, 2),
    viewConeOpacity: clamp(Number(source.viewConeOpacity ?? DEFAULT_SETTINGS.viewConeOpacity), 0.05, 0.65),
    showAllNames: !!source.showAllNames,
    showEnemyNames: source.showEnemyNames !== false,
    showViewCones: !!source.showViewCones,
    showSelf: source.showSelf !== false,
    showCt: source.showCt !== false,
    showT: source.showT !== false,
    showAlive: source.showAlive !== false,
    showDead: source.showDead !== false
  };
}

function shortestAngleDelta(from, to) {
  return ((to - from + 540) % 360) - 180;
}

function settleValue(current, target, blend, epsilon) {
  const next = current + (target - current) * blend;
  return Math.abs(target - next) <= epsilon ? target : next;
}

function settleAngle(current, target, blend, epsilon) {
  const next = current + shortestAngleDelta(current, target) * blend;
  return Math.abs(shortestAngleDelta(next, target)) <= epsilon ? target : next;
}

function setPageBackground(imagePath) {
  document.documentElement.style.setProperty(
    "--page-radar-background",
    imagePath ? "url('" + imagePath + "')" : "none"
  );
}

function normalizeMapLabel(mapName) {
  return String(mapName || "unknown");
}

function updateConnection(state) {
  if (S.connectionState === state) return;
  S.connectionState = state;
  el.connectionPill.className = "connection-pill";
  if (state === "live") {
    el.connectionPill.classList.add("is-live");
    el.connectionPill.title = "Live";
    el.connectionText.textContent = "";
    return;
  }
  if (state === "error") {
    el.connectionPill.classList.add("is-error");
    el.connectionPill.title = "Disconnected";
    el.connectionText.textContent = "";
    return;
  }
  el.connectionPill.classList.add("is-connecting");
  el.connectionPill.title = "Connecting";
  el.connectionText.textContent = "";
}

function pushRttSample(ms) {
  if (!Number.isFinite(ms)) return;
  if (!S.rttSamples.length && ms > 250) return;
  S.rttSamples.push(ms);
  if (S.rttSamples.length > 10) {
    S.rttSamples.shift();
  }
  const sorted = [...S.rttSamples].sort((a, b) => a - b);
  const middle = Math.floor(sorted.length / 2);
  const median = sorted.length % 2
    ? sorted[middle]
    : (sorted[middle - 1] + sorted[middle]) / 2;
  S.rttMedianMs = median;
  el.metaRtt.textContent = Math.round(median) + " ms";
  updateAdaptiveDelay();
}

async function pingLoop() {
  try {
    const started = performance.now();
    await fetch("./api/ping?t=" + Date.now(), { cache: "no-store", priority: "low" });
    pushRttSample(performance.now() - started);
  } catch {
  }
  setTimeout(pingLoop, 1500);
}

function syncSettingsUi() {
  el.dotSize.value = String(S.settings.dotSize);
  el.dotSizeValue.textContent = S.settings.dotSize.toFixed(1) + "x";
  el.bombSize.value = String(S.settings.bombSize);
  el.bombSizeValue.textContent = S.settings.bombSize.toFixed(1) + "x";
  el.viewConeOpacity.value = String(S.settings.viewConeOpacity);
  el.viewConeOpacityValue.textContent = Math.round(S.settings.viewConeOpacity * 100) + "%";
  el.allyNames.checked = !!S.settings.showAllNames;
  el.enemyNames.checked = !!S.settings.showEnemyNames;
  el.viewCones.checked = !!S.settings.showViewCones;
  el.filterSelf.checked = !!S.settings.showSelf;
  el.filterCt.checked = !!S.settings.showCt;
  el.filterT.checked = !!S.settings.showT;
  el.filterAlive.checked = !!S.settings.showAlive;
  el.filterDead.checked = !!S.settings.showDead;
}

function bindUi() {
  syncSettingsUi();

  el.settingsToggle.addEventListener("click", () => {
    el.settingsPopover.classList.toggle("hidden");
  });

  document.addEventListener("click", event => {
    if (el.settingsPopover.classList.contains("hidden")) return;
    if (el.settingsPopover.contains(event.target) || el.settingsToggle.contains(event.target)) return;
    el.settingsPopover.classList.add("hidden");
  });

  if (el.onlyRadarLaunch) {
    el.onlyRadarLaunch.addEventListener("click", () => {
      const url = new URL(window.location.href);
      url.searchParams.set("view", "radar");
      const popupSize = Math.max(760, Math.min(1100, window.screen.availWidth - 96, window.screen.availHeight - 96));
      const left = Math.max(0, Math.round((window.screen.availWidth - popupSize) * 0.5));
      const top = Math.max(0, Math.round((window.screen.availHeight - popupSize) * 0.5));
      const features = [
        "popup=yes",
        "resizable=yes",
        "scrollbars=no",
        "toolbar=no",
        "menubar=no",
        "location=no",
        "status=no",
        "width=" + popupSize,
        "height=" + popupSize,
        "left=" + left,
        "top=" + top
      ].join(",");
      window.open(url.toString(), "kevq_only_radar", features);
    });
  }

  const bindRange = (input, key, output, formatter) => {
    input.addEventListener("input", event => {
      S.settings[key] = Number(event.target.value);
      output.textContent = formatter ? formatter(S.settings[key]) : (S.settings[key].toFixed(1) + "x");
      saveSettings();
      render();
    });
  };

  const bindToggle = (input, key) => {
    input.addEventListener("change", event => {
      S.settings[key] = !!event.target.checked;
      saveSettings();
      render();
    });
  };

  bindRange(el.dotSize, "dotSize", el.dotSizeValue);
  bindRange(el.bombSize, "bombSize", el.bombSizeValue);
  bindRange(el.viewConeOpacity, "viewConeOpacity", el.viewConeOpacityValue, value => Math.round(value * 100) + "%");
  bindToggle(el.allyNames, "showAllNames");
  bindToggle(el.enemyNames, "showEnemyNames");
  bindToggle(el.viewCones, "showViewCones");
  bindToggle(el.filterSelf, "showSelf");
  bindToggle(el.filterCt, "showCt");
  bindToggle(el.filterT, "showT");
  bindToggle(el.filterAlive, "showAlive");
  bindToggle(el.filterDead, "showDead");

  el.mapImage.addEventListener("load", () => {
    S.layoutDirty = true;
    refreshOverlayMetrics();
    render();
  });

  if (window.ResizeObserver) {
    new ResizeObserver(() => {
      S.layoutDirty = true;
    }).observe(el.radarStage);
  } else {
    window.addEventListener("resize", () => {
      S.layoutDirty = true;
    });
  }
}

function teamColor(team) {
  return Number(team) === TEAM_CT ? "#84c8ed" : "#df7d29";
}

function getPlayerColor(player, localTeam) {
  if (player.m_is_local) return "#ffffff";
  return Number(player.m_team) === Number(localTeam) ? "#84c8ed" : "#ff2f2f";
}

function getRosterAccent(player) {
  if (Number(player.m_team) === TEAM_CT) return "#84c8ed";
  if (Number(player.m_team) === TEAM_T) return "#df7d29";
  return "#b1d0e7";
}

function shouldShowOnRadar(player, localTeam) {
  const isLocal = !!player.m_is_local;
  const dead = !!player.m_is_dead;
  const team = Number(player.m_team || 0);

  if (isLocal && !S.settings.showSelf) return false;
  if (team === TEAM_CT && !S.settings.showCt) return false;
  if (team === TEAM_T && !S.settings.showT) return false;
  if (!dead && !S.settings.showAlive) return false;
  if (dead && !S.settings.showDead) return false;
  return true;
}

function shouldShowName(player, localTeam) {
  if (player.m_is_local) return false;
  if (Number(player.m_team) === Number(localTeam)) return !!S.settings.showAllNames;
  return !!S.settings.showEnemyNames;
}

function getMarkerSize(overlayWidth) {
  const effectiveDotScale = S.settings.dotSize * DOT_SIZE_BASELINE;
  return clamp(overlayWidth * 0.027 * effectiveDotScale, 6, overlayWidth * 0.054 * DOT_SIZE_BASELINE);
}

function getBombMarkerSize(overlayWidth, isDropped) {
  if (!isDropped) {
    return clamp(overlayWidth * 0.028, 13, overlayWidth * 0.05);
  }

  const base = overlayWidth * 0.02 * S.settings.bombSize;
  return clamp(base, 11, overlayWidth * 0.043);
}

function getRadarPosition(mapData, entityCoords) {
  if (!entityCoords || !Number.isFinite(entityCoords.x) || !Number.isFinite(entityCoords.y)) {
    return { x: 0, y: 0, invalid: true };
  }
  if (!mapData || !Number.isFinite(mapData.x) || !Number.isFinite(mapData.y) || !Number.isFinite(mapData.scale)) {
    return { x: 0, y: 0, invalid: true };
  }

  return {
    x: (entityCoords.x - mapData.x) / mapData.scale / 1024,
    y: (((entityCoords.y - mapData.y) / mapData.scale) * -1.0) / 1024,
    invalid: false
  };
}

async function ensureMapsIndex() {
  if (S.mapsIndex) return S.mapsIndex;
  try {
    const response = await fetch("./maps.json", { cache: "force-cache" });
    if (!response.ok) return null;
    const json = await response.json();
    const index = new Map();
    const maps = Array.isArray(json.maps) ? json.maps : [];
    for (const map of maps) {
      index.set(String(map.name || ""), map);
    }
    S.mapsIndex = index;
    return index;
  } catch {
    return null;
  }
}

async function ensureMapData(mapName) {
  if (!mapName || mapName === "unknown") return null;
  if (S.mapDataCache.has(mapName)) return S.mapDataCache.get(mapName);

  try {
    const response = await fetch("./data/" + mapName + "/data.json", { cache: "force-cache" });
    if (response.ok) {
      const data = await response.json();
      const normalized = {
        x: Number(data.x),
        y: Number(data.y),
        scale: Number(data.scale),
        name: mapName
      };
      S.mapDataCache.set(mapName, normalized);
      return normalized;
    }
  } catch {
  }

  const mapsIndex = await ensureMapsIndex();
  const map = mapsIndex && mapsIndex.get(mapName);
  if (!map) return null;

  const normalized = {
    x: Number(map.origin?.x),
    y: Number(map.origin?.y),
    scale: Number(map.scale),
    name: mapName
  };
  S.mapDataCache.set(mapName, normalized);
  return normalized;
}

function clearMapAssets(mapName = "unknown") {
  const mapLabel = normalizeMapLabel(mapName);
  S.currentMap = "";
  S.lastKnownBombPosition = null;
  S.lastKnownBombPositionAt = 0;
  S.lastKnownBombMode = "";
  S.lastKnownBombMap = mapLabel;
  if (S.bombMarker) {
    S.bombMarker.state.active = false;
  }
  el.mapShell.classList.add("is-map-unknown");
  el.mapImage.removeAttribute("src");
  el.mapImage.alt = "";
  el.mapBackground.removeAttribute("src");
  el.mapBackground.alt = "";
  el.metaMap.textContent = mapLabel;
  el.radarMapName.textContent = mapLabel;
  S.cachedOverlay = null;
  clearOverlay();
}

function updateMapAssets(mapName) {
  if (!mapName || mapName === "unknown") {
    clearMapAssets("unknown");
    return;
  }
  const radarPath = "./data/" + mapName + "/radar.webp";
  const mapLabel = normalizeMapLabel(mapName);

  if (S.currentMap !== mapName) {
    S.currentMap = mapName;
    el.mapShell.classList.remove("is-map-unknown");
    S.lastKnownBombPosition = null;
    S.lastKnownBombPositionAt = 0;
    S.lastKnownBombMode = "";
    S.lastKnownBombMap = mapName;
    if (S.bombMarker) {
      S.bombMarker.state.active = false;
    }
    if (!el.mapImage.src.endsWith("/data/" + mapName + "/radar.webp")) {
      el.mapImage.src = radarPath;
    }
    el.mapBackground.removeAttribute("src");
    el.mapBackground.alt = "";
    el.mapImage.alt = mapName + " radar";
  }

  if (el.metaMap.textContent !== mapLabel) {
    el.metaMap.textContent = mapLabel;
  }
  if (el.radarMapName.textContent !== mapLabel) {
    el.radarMapName.textContent = mapLabel;
  }
}

function updateFrameGap(snapshot) {
  const now = getSnapshotSampleTime(snapshot);
  if (!Number.isFinite(now) || now <= 0) return;
  if (S.lastFrameAt > 0) {
    const diff = clamp(now - S.lastFrameAt, 4, 42);
    S.frameGapMs = clamp(Math.round(S.frameGapMs * 0.35 + diff * 0.65), 4, 36);
  }
  S.lastFrameAt = now;
  updateAdaptiveDelay();
}

function getSnapshotSampleTime(snapshot) {
  return Number(snapshot?.m_capture_time ?? snapshot?.m_server_time ?? snapshot?.m_ts ?? 0);
}

function updateAdaptiveDelay() {
  const halfRtt = Number.isFinite(S.rttMedianMs) ? (S.rttMedianMs * 0.5) : 0;
  const gap = Number.isFinite(S.frameGapMs) ? S.frameGapMs : 8;
  const jitter = Number.isFinite(S.arrivalJitterMs) ? S.arrivalJitterMs : 0;
  const target = Math.max(gap * 1.8, halfRtt + 6 + jitter * 1.35);
  if (!Number.isFinite(S.renderDelayMs) || S.renderDelayMs <= 0) {
    S.renderDelayMs = clamp(target, 12, 48);
    return;
  }
  S.renderDelayMs = clamp(S.renderDelayMs * 0.82 + target * 0.18, 12, 48);
}

function sanitizePayload(payload) {
  if (!payload || typeof payload !== "object") return null;
  payload.m_players = Array.isArray(payload.m_players) ? payload.m_players : [];
  payload.m_bomb = payload.m_bomb && typeof payload.m_bomb === "object" ? payload.m_bomb : null;
  return payload;
}

function ingestSnapshot(payload, arrivalNow) {
  const sampleTime = getSnapshotSampleTime(payload);
  if (!Number.isFinite(sampleTime) || sampleTime <= 0) return false;

  let snapshots = S.snapshots;
  let prev = snapshots.length ? snapshots[snapshots.length - 1] : null;
  if (prev) {
    const prevSeq = Number(prev.payload?.m_seq || 0);
    const nextSeq = Number(payload.m_seq || 0);
    const timelineRestarted =
      sampleTime + 200 < prev.sampleTime ||
      (prevSeq > 0 && nextSeq > 0 && nextSeq + 32 < prevSeq);
    if (timelineRestarted) {
      resetSnapshotTimeline();
      snapshots = S.snapshots;
      prev = null;
    }
  }

  if (prev && sampleTime <= prev.sampleTime) {
    if (Number(payload.m_seq || 0) <= Number(prev.payload?.m_seq || 0)) {
      return false;
    }
  }

  if (prev) {
    const serverGap = clamp(sampleTime - prev.sampleTime, 1, 100);
    const arrivalGap = clamp(arrivalNow - prev.arrivalTime, 1, 100);
    const jitterSample = Math.abs(arrivalGap - serverGap);
    S.arrivalJitterMs = S.arrivalJitterMs > 0
      ? (S.arrivalJitterMs * 0.82 + jitterSample * 0.18)
      : jitterSample;
  }

  const offsetSample = arrivalNow - sampleTime;
  if (Number.isFinite(offsetSample)) {
    S.minClockOffsetMs = Math.min(S.minClockOffsetMs, offsetSample);
    const baselineOffset = Number.isFinite(S.minClockOffsetMs) ? S.minClockOffsetMs : offsetSample;
    S.serverClockOffsetMs = Number.isFinite(S.serverClockOffsetMs)
      ? (S.serverClockOffsetMs * 0.9 + baselineOffset * 0.1)
      : baselineOffset;
  }

  snapshots.push({
    payload,
    sampleTime,
    arrivalTime: arrivalNow
  });
  if (snapshots.length > SNAPSHOT_BUFFER_LIMIT) {
    snapshots.splice(0, snapshots.length - SNAPSHOT_BUFFER_LIMIT);
  }

  updateFrameGap(payload);
  return true;
}

function resetSnapshotTimeline() {
  S.snapshots.length = 0;
  S.renderPayload = null;
  S.serverClockOffsetMs = NaN;
  S.minClockOffsetMs = Number.POSITIVE_INFINITY;
  S.arrivalJitterMs = 0;
  S.lastFrameAt = 0;
  S.frameGapMs = 8;
  S.renderDelayMs = INITIAL_RENDER_DELAY_MS;
  S.lastKnownBombPosition = null;
  S.lastKnownBombPositionAt = 0;
  S.lastKnownBombMode = "";
  S.lastKnownBombMap = "";
  S.bombTimerSuppressUntil = 0;
  S.bombUi.active = false;
  S.bombUi.defusing = false;
  S.bombUi.bombLeft = 0;
  S.bombUi.bombTotal = 40;
  S.bombUi.defuseLeft = 0;
  S.bombUi.defuseTotal = 10;
  S.bombUi.lastUpdateAt = 0;
  if (S.bombMarker) {
    S.bombMarker.state.active = false;
  }
}

function estimateServerNow(clientNow) {
  if (!Number.isFinite(S.serverClockOffsetMs)) return null;
  return clientNow - S.serverClockOffsetMs;
}

function scheduleUiRender() {
  if (S.uiRenderTimer) return;
  S.uiRenderTimer = window.setTimeout(() => {
    S.uiRenderTimer = 0;
    render();
  }, UI_RENDER_INTERVAL_MS);
}

function resolveModelName(player) {
  const candidate = String(player.m_model_name || "").trim().toLowerCase();
  if (KNOWN_MODELS.has(candidate)) return candidate;
  return Number(player.m_team) === TEAM_CT ? "ctm_sas" : "tm_phoenix";
}

function resolveIconName(name, fallback) {
  const candidate = String(name || "").trim().toLowerCase();
  if (KNOWN_ICONS.has(candidate)) return candidate;
  return fallback;
}

const WEAPON_LABELS = {
  ak47: "AK-47",
  aug: "AUG",
  awp: "AWP",
  bizon: "Bizon",
  c4: "C4",
  cz75a: "CZ75-Auto",
  deagle: "Deagle",
  decoy: "Decoy",
  elite: "Dual Berettas",
  famas: "FAMAS",
  fiveseven: "Five-SeveN",
  flashbang: "Flashbang",
  g3sg1: "G3SG1",
  galilar: "Galil AR",
  gauge: "Sawed-Off",
  glock: "Glock",
  hegrenade: "Grenade",
  incgrenade: "Fire",
  knife: "Knife",
  m249: "M249",
  m4a1: "M4A4",
  m4a1_silencer: "M4A1-S",
  mac10: "MAC-10",
  mag7: "MAG-7",
  molotov: "Fire",
  mp5sd: "MP5-SD",
  mp7: "MP7",
  mp9: "MP9",
  negev: "Negev",
  nova: "Nova",
  p2000: "P2000",
  p250: "P250",
  p90: "P90",
  revolver: "R8 Revolver",
  sawedoff: "Sawed-Off",
  scar20: "SCAR-20",
  sg556: "SG 553",
  smokegrenade: "Smoke",
  ssg08: "SSG 08",
  taser: "Zeus x27",
  tec9: "Tec-9",
  ump45: "UMP-45",
  usp_silencer: "USP-S",
  xm1014: "XM1014"
};

function formatWeaponName(name) {
  const candidate = String(name || "").trim().toLowerCase();
  if (!candidate) return "";
  if (WEAPON_LABELS[candidate]) return WEAPON_LABELS[candidate];
  return candidate
    .replaceAll("_", " ")
    .split(" ")
    .filter(Boolean)
    .map(part => part.charAt(0).toUpperCase() + part.slice(1))
    .join(" ");
}

function normalizeDisplayText(value, fallback = "") {
  const text = String(value ?? "").replace(/\s+/g, " ").trim();
  return text || fallback;
}

function truncateDisplayText(value, maxChars) {
  const text = normalizeDisplayText(value);
  const chars = Array.from(text);
  if (chars.length <= maxChars) return text;
  return chars.slice(0, Math.max(1, maxChars - 3)).join("").trimEnd() + "...";
}

function setMaskedIcon(element, iconName, color) {
  const path = "./assets/icons/" + iconName + ".svg";
  element.style.backgroundColor = color || "var(--radar-secondary)";
  element.style.webkitMask = "url('" + path + "') no-repeat center / contain";
  element.style.mask = "url('" + path + "') no-repeat center / contain";
}

function createPlayerCard(isRightSide) {
  const card = document.createElement("article");
  card.className = "player-card" + (isRightSide ? " is-right" : "");
  card.innerHTML = [
    '<span class="player-card__status"></span>',
    '<img class="player-card__model" alt="" />',
    '<div class="player-card__content">',
    '  <div class="player-card__top">',
    '    <div class="player-card__identity">',
    '      <span class="player-card__name"></span>',
    '    </div>',
    '    <span class="player-card__money"></span>',
    '  </div>',
    '  <div class="player-card__stats">',
    '    <span class="player-card__stat"><span class="player-card__stat-icon"></span><span class="player-card__stat-value player-card__health"></span></span>',
    '    <span class="player-card__stat"><span class="player-card__stat-icon"></span><span class="player-card__stat-value player-card__armor"></span></span>',
    '  </div>',
    '  <div class="player-card__weapon">',
    '    <span class="player-card__weapon-icon"></span>',
    '    <span class="player-card__weapon-name"></span>',
    '  </div>',
    '  <div class="player-card__util"></div>',
    '</div>'
  ].join("");

  const statIcons = card.querySelectorAll(".player-card__stat-icon");
  return {
    root: card,
    status: card.querySelector(".player-card__status"),
    model: card.querySelector(".player-card__model"),
    name: card.querySelector(".player-card__name"),
    money: card.querySelector(".player-card__money"),
    healthIcon: statIcons[0],
    armorIcon: statIcons[1],
    healthValue: card.querySelector(".player-card__health"),
    armorValue: card.querySelector(".player-card__armor"),
    weaponIcon: card.querySelector(".player-card__weapon-icon"),
    weaponName: card.querySelector(".player-card__weapon-name"),
    util: card.querySelector(".player-card__util"),
    modelKey: "",
    weaponKey: "",
    utilKey: ""
  };
}

function updatePlayerCard(card, player, localTeam) {
  card.root.style.setProperty("--card-accent", getRosterAccent(player));
  card.root.classList.toggle("is-dead", !!player.m_is_dead);

  const displayName = normalizeDisplayText(player.m_name, "Player");
  card.name.textContent = truncateDisplayText(displayName, 22);
  card.name.title = displayName;
  card.money.textContent = "$" + Math.max(0, Number(player.m_money || 0));
  card.healthValue.textContent = String(Math.max(0, Number(player.m_health || 0)));
  card.armorValue.textContent = String(Math.max(0, Number(player.m_armor || 0)));

  const modelName = resolveModelName(player);
  if (card.modelKey !== modelName) {
    card.modelKey = modelName;
    card.model.src = "./assets/characters/" + modelName + ".png";
    card.model.alt = modelName;
  }

  setMaskedIcon(card.healthIcon, "health", "var(--radar-primary)");
  setMaskedIcon(card.armorIcon, "kevlar", "var(--radar-secondary)");

  const weaponName = resolveIconName(player.m_weapon, "knife");
  if (card.weaponKey !== weaponName) {
    card.weaponKey = weaponName;
    setMaskedIcon(card.weaponIcon, weaponName, "var(--radar-primary)");
  }
  function weaponUsesAmmo(weaponId, resolvedWeaponName) {
    const wid = Number(weaponId || 0);
    if (!Number.isFinite(wid) || wid <= 0) return false;
    if (wid === 42 || wid === 49 || wid === 59) return false;
    if (wid >= 43 && wid <= 48) return false;
    if (wid >= 500 && wid <= 525) return false;
    if (resolvedWeaponName === "knife" || resolvedWeaponName === "c4") return false;
    return true;
  }
  const ammo = Number(player.m_ammo_clip);
  const weaponLabel = formatWeaponName(weaponName);
  const showAmmo = weaponUsesAmmo(player.m_weapon_id, weaponName);
  card.weaponName.textContent = weaponLabel + (showAmmo && Number.isFinite(ammo) && ammo >= 0 ? " (" + ammo + ")" : "");

  const grenades = Array.isArray(player.m_grenades) ? player.m_grenades : [];
  const utilKey = [
    Number(!!player.m_has_defuser),
    Number(!!player.m_has_bomb),
    Number(!!player.m_is_dead),
    grenades.join(",")
  ].join(":");
  if (card.utilKey !== utilKey) {
    card.utilKey = utilKey;
    const fragment = document.createDocumentFragment();

    
    if (player.m_has_defuser) {
      const utilIcon = document.createElement("span");
      utilIcon.className = "player-card__util-icon";
      setMaskedIcon(utilIcon, "defuser", "var(--radar-green)");
      fragment.appendChild(utilIcon);
    }

    
    const nadeColor = "#c8ddef";
    for (let i = 0; i < 4; i += 1) {
      if (i < grenades.length) {
        const nade = grenades[i];
        const icon = document.createElement("span");
        icon.className = "player-card__util-icon";
        setMaskedIcon(icon, KNOWN_ICONS.has(nade) ? nade : "hegrenade", nadeColor);
        fragment.appendChild(icon);
      } else {
        const dot = document.createElement("span");
        dot.className = "player-card__util-dot";
        fragment.appendChild(dot);
      }
    }

    
    if (player.m_has_bomb && !player.m_is_dead) {
      const utilIcon = document.createElement("span");
      utilIcon.className = "player-card__util-icon";
      setMaskedIcon(utilIcon, "c4", "var(--radar-primary)");
      fragment.appendChild(utilIcon);
    }

    card.util.replaceChildren(fragment);
  }
}

function buildRosterSignature(players) {
  return players.map(player => [
    player.m_idx,
    Number(!!player.m_is_local),
    Number(!!player.m_is_dead),
    player.m_name,
    player.m_health,
    player.m_armor,
    player.m_money,
    player.m_weapon,
    player.m_ammo_clip,
    Array.isArray(player.m_grenades) ? player.m_grenades.join(",") : "",
    Number(!!player.m_has_defuser),
    Number(!!player.m_has_bomb),
    player.m_model_name
  ].join(":")).join("|");
}

function renderRoster(container, teamKey, players, localTeam, emptyText) {
  const signature = buildRosterSignature(players);
  if (S.rosterSignatures[teamKey] === signature) return;
  S.rosterSignatures[teamKey] = signature;

  if (!players.length) {
    const empty = document.createElement("div");
    empty.className = "team-panel__empty";
    empty.textContent = emptyText;
    container.replaceChildren(empty);
    return;
  }

  const cardCache = S.rosterCards[teamKey];
  const used = new Set();
  const fragment = document.createDocumentFragment();
  const sorted = [...players].sort((a, b) =>
    Number(!!a.m_is_dead) - Number(!!b.m_is_dead) ||
    Number(!!a.m_is_local) - Number(!!b.m_is_local) ||
    String(a.m_name || "").localeCompare(String(b.m_name || ""))
  );

  for (const player of sorted) {
    const key = String(player.m_idx);
    let card = cardCache.get(key);
    if (!card) {
      card = createPlayerCard(teamKey === "ct");
      cardCache.set(key, card);
    }
    updatePlayerCard(card, player, localTeam);
    fragment.appendChild(card.root);
    used.add(key);
  }

  for (const key of [...cardCache.keys()]) {
    if (!used.has(key)) {
      cardCache.delete(key);
    }
  }

  container.replaceChildren(fragment);
}

function createMotionState() {
  return {
    targetX: 0,
    targetY: 0,
    renderX: null,
    renderY: null,
    targetRotation: 0,
    renderRotation: 0,
    targetOpacity: 1,
    renderOpacity: 1,
    active: false,
    velocityX: 0,
    velocityY: 0,
    lastUpdateTime: 0
  };
}

function ensureMarker(key) {
  if (!S.markers.has(key)) {
    S.markers.set(key, {
      key,
      state: createMotionState(),
      label: "",
      color: "#ffffff",
      isLocal: false,
      isDead: false,
      hasBomb: false,
      showLabel: false,
      showCone: false
    });
  }
  return S.markers.get(key);
}

function ensureBombMarker() {
  if (!S.bombMarker) {
    S.bombMarker = {
      state: createMotionState(),
      size: 12,
      color: "#d88a32",
      isDropped: false,
      isPlanted: false,
      isDefused: false
    };
  }
  return S.bombMarker;
}

function calculatePlayerRotation(player) {
  const playerViewAngle = 90 - Number(player.m_eye_angle || 0);
  const idx = String(player.m_idx);
  const previous = Number(S.rotations.get(idx) || playerViewAngle);
  const next = previous + shortestAngleDelta(previous, playerViewAngle);
  S.rotations.set(idx, next);
  return next;
}

function lerp(a, b, t) {
  return a + (b - a) * t;
}

function hasFiniteVec3(vec) {
  return !!vec &&
    Number.isFinite(Number(vec.x)) &&
    Number.isFinite(Number(vec.y)) &&
    Number.isFinite(Number(vec.z));
}

function cloneVec3(vec) {
  return {
    x: Number(vec?.x || 0),
    y: Number(vec?.y || 0),
    z: Number(vec?.z || 0)
  };
}

function addVelocity(vec, velocity, dtSec) {
  return {
    x: vec.x + velocity.x * dtSec,
    y: vec.y + velocity.y * dtSec,
    z: vec.z + velocity.z * dtSec
  };
}

function deriveVelocity(fromVec, toVec, dtSec) {
  if (!hasFiniteVec3(fromVec) || !hasFiniteVec3(toVec) || !Number.isFinite(dtSec) || dtSec <= 0) {
    return { x: 0, y: 0, z: 0 };
  }
  return {
    x: (toVec.x - fromVec.x) / dtSec,
    y: (toVec.y - fromVec.y) / dtSec,
    z: (toVec.z - fromVec.z) / dtSec
  };
}

function getPlayerVelocity(player, fallbackFrom, fallbackTo, dtSec) {
  const velocity = player?.m_velocity;
  if (velocity && Number.isFinite(Number(velocity.x)) && Number.isFinite(Number(velocity.y))) {
    return {
      x: Number(velocity.x || 0),
      y: Number(velocity.y || 0),
      z: Number(velocity.z || 0)
    };
  }
  return deriveVelocity(fallbackFrom, fallbackTo, dtSec);
}

function hermiteInterpolateVec3(fromVec, toVec, fromVelocity, toVelocity, t, dtSec) {
  const tt = clamp(t, 0, 1);
  const t2 = tt * tt;
  const t3 = t2 * tt;
  const h00 = 2 * t3 - 3 * t2 + 1;
  const h10 = t3 - 2 * t2 + tt;
  const h01 = -2 * t3 + 3 * t2;
  const h11 = t3 - t2;
  const m0 = dtSec;
  const m1 = dtSec;

  return {
    x: h00 * fromVec.x + h10 * fromVelocity.x * m0 + h01 * toVec.x + h11 * toVelocity.x * m1,
    y: h00 * fromVec.y + h10 * fromVelocity.y * m0 + h01 * toVec.y + h11 * toVelocity.y * m1,
    z: h00 * fromVec.z + h10 * fromVelocity.z * m0 + h01 * toVec.z + h11 * toVelocity.z * m1
  };
}

function buildPlayerIndex(players) {
  const index = new Map();
  for (let i = 0; i < players.length; ++i) {
    const player = players[i];
    index.set(String(player.m_idx), player);
  }
  return index;
}

function samplePlayerState(prevPlayer, nextPlayer, alpha, dtMs, extrapolationMs) {
  const latest = nextPlayer || prevPlayer;
  if (!latest) return null;

  const prevPos = hasFiniteVec3(prevPlayer?.m_position) ? cloneVec3(prevPlayer.m_position) : null;
  const nextPos = hasFiniteVec3(nextPlayer?.m_position) ? cloneVec3(nextPlayer.m_position) : (prevPos ? cloneVec3(prevPos) : null);
  if (!nextPos && !prevPos) return null;

  const dtSec = Math.max(0.001, dtMs / 1000);
  const chosen = alpha >= 0.5 ? (nextPlayer || prevPlayer) : (prevPlayer || nextPlayer);
  let position = nextPos ? cloneVec3(nextPos) : cloneVec3(prevPos);
  let rotation = 90 - Number(latest.m_eye_angle || 0);

  if (prevPlayer && nextPlayer && prevPos && nextPos) {
    const fromVelocity = getPlayerVelocity(prevPlayer, prevPos, nextPos, dtSec);
    const toVelocity = getPlayerVelocity(nextPlayer, prevPos, nextPos, dtSec);
    position = hermiteInterpolateVec3(prevPos, nextPos, fromVelocity, toVelocity, alpha, dtSec);

    const prevRotation = 90 - Number(prevPlayer.m_eye_angle || 0);
    const nextRotation = 90 - Number(nextPlayer.m_eye_angle || 0);
    rotation = prevRotation + shortestAngleDelta(prevRotation, nextRotation) * alpha;

    if (extrapolationMs > 0 && !chosen?.m_is_dead) {
      const extrapolationSec = Math.min(extrapolationMs, 28) / 1000;
      position = addVelocity(position, toVelocity, extrapolationSec);
      rotation += (shortestAngleDelta(prevRotation, nextRotation) / dtSec) * extrapolationSec;
    }
  } else if (extrapolationMs > 0 && !chosen?.m_is_dead) {
    const extrapolationSec = Math.min(extrapolationMs, 24) / 1000;
    position = addVelocity(
      position,
      getPlayerVelocity(latest, prevPos || nextPos, nextPos || prevPos, dtSec),
      extrapolationSec
    );
  }

  return {
    ...chosen,
    m_position: position,
    m_eye_angle: 90 - rotation,
    _wrRotation: rotation
  };
}

function sampleBombState(prevBomb, nextBomb, alpha, dtMs, extrapolationMs) {
  const latest = nextBomb || prevBomb;
  if (!latest) return null;

  const chosen = alpha >= 0.5 ? (nextBomb || prevBomb) : (prevBomb || nextBomb);
  if (!chosen) return null;
  const elapsedSec = Math.max(0, Number(extrapolationMs || 0)) / 1000;
  const adjustTimer = value => Math.max(0, Number(value || 0) - elapsedSec);

  const prevPos = hasFiniteVec3(prevBomb?.m_position) ? cloneVec3(prevBomb.m_position) : null;
  const nextPos = hasFiniteVec3(nextBomb?.m_position) ? cloneVec3(nextBomb.m_position) : prevPos;
  if (!prevBomb || !nextBomb || !prevPos || !nextPos) {
    return {
      ...chosen,
      m_blow_time: adjustTimer(chosen.m_blow_time),
      m_defuse_time: adjustTimer(chosen.m_defuse_time),
      m_timer_length: Number(chosen.m_timer_length || 40),
      m_defuse_length: Number(chosen.m_defuse_length || 10),
      m_position: nextPos || prevPos || { x: 0, y: 0, z: 0 }
    };
  }

  const sameMode =
    !!prevBomb.m_is_planted === !!nextBomb.m_is_planted &&
    !!prevBomb.m_is_dropped === !!nextBomb.m_is_dropped;
  if (!sameMode) {
    return {
      ...chosen,
      m_blow_time: adjustTimer(chosen.m_blow_time),
      m_defuse_time: adjustTimer(chosen.m_defuse_time),
      m_timer_length: Number(chosen.m_timer_length || 40),
      m_defuse_length: Number(chosen.m_defuse_length || 10),
      m_position: nextPos
    };
  }

  const t = clamp(alpha, 0, 1);
  let position = {
    x: lerp(prevPos.x, nextPos.x, t),
    y: lerp(prevPos.y, nextPos.y, t),
    z: lerp(prevPos.z, nextPos.z, t)
  };

  if (extrapolationMs > 0 && nextBomb.m_is_dropped) {
    const dtSec = Math.max(0.001, dtMs / 1000);
    const extrapolationSec = Math.min(extrapolationMs, 24) / 1000;
    position = addVelocity(position, deriveVelocity(prevPos, nextPos, dtSec), extrapolationSec);
  }

  return {
    ...chosen,
    m_blow_time: adjustTimer(lerp(Number(prevBomb.m_blow_time || 0), Number(nextBomb.m_blow_time || 0), t)),
    m_defuse_time: adjustTimer(lerp(Number(prevBomb.m_defuse_time || 0), Number(nextBomb.m_defuse_time || 0), t)),
    m_timer_length: Number(nextBomb.m_timer_length || prevBomb.m_timer_length || 40),
    m_defuse_length: Number(nextBomb.m_defuse_length || prevBomb.m_defuse_length || 10),
    m_position: position
  };
}

function buildSampledPayload(prevPayload, nextPayload, alpha, renderTime, dtMs, extrapolationMs) {
  const latest = nextPayload || prevPayload;
  if (!latest) return null;

  if (!prevPayload || !nextPayload || prevPayload.m_map !== nextPayload.m_map) {
    return {
      ...latest,
      m_server_time: renderTime,
      m_capture_time: renderTime
    };
  }

  const prevIndex = buildPlayerIndex(prevPayload.m_players || []);
  const nextIndex = buildPlayerIndex(nextPayload.m_players || []);
  const keys = new Set([...prevIndex.keys(), ...nextIndex.keys()]);
  const sampledPlayers = [];
  for (const key of keys) {
    const sampled = samplePlayerState(prevIndex.get(key), nextIndex.get(key), alpha, dtMs, extrapolationMs);
    if (sampled) sampledPlayers.push(sampled);
  }
  sampledPlayers.sort((a, b) => Number(a.m_idx) - Number(b.m_idx));

  return {
    ...latest,
    m_players: sampledPlayers,
    m_bomb: sampleBombState(prevPayload.m_bomb, nextPayload.m_bomb, alpha, dtMs, extrapolationMs),
    m_server_time: renderTime,
    m_capture_time: renderTime
  };
}

function sampleRenderPayload(clientNow) {
  if (!S.snapshots.length) return S.payload;

  const estimatedServerNow = estimateServerNow(clientNow);
  if (!Number.isFinite(estimatedServerNow)) {
    return S.snapshots[S.snapshots.length - 1].payload;
  }

  const renderTime = estimatedServerNow - S.renderDelayMs;
  if (S.snapshots.length === 1) {
    const only = S.snapshots[0];
    const extrapolationMs = clamp(renderTime - only.sampleTime, 0, 1000);
    return buildSampledPayload(only.payload, only.payload, 1, renderTime, S.frameGapMs, extrapolationMs);
  }
  const snapshots = S.snapshots;

  if (renderTime <= snapshots[0].sampleTime) {
    return buildSampledPayload(snapshots[0].payload, snapshots[0].payload, 0, renderTime, S.frameGapMs, 0);
  }

  for (let i = 1; i < snapshots.length; ++i) {
    const older = snapshots[i - 1];
    const newer = snapshots[i];
    if (renderTime <= newer.sampleTime) {
      const dtMs = Math.max(1, newer.sampleTime - older.sampleTime);
      const alpha = clamp((renderTime - older.sampleTime) / dtMs, 0, 1);
      return buildSampledPayload(older.payload, newer.payload, alpha, renderTime, dtMs, 0);
    }
  }

  const older = snapshots[snapshots.length - 2];
  const newer = snapshots[snapshots.length - 1];
  const dtMs = Math.max(1, newer.sampleTime - older.sampleTime);
  const extrapolationMs = clamp(renderTime - newer.sampleTime, 0, 28);
  return buildSampledPayload(older.payload, newer.payload, 1, renderTime, dtMs, extrapolationMs);
}

function layoutMapViewport() {
  if (!el.mapShell || !el.mapImage || !el.radarStage) return;

  const stageWidth = el.radarStage.clientWidth;
  const stageHeight = el.radarStage.clientHeight;
  if (!stageWidth || !stageHeight) return;

  const inset = IS_ONLY_RADAR ? 0 : 24;
  const size = Math.max(1, Math.floor(Math.min(stageWidth - inset * 2, stageHeight - inset * 2)));

  el.mapShell.style.width = size + "px";
  el.mapShell.style.height = size + "px";
  el.mapImage.style.width = size + "px";
  el.mapImage.style.height = size + "px";
}

function refreshOverlayMetrics() {
  if (S.layoutDirty) {
    layoutMapViewport();
    S.layoutDirty = false;
  }
  if (!el.radarOverlay || !el.mapImage.clientWidth || !el.mapImage.clientHeight) {
    S.cachedOverlay = null;
    return null;
  }
  if (!S.overlayContext) {
    S.overlayContext = el.radarOverlay.getContext("2d", { alpha: true });
  }

  const width = el.mapImage.clientWidth;
  const height = el.mapImage.clientHeight;
  const dpr = Math.max(1, window.devicePixelRatio || 1);
  const pixelWidth = Math.max(1, Math.round(width * dpr));
  const pixelHeight = Math.max(1, Math.round(height * dpr));

  if (el.radarOverlay.width !== pixelWidth || el.radarOverlay.height !== pixelHeight) {
    el.radarOverlay.width = pixelWidth;
    el.radarOverlay.height = pixelHeight;
    el.radarOverlay.style.width = width + "px";
    el.radarOverlay.style.height = height + "px";
  }

  S.cachedOverlay = { ctx: S.overlayContext, width, height, dpr };
  return S.cachedOverlay;
}

function getOverlayMetrics() {
  if (S.cachedOverlay && !S.layoutDirty) return S.cachedOverlay;
  return refreshOverlayMetrics();
}

function clearOverlay() {
  const m = getOverlayMetrics();
  if (!m) return null;
  m.ctx.setTransform(m.dpr, 0, 0, m.dpr, 0, 0);
  m.ctx.clearRect(0, 0, m.width, m.height);
  return m;
}

function drawRoundedRect(ctx, x, y, width, height, radius) {
  const r = Math.min(radius, width * 0.5, height * 0.5);
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.arcTo(x + width, y, x + width, y + height, r);
  ctx.arcTo(x + width, y + height, x, y + height, r);
  ctx.arcTo(x, y + height, x, y, r);
  ctx.arcTo(x, y, x + width, y, r);
  ctx.closePath();
}

function drawMarkerCone(ctx, entry, markerSize) {
  if (!entry.showCone || entry.isDead || entry.state.renderX === null || entry.state.renderY === null) return;

  const originX = entry.state.renderX + markerSize * 0.5;
  const originY = entry.state.renderY + markerSize * 0.48;
  const noseY = -markerSize * 0.38;
  const length = markerSize * 3.1;
  const spread = markerSize * 0.82;

  ctx.save();
  ctx.translate(originX, originY);
  ctx.rotate(entry.state.renderRotation * Math.PI / 180);
  ctx.globalAlpha = S.settings.viewConeOpacity * entry.state.renderOpacity;
  ctx.fillStyle = entry.color;
  ctx.beginPath();
  ctx.moveTo(0, noseY);
  ctx.lineTo(-spread, noseY - length);
  ctx.lineTo(spread, noseY - length);
  ctx.closePath();
  ctx.fill();
  ctx.restore();
}

function drawPlayerMarker(ctx, entry, markerSize) {
  if (!entry.state.active || entry.state.renderX === null || entry.state.renderY === null) return;

  const centerX = entry.state.renderX + markerSize * 0.5;
  const centerY = entry.state.renderY + markerSize * 0.5;
  const bodySize = markerSize * 0.54;
  const half = bodySize * 0.5;
  const radius = bodySize * 0.5;

  ctx.save();
  ctx.translate(centerX, centerY);
  ctx.globalAlpha = entry.state.renderOpacity;

  if (entry.isDead) {
    const crossSize = markerSize * 0.18;
    ctx.lineWidth = 1.5;
    ctx.lineCap = "round";
    ctx.strokeStyle = "rgba(255, 255, 255, 0.9)";
    ctx.beginPath();
    ctx.moveTo(-crossSize, -crossSize);
    ctx.lineTo(crossSize, crossSize);
    ctx.moveTo(crossSize, -crossSize);
    ctx.lineTo(-crossSize, crossSize);
    ctx.stroke();
    ctx.restore();
    return;
  }

  ctx.rotate((entry.state.renderRotation + 135) * Math.PI / 180);

  if (entry.hasBomb && !entry.isDead) {
    ctx.beginPath();
    ctx.setLineDash([3, 2]);
    ctx.lineWidth = 1.25;
    ctx.strokeStyle = "rgba(255, 176, 72, 0.95)";
    ctx.arc(0, 0, markerSize * 0.46, 0, Math.PI * 2);
    ctx.stroke();
    ctx.setLineDash([]);
  }

  ctx.beginPath();
  ctx.moveTo(-half + radius, -half);
  ctx.lineTo(half - radius, -half);
  ctx.quadraticCurveTo(half, -half, half, -half + radius);
  ctx.lineTo(half, half - radius);
  ctx.quadraticCurveTo(half, half, half - radius, half);
  ctx.lineTo(-half, half);
  ctx.lineTo(-half, -half + radius);
  ctx.quadraticCurveTo(-half, -half, -half + radius, -half);
  ctx.closePath();
  ctx.fillStyle = entry.color;
  ctx.fill();

  ctx.lineWidth = 1.05;
  ctx.strokeStyle = "rgba(7, 18, 29, 0.95)";
  ctx.stroke();

  ctx.restore();
}

function rectsOverlap(a, b, padding = 0) {
  return a.x < b.x + b.width + padding &&
    a.x + a.width > b.x - padding &&
    a.y < b.y + b.height + padding &&
    a.y + a.height > b.y - padding;
}

function roundRectPath(ctx, x, y, width, height, radius) {
  const r = Math.min(radius, width * 0.5, height * 0.5);
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.arcTo(x + width, y, x + width, y + height, r);
  ctx.arcTo(x + width, y + height, x, y + height, r);
  ctx.arcTo(x, y + height, x, y, r);
  ctx.arcTo(x, y, x + width, y, r);
  ctx.closePath();
}

function colorWithAlpha(color, alpha) {
  const candidate = String(color || "").trim();
  const hex = candidate.startsWith("#") ? candidate.slice(1) : candidate;
  if (hex.length === 6) {
    const r = parseInt(hex.slice(0, 2), 16);
    const g = parseInt(hex.slice(2, 4), 16);
    const b = parseInt(hex.slice(4, 6), 16);
    if (Number.isFinite(r) && Number.isFinite(g) && Number.isFinite(b)) {
      return "rgba(" + r + ", " + g + ", " + b + ", " + alpha + ")";
    }
  }
  return color;
}

function measureLabelText(ctx, text) {
  const key = S.cachedFontStr + "|" + text;
  let textWidth = S.labelMeasureCache.get(key);
  if (textWidth === undefined) {
    textWidth = ctx.measureText(text).width;
    S.labelMeasureCache.set(key, textWidth);
  }
  return textWidth;
}

function fitLabelText(ctx, rawText, maxWidth) {
  const text = String(rawText || "").trim();
  if (!text) return "";
  if (measureLabelText(ctx, text) <= maxWidth) return text;

  let clipped = text;
  while (clipped.length > 1) {
    const next = clipped.trimEnd() + "…";
    if (measureLabelText(ctx, next) <= maxWidth) {
      return next;
    }
    clipped = clipped.slice(0, -1);
  }
  return text.charAt(0);
}

function fitLabelTextSafe(ctx, rawText, maxWidth) {
  const text = normalizeDisplayText(rawText);
  if (!text) return "";
  if (measureLabelText(ctx, text) <= maxWidth) return text;

  const clipped = Array.from(text);
  while (clipped.length > 1) {
    const next = clipped.join("").trimEnd() + "...";
    if (measureLabelText(ctx, next) <= maxWidth) {
      return next;
    }
    clipped.pop();
  }
  return text.charAt(0);
}

function buildLabelLayout(ctx, markers, markerSize, overlayWidth, overlayHeight) {
  const fontSize = clamp(markerSize * 0.58, 8, 11);
  const fontStr = "600 " + fontSize + "px TASAOrbiter, Segoe UI, sans-serif";
  if (S.cachedFontStr !== fontStr) {
    S.cachedFontStr = fontStr;
    S.labelMeasureCache.clear();
  }
  ctx.font = fontStr;

  const result = S._labelBuf || (S._labelBuf = []);
  result.length = 0;
  const placedRects = S._labelRectBuf || (S._labelRectBuf = []);
  placedRects.length = 0;

  const markerRects = [];
  for (let i = 0; i < markers.length; ++i) {
    const entry = markers[i];
    if (!entry.state.active || entry.state.renderX === null || entry.state.renderY === null) continue;
    markerRects.push({
      key: String(entry.key || i),
      x: entry.state.renderX - 2,
      y: entry.state.renderY - 2,
      width: markerSize + 4,
      height: markerSize + 4
    });
  }

  for (let i = 0; i < markers.length; ++i) {
    const entry = markers[i];
    if (!entry.showLabel || entry.isDead || entry.state.renderX === null || entry.state.renderY === null) continue;

    const centerX = entry.state.renderX + markerSize * 0.5;
    const centerY = entry.state.renderY + markerSize * 0.5;
    const maxTextWidth = clamp(overlayWidth * 0.18, 54, 122);
    const text = fitLabelTextSafe(ctx, entry.label, maxTextWidth);
    if (!text) continue;

    const textWidth = measureLabelText(ctx, text);
    const padX = Math.round(clamp(fontSize * 0.6, 5, 8));
    const boxHeight = Math.round(fontSize + 8);
    const boxWidth = Math.round(textWidth + padX * 2);
    const gap = Math.round(markerSize * 0.38 + 5);
    const key = String(entry.key || i);

    const candidates = [
      { x: centerX - boxWidth * 0.5, y: centerY - markerSize * 0.78 - boxHeight - 2 },
      { x: centerX + gap, y: centerY - boxHeight * 0.5 - 1 },
      { x: centerX - gap - boxWidth, y: centerY - boxHeight * 0.5 - 1 },
      { x: centerX - boxWidth * 0.5, y: centerY + markerSize * 0.42 + 4 },
      { x: centerX + gap * 0.7, y: centerY - markerSize * 0.68 - boxHeight },
      { x: centerX - gap * 0.7 - boxWidth, y: centerY - markerSize * 0.68 - boxHeight }
    ];

    let chosenRect = null;
    let fallbackRect = null;
    for (let c = 0; c < candidates.length; ++c) {
      const candidate = candidates[c];
      const rect = {
        x: clamp(candidate.x, 4, overlayWidth - boxWidth - 4),
        y: clamp(candidate.y, 4, overlayHeight - boxHeight - 4),
        width: boxWidth,
        height: boxHeight
      };
      if (!fallbackRect) fallbackRect = rect;

      let blocked = false;
      for (let m = 0; m < markerRects.length; ++m) {
        const markerRect = markerRects[m];
        if (markerRect.key === key) continue;
        if (rectsOverlap(rect, markerRect, 5)) {
          blocked = true;
          break;
        }
      }
      if (blocked) continue;

      for (let r = 0; r < placedRects.length; ++r) {
        if (rectsOverlap(rect, placedRects[r], 4)) {
          blocked = true;
          break;
        }
      }
      if (!blocked) {
        chosenRect = rect;
        break;
      }
    }

    const rect = chosenRect || fallbackRect;
    if (!rect) continue;
    placedRects.push(rect);

    result.push({
      text,
      textColor: entry.color === "#84c8ed" ? "rgba(238, 248, 255, 0.96)" : "rgba(255, 239, 239, 0.97)",
      fillColor: "rgba(6, 16, 24, 0.86)",
      strokeColor: colorWithAlpha(entry.color, 0.44),
      x: rect.x,
      y: rect.y,
      width: rect.width,
      height: rect.height,
      radius: Math.round(boxHeight * 0.46),
      textX: rect.x + rect.width * 0.5,
      textY: rect.y + rect.height * 0.54
    });
  }
  return result;
}

function drawLabels(ctx, labels) {
  if (!labels.length) return;

  ctx.save();
  ctx.font = S.cachedFontStr;
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";

  for (let i = 0; i < labels.length; ++i) {
    const label = labels[i];
    roundRectPath(ctx, label.x, label.y, label.width, label.height, label.radius);
    ctx.fillStyle = label.fillColor;
    ctx.fill();
    ctx.lineWidth = 1;
    ctx.strokeStyle = label.strokeColor;
    ctx.stroke();

    ctx.shadowColor = "rgba(0, 0, 0, 0.35)";
    ctx.shadowBlur = 3;
    ctx.fillStyle = label.textColor;
    ctx.fillText(label.text, label.textX, label.textY);
    ctx.shadowBlur = 0;
  }

  ctx.restore();
}

function drawC4Icon(ctx, centerX, centerY, size) {
  const iconSize = Math.max(8, size);
  const x = centerX - iconSize * 0.5;
  const y = centerY - iconSize * 0.5;

  if (C4_ICON.complete && C4_ICON.naturalWidth > 0) {
    ctx.drawImage(C4_ICON, x, y, iconSize, iconSize);
    return;
  }

  ctx.save();
  ctx.translate(centerX, centerY);
  ctx.strokeStyle = "rgba(255, 242, 220, 0.95)";
  ctx.fillStyle = "rgba(255, 242, 220, 0.95)";
  ctx.lineWidth = Math.max(1, iconSize * 0.08);
  const bodyW = iconSize * 0.46;
  const bodyH = iconSize * 0.56;
  drawRoundedRect(ctx, -bodyW * 0.5, -bodyH * 0.25, bodyW, bodyH, iconSize * 0.08);
  ctx.stroke();
  ctx.fillRect(-bodyW * 0.22, -bodyH * 0.45, bodyW * 0.44, iconSize * 0.07);
  ctx.beginPath();
  ctx.moveTo(bodyW * 0.16, -bodyH * 0.44);
  ctx.lineTo(bodyW * 0.48, -bodyH * 0.68);
  ctx.lineTo(bodyW * 0.62, -bodyH * 0.62);
  ctx.stroke();
  const cell = bodyW * 0.16;
  for (let row = 0; row < 3; row += 1) {
    for (let col = 0; col < 3; col += 1) {
      ctx.fillRect(
        -cell * 1.45 + col * cell * 1.45,
        -cell * 0.45 + row * cell * 1.35,
        cell,
        cell);
    }
  }
  ctx.restore();
}

function drawBombMarker(ctx, bombMarker) {
  if (!bombMarker || !bombMarker.state.active || bombMarker.state.renderX === null || bombMarker.state.renderY === null) return;

  const size = bombMarker.size;
  const x = bombMarker.state.renderX;
  const y = bombMarker.state.renderY;
  const centerX = x + size * 0.5;
  const centerY = y + size * 0.5;

  ctx.save();
  ctx.globalAlpha = bombMarker.state.renderOpacity;

  if (bombMarker.isDropped) {
    const glowRadius = size * 1.08;
    const ringRadius = size * 0.86;
    const coreRadius = size * 0.66;

    ctx.beginPath();
    ctx.fillStyle = "rgba(255, 167, 70, 0.16)";
    ctx.arc(centerX, centerY, glowRadius, 0, Math.PI * 2);
    ctx.fill();

    ctx.beginPath();
    ctx.arc(centerX, centerY, coreRadius, 0, Math.PI * 2);
    ctx.fillStyle = "rgba(8, 16, 24, 0.96)";
    ctx.fill();

    ctx.beginPath();
    ctx.arc(centerX, centerY, ringRadius, 0, Math.PI * 2);
    ctx.lineWidth = Math.max(1.6, size * 0.12);
    ctx.strokeStyle = "rgba(255, 176, 82, 0.96)";
    ctx.stroke();

    ctx.beginPath();
    ctx.arc(centerX, centerY, coreRadius, 0, Math.PI * 2);
    ctx.lineWidth = 1;
    ctx.strokeStyle = "rgba(255, 189, 102, 0.22)";
    ctx.stroke();

    drawC4Icon(ctx, centerX, centerY, Math.max(10, size * 0.92));
  } else {
    ctx.beginPath();
    ctx.strokeStyle = bombMarker.isDefused ? "rgba(97, 215, 132, 0.65)" : "rgba(239, 115, 115, 0.55)";
    ctx.lineWidth = 2;
    ctx.arc(centerX, centerY, size * 0.92, 0, Math.PI * 2);
    ctx.stroke();

    ctx.beginPath();
    ctx.fillStyle = bombMarker.isDefused ? "rgba(97, 215, 132, 0.92)" : "rgba(239, 115, 115, 0.96)";
    ctx.arc(centerX, centerY, size * 0.62, 0, Math.PI * 2);
    ctx.fill();
    drawC4Icon(ctx, centerX, centerY, Math.max(10, size * 0.86));
  }
  ctx.restore();
}

const GRENADE_COLORS = {
  smoke:     { fill: "rgba(140, 180, 220, 0.55)", stroke: "rgba(180, 210, 240, 0.85)", label: "Smoke" },
  inferno:   { fill: "rgba(255, 120, 40, 0.45)",  stroke: "rgba(255, 160, 60, 0.85)",  label: "Fire" },
  decoy:     { fill: "rgba(200, 200, 80, 0.45)",  stroke: "rgba(220, 220, 100, 0.85)", label: "Decoy" },
  explosive: { fill: "rgba(255, 80, 80, 0.45)",   stroke: "rgba(255, 120, 100, 0.85)", label: "Grenade" },
};

function drawWorldGrenades(ctx, payload, mapData, overlayWidth, overlayHeight) {
  const grenades = payload && Array.isArray(payload.m_world_grenades) ? payload.m_world_grenades : [];
  if (grenades.length === 0) return;

  for (let i = 0; i < grenades.length; ++i) {
    const g = grenades[i];
    const colors = GRENADE_COLORS[g.type];
    if (!colors) continue;

    const pos = getRadarPosition(mapData, g.position);
    if (pos.invalid || pos.x < -0.05 || pos.x > 1.05 || pos.y < -0.05 || pos.y > 1.05) continue;

    const px = overlayWidth * clamp(pos.x, 0, 1);
    const py = overlayHeight * clamp(pos.y, 0, 1);

    
    if (g.type === "smoke") {
      const smokeRadius = Math.max(12, overlayWidth * 0.028);
      ctx.globalAlpha = 0.5;
      ctx.beginPath();
      ctx.arc(px, py, smokeRadius, 0, Math.PI * 2);
      ctx.fillStyle = colors.fill;
      ctx.fill();
      ctx.strokeStyle = colors.stroke;
      ctx.lineWidth = 1.5;
      ctx.stroke();
      ctx.globalAlpha = 1.0;
    }
    
    else if (g.type === "inferno") {
      const fireRadius = Math.max(10, overlayWidth * 0.022);
      ctx.globalAlpha = 0.55;
      ctx.beginPath();
      ctx.arc(px, py, fireRadius, 0, Math.PI * 2);
      ctx.fillStyle = colors.fill;
      ctx.fill();
      ctx.strokeStyle = colors.stroke;
      ctx.lineWidth = 1.5;
      ctx.stroke();
      ctx.globalAlpha = 1.0;
    }
    
    else {
      const sz = Math.max(5, overlayWidth * 0.008);
      ctx.globalAlpha = 0.85;
      ctx.beginPath();
      ctx.moveTo(px, py - sz);
      ctx.lineTo(px + sz, py);
      ctx.lineTo(px, py + sz);
      ctx.lineTo(px - sz, py);
      ctx.closePath();
      ctx.fillStyle = colors.fill;
      ctx.fill();
      ctx.strokeStyle = colors.stroke;
      ctx.lineWidth = 1.2;
      ctx.stroke();
      ctx.globalAlpha = 1.0;
    }

    
    const life = Number(g.life || 0);
    if (life > 0.1) {
      const label = colors.label + " " + life.toFixed(1) + "s";
      ctx.font = "bold 10px sans-serif";
      ctx.fillStyle = "rgba(255, 255, 255, 0.9)";
      ctx.strokeStyle = "rgba(0, 0, 0, 0.7)";
      ctx.lineWidth = 2.5;
      ctx.strokeText(label, px + 8, py + 4);
      ctx.fillText(label, px + 8, py + 4);
    }
  }
}

function drawOverlayCanvas() {
  const metrics = clearOverlay();
  if (!metrics) return;

  const payload = S.renderPayload || S.payload;
  const mapData = payload && S.mapDataCache.get(payload.m_map);
  if (!payload || !mapData || !el.mapImage.complete) {
    return;
  }

  const ctx = metrics.ctx;
  const overlayWidth = metrics.width;
  const overlayHeight = metrics.height;
  const markerSize = getMarkerSize(overlayWidth);

  
  const visibleMarkers = S._visibleBuf || (S._visibleBuf = []);
  visibleMarkers.length = 0;
  for (const entry of S.markers.values()) {
    if (entry.state.active && entry.state.renderX !== null) {
      visibleMarkers.push(entry);
    }
  }

  
  drawWorldGrenades(ctx, payload, mapData, overlayWidth, overlayHeight);

  if (S.bombMarker && S.bombMarker.state.active && S.bombMarker.isDropped) {
    drawBombMarker(ctx, S.bombMarker);
  }

  for (let i = 0; i < visibleMarkers.length; ++i) {
    drawMarkerCone(ctx, visibleMarkers[i], markerSize);
  }

  for (let i = 0; i < visibleMarkers.length; ++i) {
    drawPlayerMarker(ctx, visibleMarkers[i], markerSize);
  }

  drawLabels(ctx, buildLabelLayout(ctx, visibleMarkers, markerSize, overlayWidth, overlayHeight));

  if (S.bombMarker && S.bombMarker.state.active && !S.bombMarker.isDropped) {
    drawBombMarker(ctx, S.bombMarker);
  }
}

function updateMarker(entry, player, mapData, localTeam, markerSize, overlayWidth, overlayHeight) {
  const currentPosition = getRadarPosition(mapData, player.m_position);
  const inBounds = !currentPosition.invalid &&
    currentPosition.x >= -0.05 && currentPosition.x <= 1.05 &&
    currentPosition.y >= -0.05 && currentPosition.y <= 1.05;

  if (inBounds) {
    S.lastKnownPositions.set(player.m_idx, {
      x: clamp(currentPosition.x, 0, 1),
      y: clamp(currentPosition.y, 0, 1)
    });
  }

  const effectivePosition = player.m_is_dead
    ? (S.lastKnownPositions.get(player.m_idx) || currentPosition)
    : currentPosition;

  if (!effectivePosition || effectivePosition.invalid) {
    entry.state.active = false;
    return;
  }

  const x = overlayWidth * clamp(effectivePosition.x, 0, 1) - markerSize * 0.5;
  const y = overlayHeight * clamp(effectivePosition.y, 0, 1) - markerSize * 0.5;
  const color = getPlayerColor(player, localTeam);
  const rotation = Number.isFinite(player?._wrRotation)
    ? Number(player._wrRotation)
    : (90 - Number(player.m_eye_angle || 0));
  const label = normalizeDisplayText(player.m_name);

  entry.label = label;
  entry.color = color;
  entry.isLocal = !!player.m_is_local;
  entry.isDead = !!player.m_is_dead;
  entry.hasBomb = !!player.m_has_bomb && !player.m_is_dead;
  entry.showLabel = !player.m_is_local && !!label && shouldShowName(player, localTeam);
  entry.showCone = S.settings.showViewCones && !player.m_is_dead;

  entry.state.active = true;
  entry.state.targetX = x;
  entry.state.targetY = y;
  entry.state.targetRotation = rotation;
  entry.state.targetOpacity = player.m_is_dead ? 0.84 : 1;
  entry.state.renderX = x;
  entry.state.renderY = y;
  entry.state.renderRotation = rotation;
  entry.state.renderOpacity = entry.state.targetOpacity;
  entry.state.lastUpdateTime = performance.now();
  entry.state.velocityX = 0;
  entry.state.velocityY = 0;
}

function hideUnusedMarkers(usedKeys) {
  for (const [key, entry] of S.markers.entries()) {
    if (usedKeys.has(key)) continue;
    entry.state.active = false;
  }
}

function updateBombMarker(bomb, mapData, overlayWidth, overlayHeight, mapName) {
  const marker = ensureBombMarker();
  const isActive = !!bomb && (!!bomb.m_is_planted || !!bomb.m_is_dropped);
  const position = bomb ? getRadarPosition(mapData, bomb.m_position) : { invalid: true };
  const validPosition = !position.invalid &&
    position.x >= -0.05 && position.x <= 1.05 &&
    position.y >= -0.05 && position.y <= 1.05;
  const isDropped = !!bomb?.m_is_dropped && !bomb?.m_is_planted;
  const modeKey = isDropped ? "dropped" : (!!bomb?.m_is_planted ? "planted" : "inactive");
  const nowMs = performance.now();

  if (validPosition) {
    S.lastKnownBombPosition = {
      x: clamp(position.x, 0, 1),
      y: clamp(position.y, 0, 1)
    };
    S.lastKnownBombPositionAt = nowMs;
    S.lastKnownBombMode = modeKey;
    S.lastKnownBombMap = String(mapName || "");
  }

  const fallbackTtlMs = isDropped ? 180 : 650;
  const canReuseLastKnown =
    !validPosition &&
    !!S.lastKnownBombPosition &&
    S.lastKnownBombMode === modeKey &&
    S.lastKnownBombMap === String(mapName || "") &&
    S.lastKnownBombPositionAt > 0 &&
    (nowMs - S.lastKnownBombPositionAt) <= fallbackTtlMs;
  const effectivePosition = validPosition ? position : (canReuseLastKnown ? S.lastKnownBombPosition : null);
  if (!isActive || !effectivePosition) {
    marker.state.active = false;
    if (!isActive) {
      S.lastKnownBombPosition = null;
      S.lastKnownBombPositionAt = 0;
      S.lastKnownBombMode = "";
      S.lastKnownBombMap = String(mapName || "");
    }
    return;
  }

  const size = getBombMarkerSize(overlayWidth, isDropped);
  marker.size = size;
  marker.color = bomb.m_is_defused ? "#61d784" : (bomb.m_is_planted ? "#ef7373" : "#d88a32");
  marker.isDropped = isDropped;
  marker.isPlanted = !!bomb.m_is_planted;
  marker.isDefused = !!bomb.m_is_defused;
  marker.state.active = true;
  marker.state.targetX = overlayWidth * clamp(effectivePosition.x, 0, 1) - size * 0.5;
  marker.state.targetY = overlayHeight * clamp(effectivePosition.y, 0, 1) - size * 0.5;
  marker.state.targetOpacity = isDropped ? 0.82 : 0.96;
  marker.state.renderX = marker.state.targetX;
  marker.state.renderY = marker.state.targetY;
  marker.state.renderOpacity = marker.state.targetOpacity;
}

function updateBombStatus(bomb) {
  const now = performance.now();
  const isLiveTimer = !!bomb && !!bomb.m_is_planted && (!!bomb.m_is_ticking || !!bomb.m_is_defusing) && !bomb.m_is_defused;
  const timerLength = clamp(Number(bomb?.m_timer_length || 40), 5, 90);
  const defuseLength = clamp(Number(bomb?.m_defuse_length || 10), 1, 15);
  const rawBlow = Number(bomb?.m_blow_time || 0);
  const rawDefuse = Number(bomb?.m_defuse_time || 0);
  const blowValid = Number.isFinite(rawBlow) && rawBlow > 0.05 && rawBlow <= timerLength + 2.0;
  const defuseValid = Number.isFinite(rawDefuse) && rawDefuse > 0.05 && rawDefuse <= defuseLength + 2.0;
  const dt = S.bombUi.lastUpdateAt > 0 ? clamp((now - S.bombUi.lastUpdateAt) / 1000, 0, 0.25) : 0;

  if (isLiveTimer && blowValid && rawBlow > 5.0) {
    S.bombTimerSuppressUntil = 0;
  }

  if (isLiveTimer && now >= S.bombTimerSuppressUntil) {
    S.bombUi.bombTotal = timerLength;
    if (!S.bombUi.active || S.bombUi.bombLeft <= 0) {
      S.bombUi.bombLeft = blowValid ? rawBlow : timerLength;
    } else if (blowValid) {
      S.bombUi.bombLeft = rawBlow;
    } else {
      S.bombUi.bombLeft = Math.max(0, S.bombUi.bombLeft - dt);
    }

    if (bomb.m_is_defusing) {
      S.bombUi.defuseTotal = defuseLength;
      if (!S.bombUi.defusing || S.bombUi.defuseLeft <= 0) {
        S.bombUi.defuseLeft = defuseValid ? rawDefuse : defuseLength;
      } else if (defuseValid) {
        S.bombUi.defuseLeft = rawDefuse;
      } else {
        S.bombUi.defuseLeft = Math.max(0, S.bombUi.defuseLeft - dt);
      }
      S.bombUi.defusing = true;
    } else {
      S.bombUi.defusing = false;
      S.bombUi.defuseLeft = 0;
    }

    S.bombUi.active = true;
    S.bombUi.lastUpdateAt = now;
  } else if (!isLiveTimer) {
    S.bombUi.active = false;
    S.bombUi.defusing = false;
    S.bombUi.bombLeft = 0;
    S.bombUi.defuseLeft = 0;
    S.bombUi.lastUpdateAt = now;
  }

  const blow = S.bombUi.active ? S.bombUi.bombLeft : 0;
  const showTimer = isLiveTimer && blow > 0.05 && now >= S.bombTimerSuppressUntil;

  if (isLiveTimer && S.bombUi.active && blow <= 0.05) {
    S.bombTimerSuppressUntil = now + 1750;
  }

  el.bombCarrierRow.classList.add("hidden");
  el.bombCarrierName.textContent = "---";
  el.bombCarrierName.title = "";

  if (!showTimer) {
    el.bombStatus.classList.add("hidden");
    return;
  }

  el.bombStatus.classList.remove("hidden", "is-critical", "is-defuse-safe", "is-defuse-late", "is-carrier-only");

  const defuse = S.bombUi.defusing ? S.bombUi.defuseLeft : 0;
  const bombFrac = clamp(blow / timerLength, 0, 1);
  const showDefuse = !!bomb?.m_is_defusing && defuse > 0.05;
  const defuseFrac = showDefuse ? clamp(defuse / defuseLength, 0, 1) : 0;
  const defuseCanFinish = showDefuse && defuse <= blow;

  el.bombTimeRow.classList.remove("hidden");
  el.bombTimeBar.classList.remove("hidden");
  if (blow <= 10) {
    el.bombStatus.classList.add("is-critical");
  }
  if (showDefuse) {
    el.bombStatus.classList.add(defuseCanFinish ? "is-defuse-safe" : "is-defuse-late");
  }
  el.bombTimeLeft.textContent = blow.toFixed(1) + "s";
  el.bombTimeProgress.style.width = (bombFrac * 100).toFixed(2) + "%";
  if (showDefuse) {
    el.defuseTimerRow.classList.remove("hidden");
    el.defuseTimeLeft.textContent = defuse.toFixed(1) + "s";
    el.defuseTimeProgress.style.width = (defuseFrac * 100).toFixed(2) + "%";
  } else {
    el.defuseTimerRow.classList.add("hidden");
    el.defuseTimeProgress.style.width = "0%";
  }
}

function renderOverlay(payload = S.renderPayload || S.payload) {
  const mapData = payload && S.mapDataCache.get(payload.m_map);
  const players = payload && Array.isArray(payload.m_players) ? payload.m_players : [];

  if (!payload || !mapData || !el.mapImage.complete) {
    hideUnusedMarkers(new Set());
    if (S.bombMarker) {
      S.bombMarker.state.active = false;
    }
    return;
  }

  const metrics = getOverlayMetrics();
  if (!metrics) return;

  const overlayWidth = metrics.width;
  const overlayHeight = metrics.height;
  const markerSize = getMarkerSize(overlayWidth);
  const used = S._usedKeys || (S._usedKeys = new Set());
  used.clear();

  for (const player of players) {
    if (!shouldShowOnRadar(player, payload.m_local_team)) continue;
    const key = String(player.m_idx);
    const marker = ensureMarker(key);
    updateMarker(marker, player, mapData, payload.m_local_team, markerSize, overlayWidth, overlayHeight);
    used.add(key);
  }

  hideUnusedMarkers(used);
  updateBombMarker(payload.m_bomb, mapData, overlayWidth, overlayHeight, payload.m_map);
  
}

function render() {
  const payload = S.payload;
  const players = payload && Array.isArray(payload.m_players) ? payload.m_players : [];
  const localTeam = payload ? Number(payload.m_local_team || 0) : 0;

  
  let visibleCount = 0, tCount = 0, ctCount = 0;
  for (let i = 0; i < players.length; ++i) {
    const team = Number(players[i].m_team);
    if (team === TEAM_T) ++tCount;
    else if (team === TEAM_CT) ++ctCount;
    if (shouldShowOnRadar(players[i], localTeam)) ++visibleCount;
  }

  el.radarEntities.textContent = String(visibleCount);
  el.radarFocus.textContent = visibleCount === players.length ? "Tactical" : "Filtered";
  el.terroristCount.textContent = String(tCount);
  el.counterTerroristCount.textContent = String(ctCount);
  el.footerTCount.textContent = String(tCount);
  el.footerCtCount.textContent = String(ctCount);
  el.footerVisibleCount.textContent = String(visibleCount);

  renderRoster(el.terroristList, "t", players.filter(p => Number(p.m_team) === TEAM_T), localTeam, "No terrorists");
  renderRoster(el.counterTerroristList, "ct", players.filter(p => Number(p.m_team) === TEAM_CT), localTeam, "No counter-terrorists");
  const timerPayload = S.renderPayload || payload;
  updateBombStatus(timerPayload ? timerPayload.m_bomb : null);

  if (!payload || !players.length) {
    el.radarMessage.textContent = "Waiting for live entities";
    el.radarMessage.classList.remove("hidden");
  } else if (!visibleCount) {
    el.radarMessage.textContent = "Filters hide every entity";
    el.radarMessage.classList.remove("hidden");
  } else {
    el.radarMessage.classList.add("hidden");
  }
}

function animateScene(timestamp) {
  if (!S.animationClock) {
    S.animationClock = timestamp;
  }
  S.animationClock = timestamp;
  S.renderPayload = sampleRenderPayload(performance.now()) || S.payload;
  updateBombStatus(S.renderPayload ? S.renderPayload.m_bomb : null, S.renderPayload ? S.renderPayload.m_players : []);
  renderOverlay(S.renderPayload);
  drawOverlayCanvas();
  S.animationHandle = window.requestAnimationFrame(animateScene);
}

function flushPendingPayload() {
  S.renderHandle = 0;
  const payload = sanitizePayload(S.pendingPayload);
  S.pendingPayload = null;
  if (!payload || !payload.m_map || payload.m_map === "invalid") return;

  const arrivalNow = performance.now();
  const hasMapData = S.mapDataCache.has(payload.m_map);
  updateConnection("live");
  S.currentTransport = S.socket ? "websocket" : "poll";
  updateMapAssets(payload.m_map);
  const mapChanged = !!(S.payload && S.payload.m_map && S.payload.m_map !== payload.m_map);
  if (mapChanged) {
    resetSnapshotTimeline();
  }
  const hasLiveEntities = payload.m_players.length > 0 ||
    !!(payload.m_bomb && (payload.m_bomb.m_is_planted || payload.m_bomb.m_is_dropped));
  if (hasLiveEntities) {
    ingestSnapshot(payload, arrivalNow);
  } else {
    resetSnapshotTimeline();
  }

  
  if (payload.m_map !== "unknown" && !hasMapData) {
    ensureMapData(payload.m_map).then(() => {
      render();
    });
  }

  S.payload = payload;
  if (!S.renderPayload) {
    S.renderPayload = payload;
  }
  scheduleUiRender();
}

function schedulePayloadRender() {
  if (S.renderHandle) return;
  S.renderHandle = window.requestAnimationFrame(() => {
    flushPendingPayload();
  });
}

function queuePayload(payload) {
  S.pendingPayload = payload;
  schedulePayloadRender();
}

async function primeStatus() {
  if (S.statusPrimed) return;
  try {
    const response = await fetch("./api/status?t=" + Date.now(), { cache: "no-store" });
    if (!response.ok) return;
    const status = await response.json();
    const mapName = String(status.active_map || "");
    if (mapName && mapName !== "invalid") {
      updateMapAssets(mapName);
      await ensureMapData(mapName);
      S.statusPrimed = true;
      render();
    }
  } catch {
  }
}

function stopPolling() {
  if (S.pollingTimer) {
    window.clearTimeout(S.pollingTimer);
    S.pollingTimer = 0;
  }
}

function connectWebSocket() {
  if (!("WebSocket" in window)) {
    startPolling();
    return;
  }

  stopPolling();
  updateConnection("connecting");

  if (S.socket) {
    try {
      S.socket.onopen = null;
      S.socket.onmessage = null;
      S.socket.onerror = null;
      S.socket.onclose = null;
      S.socket.close();
    } catch {
    }
    S.socket = null;
  }

  const protocol = window.location.protocol === "https:" ? "wss://" : "ws://";
  const socket = new WebSocket(protocol + window.location.host + "/api/ws?t=" + Date.now());
  S.socket = socket;

  socket.onopen = () => {
    if (S.socket !== socket) return;
    S.currentTransport = "websocket";
    S.reconnectDelay = 400;
    updateConnection("live");
  };

  socket.onmessage = event => {
    try {
      queuePayload(JSON.parse(event.data));
      S.reconnectDelay = 400;
    } catch (e) {
      console.error("[WR] WebSocket parse error", e);
    }
  };

  socket.onerror = () => {
  };

  socket.onclose = () => {
    if (S.socket !== socket) return;
    S.socket = null;
    updateConnection("error");
    const delay = S.reconnectDelay;
    S.reconnectDelay = clamp(Math.round(delay * 1.5), 400, 5000);
    S.pollingTimer = window.setTimeout(connectWebSocket, delay);
  };
}

function startPolling() {
  stopPolling();
  S.currentTransport = "poll";
  const loop = async () => {
    try {
      const response = await fetch("./api/live?t=" + Date.now(), { cache: "no-store" });
      if (!response.ok) throw new Error("bad response");
      queuePayload(await response.json());
      updateConnection("live");
    } catch {
      updateConnection("error");
    }
    S.pollingTimer = window.setTimeout(loop, 32);
  };
  loop();
}

bindUi();
render();
primeStatus();
connectWebSocket();
pingLoop();
S.animationHandle = window.requestAnimationFrame(animateScene);

window.addEventListener("beforeunload", () => {
  stopPolling();
  if (S.socket) {
    try {
      S.socket.onopen = null;
      S.socket.onmessage = null;
      S.socket.onerror = null;
      S.socket.onclose = null;
      S.socket.close();
    } catch {
    }
    S.socket = null;
  }
  if (S.eventSource) {
    S.eventSource.close();
  }
  if (S.uiRenderTimer) {
    window.clearTimeout(S.uiRenderTimer);
  }
  if (S.renderHandle) {
    window.cancelAnimationFrame(S.renderHandle);
  }
  if (S.animationHandle) {
    window.cancelAnimationFrame(S.animationHandle);
  }
});
