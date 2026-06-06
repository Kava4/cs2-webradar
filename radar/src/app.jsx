import ReactDOM from "react-dom/client";
import { useEffect, useState, useRef } from "react";
import "./App.css";
import PlayerCard from "./components/playercard";
import Radar from "./components/radar";
import { getLatency, Latency } from "./components/latency";
import MaskedIcon from "./components/maskedicon";
import Scoreboard from "./components/scoreboard";
import KillFeed from "./components/killfeed";
import { BRAND_ICON, BRAND_NAME, WS_PATH } from "./brand";
import SupportButton from "./components/SupportButton";

const CONNECTION_TIMEOUT = 5000;

/* change this to '1' if you want to use offline (your own pc only) */
const USE_LOCALHOST = 0;

/* static public ip fallback (optional) */
const PUBLIC_IP = "public ip goes here".trim();
const PORT = 22006;

const EFFECTIVE_IP = USE_LOCALHOST
  ? "localhost"
  : PUBLIC_IP.match(/[a-zA-Z]/)
  ? window.location.hostname
  : PUBLIC_IP;

// WebSocket always connects to same hostname as the page, port 22006
const WS_URL = `ws://${window.location.hostname}:${PORT}${WS_PATH}`;

const IS_LOCAL_SESSION = (() => {
  const h = window.location.hostname;
  if (h === "localhost" || h === "127.0.0.1") return true;
  if (/^192\.168\.\d{1,3}\.\d{1,3}$/.test(h)) return true;
  if (/^10\.\d{1,3}\.\d{1,3}\.\d{1,3}$/.test(h)) return true;
  if (/^172\.(1[6-9]|2\d|3[01])\.\d{1,3}\.\d{1,3}$/.test(h)) return true;
  return false;
})();

const DEFAULT_SETTINGS = {
  dotSize: 1,
  bombSize: 0.5,
  coneLength: 1,
  showAllNames: false,
  showEnemyNames: true,
  showViewCones: true,
  showGrenades: true,
  showSmoke: true,
  showFire: true,
  showGrenadeLabels: true,
  showHealthBars: true,
  showAmmo: true,
  showArmor: true,
  showPing: true,
  showMoney: true,
  showFlashEffect: true,
  showBombTimer: true,
  showKillFeed: true,
  showTCards: true,
  showCTCards: true,
};

const loadSettings = () => {
  try {
    const saved = localStorage.getItem("radarSettings");
    return saved ? { ...DEFAULT_SETTINGS, ...JSON.parse(saved) } : DEFAULT_SETTINGS;
  } catch { return DEFAULT_SETTINGS; }
};

// ── RadarColumn – MUST be defined outside App so React doesn't remount it ─────
const RadarColumn = ({
  roundInfo, localTeam, playerArray, mapData, averageLatency,
  bombData, grenadeArray, settings,
  publicIP, copied, copyAddress, setSettings,
}) => (
  <div className="flex flex-col items-center gap-2 flex-1" style={{ height: "100%", width: "100%", minHeight: 0 }}>
    <Radar
      playerArray={playerArray}
      radarImage={`./data/${mapData.name}/radar.png${mapData.imgBust ? `?v=${mapData.imgBust}` : ''}`}
      mapData={mapData}
      localTeam={localTeam}
      averageLatency={averageLatency}
      bombData={bombData}
      grenadeArray={grenadeArray}
      roundInfo={roundInfo}
      settings={settings}
      publicIP={publicIP}
      copied={copied}
      copyAddress={copyAddress}
      setSettings={setSettings}
    />
    {/* Kill feed – only visible outside fullscreen */}
    <div className="w-full flex justify-end pr-2">
      {settings?.showKillFeed !== false && <KillFeed playerArray={playerArray} />}
    </div>
  </div>
);

const App = () => {
  const [averageLatency, setAverageLatency] = useState(0);
  const [playerArray, setPlayerArray]       = useState([]);
  const [mapData, setMapData]               = useState();
  const [localTeam, setLocalTeam]           = useState();
  const [bombData, setBombData]             = useState();
  const [grenadeArray, setGrenadeArray]     = useState([]);
  const [roundInfo, setRoundInfo]           = useState(null);
  const [settings, setSettings]             = useState(loadSettings());
  const [publicIP, setPublicIP]             = useState(null);
  const [showTeamList, setShowTeamList]     = useState("both");
  const [isMobile, setIsMobile]             = useState(false);
  const [copied, setCopied]                 = useState(false);
  const [radarFullscreen, setRadarFullscreen] = useState(false);
  const wsRef                               = useRef(null);

  useEffect(() => {
    const onFullscreenChange = () => setRadarFullscreen(!!document.fullscreenElement);
    document.addEventListener("fullscreenchange", onFullscreenChange);
    return () => document.removeEventListener("fullscreenchange", onFullscreenChange);
  }, []);

  const toggleRadarFullscreen = () => {
    const el = document.getElementById("radar");
    if (!el) return;
    if (!document.fullscreenElement) el.requestFullscreen().catch(console.error);
    else document.exitFullscreen();
  };

  // ── Track current map name to avoid redundant fetches and detect changes ────
  // loadingForMap: null = idle, string = fetch in-flight for that map name.
  // We intentionally do NOT use a separate boolean — the map name IS the lock.
  const currentMapRef  = useRef(null);   // last successfully loaded map
  const loadingForMap  = useRef(null);   // map whose fetch is currently in-flight

  /* Detect mobile */
  useEffect(() => {
    const checkMobile = () => setIsMobile(window.innerWidth < 1024);
    checkMobile();
    window.addEventListener("resize", checkMobile);
    return () => window.removeEventListener("resize", checkMobile);
  }, []);

  /* Save settings */
  useEffect(() => {
    localStorage.setItem("radarSettings", JSON.stringify(settings));
  }, [settings]);

  /* Fetch public IP */
  useEffect(() => {
    fetch("https://api.ipify.org?format=json")
      .then(res => res.json())
      .then(data => setPublicIP(data.ip))
      .catch(() => setPublicIP("Unavailable"));
  }, []);

  /* Copy IP */
  const shutdownApp = async () => {
    if (!window.confirm(`Stop ${BRAND_NAME}?`)) return;
    const ws = wsRef.current;
    if (ws?.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ cmd: "shutdown" }));
    }
    try {
      await fetch(`http://${window.location.hostname}:${PORT}/shutdown`, { method: "POST" });
    } catch {
      // WS broadcast is the primary path
    }
  };

  const copyAddress = () => {
    if (!publicIP) return;
    const text = `${publicIP}:5173`;
    if (navigator.clipboard && window.isSecureContext) {
      navigator.clipboard.writeText(text);
    } else {
      const textarea = document.createElement("textarea");
      textarea.value = text;
      textarea.style.position = "fixed";
      textarea.style.opacity = "0";
      document.body.appendChild(textarea);
      textarea.select();
      document.execCommand("copy");
      textarea.remove();
    }
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  /* ── WebSocket with exponential-backoff reconnect ────────────────────────── */
  useEffect(() => {
    const wsURL = WS_URL;

    let ws          = null;
    let attempt     = 0;
    let retryTimer  = null;
    let destroyed   = false;

    const setStatus = (text) => {
      const el = document.getElementsByClassName("radar_message")[0];
      if (el) el.textContent = text;
    };

    const connect = () => {
      if (destroyed) return;
      try {
        ws = new WebSocket(wsURL);
        wsRef.current = ws;
      } catch (err) {
        setStatus(`WebSocket error: ${err}`);
        scheduleReconnect();
        return;
      }

      const connTimeout = setTimeout(() => {
        if (ws.readyState !== WebSocket.OPEN) ws.close();
      }, CONNECTION_TIMEOUT);

      ws.onopen = () => {
        clearTimeout(connTimeout);
        attempt = 0;
        setStatus("");
        console.info("WebSocket connected");
      };

      ws.onclose = () => {
        clearTimeout(connTimeout);
        if (!destroyed) scheduleReconnect();
      };

      ws.onerror = (err) => {
        clearTimeout(connTimeout);
        console.error("WebSocket error", err);
        // onclose fires right after onerror; reconnect scheduled there
      };

      // ── Process one WebSocket message ─────────────────────────────────────
      // The handler is async because event.data is a Blob. Multiple async
      // callbacks can run interleaved, so all map-state decisions are based
      // solely on loadingForMap (a ref that holds the in-flight map name)
      // rather than a boolean that can be stale across await points.
      ws.onmessage = async (event) => {
        setAverageLatency(getLatency());

        let parsedData;
        try {
          parsedData = JSON.parse(await event.data.text());
        } catch {
          return;
        }

        if (parsedData.cmd === "shutdown") return;

        // ── Reload command from C++ on map change ─────────────────────────────
        if (parsedData.cmd === "reload") {
          console.info("[radar] Map change detected — reloading page");
          location.reload();
          return;
        }

        setPlayerArray(parsedData.m_players ?? []);
        setLocalTeam(parsedData.m_local_team);
        setBombData(parsedData.m_bomb);
        setGrenadeArray(parsedData.m_grenades ?? []);
        setRoundInfo(parsedData.m_round_info ?? null);

        const map = parsedData.m_map;
        const validMap = map && map !== "invalid";

        // ── Map change detection (pure client-side, no C++ needed) ───────────
        // If we already have a loaded map and a DIFFERENT valid map arrives
        // → just reload the page. Clean, simple, reliable.
        if (validMap && currentMapRef.current && map !== currentMapRef.current) {
          console.info(`[radar] Map changed ${currentMapRef.current} → ${map}, reloading`);
          location.reload();
          return;
        }

        // ── No valid map ──────────────────────────────────────────────────────
        if (!validMap) {
          if (currentMapRef.current !== null || loadingForMap.current !== null) {
            console.info("[radar] Map cleared — waiting for new map");
            currentMapRef.current = null;
            loadingForMap.current = null;
            setMapData(undefined);
            document.body.style.backgroundImage = "";
          }
          return;
        }

        // ── Same map already loaded ───────────────────────────────────────────
        if (map === currentMapRef.current) return;

        // ── Same map already being fetched ───────────────────────────────────
        if (loadingForMap.current === map) return;

        // ── First load or retry ───────────────────────────────────────────────
        loadingForMap.current = map;
        setMapData(undefined);
        console.info(`[radar] Loading map: ${map}`);

        try {
          const bust = Date.now();
          const res  = await fetch(`data/${map}/data.json?v=${bust}`);
          if (!res.ok) throw new Error(`HTTP ${res.status}`);
          const json = await res.json();

          if (loadingForMap.current !== map) return; // stale

          currentMapRef.current = map;
          setMapData({ ...json, name: map, imgBust: bust });
          document.body.style.backgroundImage = `url(./data/${map}/background.png?v=${bust})`;
          console.info(`[radar] Map ready: ${map}`);
        } catch (err) {
          console.error(`[radar] Failed to load ${map}:`, err);
        } finally {
          if (loadingForMap.current === map) loadingForMap.current = null;
        }
      };
    };

    const scheduleReconnect = () => {
      if (destroyed) return;
      // Exponential backoff: 500ms, 1s, 2s, 4s, 8s, 16s (max)
      const delay = Math.min(500 * Math.pow(2, attempt), 16000)
                  + Math.random() * 200; // jitter
      attempt++;
      setStatus(`Reconnecting in ${(delay / 1000).toFixed(1)}s… (attempt ${attempt})`);
      console.info(`WebSocket reconnect in ${Math.round(delay)}ms`);
      retryTimer = setTimeout(connect, delay);
    };

    connect();

    return () => {
      destroyed = true;
      clearTimeout(retryTimer);
      wsRef.current = null;
      if (ws) ws.close();
    };
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
  /* ── END WebSocket ─────────────────────────────────────────────────────────── */

  const terrorists        = playerArray.filter(p => p.m_team === 2);
  const counterTerrorists = playerArray.filter(p => p.m_team === 3);

  const showTCards  = settings.showTCards  !== false;
  const showCTCards = settings.showCTCards !== false;

  return (
    <div
      className="fixed inset-0 flex flex-col overflow-hidden"
      style={{ background: "var(--bg-base)" }}
    >
      {/* ── Top bar ──────────────────────────────────────────────────────────── */}
      {!radarFullscreen && (
      <div style={{
        display: "grid",
        gridTemplateColumns: "auto 1fr auto",
        alignItems: "center",
        gap: 12,
        padding: "8px 16px",
        borderBottom: "1px solid var(--bg-border-dim)",
        background: "var(--bg-panel)",
        flexShrink: 0,
        zIndex: 100,
        minHeight: 52,
      }}>
        <div style={{ display: "flex", alignItems: "center", gap: 10, minWidth: 0 }}>
          <img
            src={BRAND_ICON}
            alt=""
            style={{
              width: 36,
              height: 36,
              borderRadius: 8,
              display: "block",
              flexShrink: 0,
            }}
          />
          <div style={{ lineHeight: 1.1, minWidth: 0 }}>
            <div style={{
              fontSize: 15,
              fontWeight: 800,
              letterSpacing: "0.03em",
              whiteSpace: "nowrap",
            }}>
              <span style={{ color: "var(--text-primary)" }}>Aim</span>
              <span style={{ color: "#0084ff" }}>Sync</span>
            </div>
            <div style={{
              fontSize: 9,
              fontWeight: 700,
              letterSpacing: "0.22em",
              color: "var(--text-muted)",
              marginTop: 2,
            }}>
              WEBRADAR
            </div>
          </div>
        </div>

        <div style={{ display: "flex", justifyContent: "center", minWidth: 0 }}>
          {settings?.showScoreboard !== false && roundInfo && (
            <Scoreboard roundInfo={roundInfo} localTeam={localTeam} bombData={bombData} compact />
          )}
        </div>

        <div style={{ display: "flex", alignItems: "center", gap: 8, justifySelf: "end" }}>
        <SupportButton />

        {/* Share Radar */}
        {publicIP && (
          <button
            onClick={copyAddress}
            disabled={copied}
            style={{
              display: "flex", alignItems: "center", gap: 6,
              padding: "6px 12px", borderRadius: 6,
              background: "transparent",
              border: "1px solid var(--bg-border-dim)",
              color: copied ? "var(--hp-high)" : "var(--text-secondary)",
              fontSize: 12, fontWeight: 700, cursor: "pointer",
              fontFamily: "inherit", letterSpacing: "0.04em", transition: "all 0.15s",
            }}
            onMouseEnter={e => { if (!copied) { e.currentTarget.style.borderColor = "var(--bg-border)"; e.currentTarget.style.color = "var(--text-primary)"; }}}
            onMouseLeave={e => { if (!copied) { e.currentTarget.style.borderColor = "var(--bg-border-dim)"; e.currentTarget.style.color = "var(--text-secondary)"; }}}
          >
            <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
              {copied
                ? <path d="M20 6L9 17l-5-5" />
                : <><rect x="9" y="9" width="13" height="13" rx="2" /><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1" /></>
              }
            </svg>
            {copied ? "Copied!" : "Share Radar"}
          </button>
        )}

        {playerArray.length > 0 && mapData && (
          <button
            onClick={toggleRadarFullscreen}
            style={{
              display: "flex", alignItems: "center", gap: 6,
              padding: "6px 12px", borderRadius: 6,
              background: "transparent",
              border: "1px solid var(--bg-border-dim)",
              color: "var(--text-secondary)",
              fontSize: 12, fontWeight: 700, cursor: "pointer",
              fontFamily: "inherit", letterSpacing: "0.04em", transition: "all 0.15s",
            }}
            onMouseEnter={e => { e.currentTarget.style.borderColor = "var(--bg-border)"; e.currentTarget.style.color = "var(--text-primary)"; }}
            onMouseLeave={e => { e.currentTarget.style.borderColor = "var(--bg-border-dim)"; e.currentTarget.style.color = "var(--text-secondary)"; }}
          >
            <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
              <path d="M8 3H5a2 2 0 0 0-2 2v3m18 0V5a2 2 0 0 0-2-2h-3m0 18h3a2 2 0 0 0 2-2v-3M3 16v3a2 2 0 0 0 2 2h3"/>
            </svg>
            Fullscreen
          </button>
        )}

        {IS_LOCAL_SESSION && (
          <button
            onClick={shutdownApp}
            style={{
              display: "flex", alignItems: "center", gap: 6,
              padding: "6px 12px", borderRadius: 6,
              background: "transparent",
              border: "1px solid rgba(239,68,68,0.35)",
              color: "#f87171",
              fontSize: 12, fontWeight: 700, cursor: "pointer",
              fontFamily: "inherit", letterSpacing: "0.04em", transition: "all 0.15s",
            }}
            onMouseEnter={e => { e.currentTarget.style.borderColor = "rgba(239,68,68,0.6)"; e.currentTarget.style.background = "rgba(239,68,68,0.08)"; }}
            onMouseLeave={e => { e.currentTarget.style.borderColor = "rgba(239,68,68,0.35)"; e.currentTarget.style.background = "transparent"; }}
          >
            <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
              <path d="M18 6L6 18M6 6l12 12" />
            </svg>
            Exit
          </button>
        )}

        {/* Settings / Latency */}
        <Latency value={averageLatency} settings={settings} setSettings={setSettings} />
        </div>
      </div>
      )}

      {/* ── Main content ─────────────────────────────────────────────────────── */}
      <div className="flex-1 overflow-y-auto overflow-x-hidden" style={{ padding: isMobile ? "12px" : "16px 20px" }}>

        {playerArray.length > 0 && mapData ? (
          <>
            {/* ── Mobile layout ──────────────────────────────────────────────── */}
            {isMobile && (
              <div className="flex flex-col gap-3">

                {/* Radar — full width, square aspect */}
                <div style={{ width: "100%", aspectRatio: "1 / 1", minHeight: 0 }}>
                  <RadarColumn
                    roundInfo={roundInfo} localTeam={localTeam} playerArray={playerArray}
                    mapData={mapData} averageLatency={averageLatency} bombData={bombData}
                    grenadeArray={grenadeArray} settings={settings}
                    publicIP={publicIP} copied={copied} copyAddress={copyAddress} setSettings={setSettings}
                  />
                </div>

                {/* Team tabs */}
                <div style={{ display: "flex", gap: 6 }}>
                  {[
                    { key: "t",    label: `T  (${terrorists.length})`,        color: "var(--t-color)",  border: "var(--t-border)",  bg: "var(--t-dim)"  },
                    { key: "ct",   label: `CT  (${counterTerrorists.length})`, color: "var(--ct-color)", border: "var(--ct-border)", bg: "var(--ct-dim)" },
                    { key: "both", label: "Both",                              color: "var(--text-primary)", border: "var(--bg-border)", bg: "var(--bg-card)" },
                  ].map(({ key, label, color, border, bg }) => (
                    <button
                      key={key}
                      onClick={() => setShowTeamList(key)}
                      style={{
                        flex: 1, padding: "7px 6px", borderRadius: 6,
                        border: `1px solid ${showTeamList === key ? border : "var(--bg-border-dim)"}`,
                        background: showTeamList === key ? bg : "transparent",
                        color: showTeamList === key ? color : "var(--text-secondary)",
                        fontSize: 11, fontWeight: 700, cursor: "pointer", fontFamily: "inherit",
                        transition: "all 0.12s",
                      }}
                    >{label}</button>
                  ))}
                </div>

                {/* Player cards — full width, vertical list */}
                <ul style={{ listStyle: "none", margin: 0, padding: 0, display: "flex", flexDirection: "column", gap: 6 }}>
                  {(showTeamList === "t" || showTeamList === "both") && showTCards && terrorists.map(p => (
                    <PlayerCard key={p.m_idx} playerData={p} isOnRightSide={false} settings={settings} />
                  ))}
                  {(showTeamList === "ct" || showTeamList === "both") && showCTCards && counterTerrorists.map(p => (
                    <PlayerCard key={p.m_idx} playerData={p} isOnRightSide={true} settings={settings} />
                  ))}
                </ul>

              </div>
            )}

            {/* ── Desktop layout ─────────────────────────────────────────────── */}
            {!isMobile && (
              <div className="flex flex-col gap-2" style={{ height: radarFullscreen ? "100vh" : "calc(100vh - 60px)", minHeight: 0 }}>

              <div className="flex gap-4 flex-1" style={{ minHeight: 0 }}>

                {/* T side */}
                {showTCards && (
                  <div className="flex flex-col gap-2" style={{ width: 220, flexShrink: 0, overflowY: "auto" }}>
                    {terrorists.map(p => (
                      <PlayerCard key={p.m_idx} playerData={p} isOnRightSide={false} settings={settings} />
                    ))}
                  </div>
                )}

                {/* Radar */}
                <RadarColumn
                  roundInfo={roundInfo} localTeam={localTeam} playerArray={playerArray}
                  mapData={mapData} averageLatency={averageLatency} bombData={bombData}
                  grenadeArray={grenadeArray} settings={settings}
                  publicIP={publicIP} copied={copied} copyAddress={copyAddress} setSettings={setSettings}
                />

                {/* CT side */}
                {showCTCards && (
                  <div className="flex flex-col gap-2" style={{ width: 220, flexShrink: 0, overflowY: "auto" }}>
                    {counterTerrorists.map(p => (
                      <PlayerCard key={p.m_idx} playerData={p} isOnRightSide={true} settings={settings} />
                    ))}
                  </div>
                )}
              </div>
              </div>
            )}
          </>
        ) : (
          /* ── Waiting screen ──────────────────────────────────────────────── */
          <div className="flex flex-col items-center justify-center h-full gap-4" style={{ minHeight: "60vh" }}>
            <div className="radar_message" style={{
              color: "var(--text-secondary)", fontSize: 14, textAlign: "center",
              maxWidth: 400, lineHeight: 1.6,
            }}>
              {mapData === undefined && playerArray.length === 0
                ? "Waiting for game data…"
                : "Connecting to WebSocket…"}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default App;