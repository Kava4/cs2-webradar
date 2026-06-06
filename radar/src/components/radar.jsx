import { useRef, useState, useEffect, useCallback } from "react";
import Player from "./player";
import Bomb from "./bomb";
import Grenade from "./grenade";
import PlayerCard from "./playercard";
import Scoreboard from "./scoreboard";
import KillFeed from "./killfeed";
import { Latency } from "./latency";
import SupportButton from "./SupportButton";

const MIN_ZOOM = 1.0;
const MAX_ZOOM = 6.0;
const ZOOM_FACTOR = 1.18;

const Radar = ({
  playerArray,
  radarImage,
  mapData,
  localTeam,
  averageLatency,
  bombData,
  settings,
  grenadeArray = [],
  roundInfo = null,
  publicIP    = null,
  copied      = false,
  copyAddress = null,
  setSettings = null,
}) => {
  const containerRef  = useRef();
  const mapAreaRef    = useRef();
  const radarImageRef = useRef();
  const [isFullscreen, setIsFullscreen] = useState(false);

  // Natural letterboxed rect of the image (pixels, relative to container)
  const [imgStyle, setImgStyle] = useState({ w: 0, h: 0, ml: 0, mt: 0 });

  // Zoom & pan. Pan is in screen pixels relative to container centre.
  const [zoom, setZoom] = useState(1);
  const [pan,  setPan]  = useState({ x: 0, y: 0 });
  const dragRef  = useRef(null);
  const touchRef = useRef(null);

  // ── measure letterbox rect ──────────────────────────────────────────────────
  const measure = useCallback(() => {
    const img = radarImageRef.current;
    const con = mapAreaRef.current;
    if (!img || !con) return;
    const nw = img.naturalWidth  || 1024;
    const nh = img.naturalHeight || 1024;
    const cw = con.clientWidth;
    const ch = con.clientHeight;
    if (cw <= 0 || ch <= 0) return;
    const ratio = nw / nh;
    let w, h;
    if (cw / ch > ratio) { w = ch * ratio; h = ch; }
    else                  { w = cw;         h = cw / ratio; }
    setImgStyle({ w, h, ml: (cw - w) / 2, mt: (ch - h) / 2 });
  }, []);

  useEffect(() => {
    const img = radarImageRef.current;
    const con = mapAreaRef.current;
    if (!img || !con) return;
    img.addEventListener("load", measure);
    const ro = new ResizeObserver(measure);
    ro.observe(con);
    if (img.complete) measure();
    else measure();
    return () => { img.removeEventListener("load", measure); ro.disconnect(); };
  }, [radarImage, isFullscreen, measure]);

  // Reset on map change
  useEffect(() => { setZoom(1); setPan({ x: 0, y: 0 }); }, [radarImage]);

  // Clamp pan so zoomed map can't scroll outside container
  const clamp = useCallback((px, py, z) => {
    const con = mapAreaRef.current;
    if (!con || !imgStyle.w) return { x: px, y: py };
    const maxX = Math.max(0, (imgStyle.w * z - con.clientWidth)  / 2);
    const maxY = Math.max(0, (imgStyle.h * z - con.clientHeight) / 2);
    return {
      x: Math.min(maxX, Math.max(-maxX, px)),
      y: Math.min(maxY, Math.max(-maxY, py)),
    };
  }, [imgStyle]);

  useEffect(() => {
    setPan(prev => clamp(prev.x, prev.y, zoom));
  }, [zoom, imgStyle, clamp]);

  // ── fullscreen ──────────────────────────────────────────────────────────────
  useEffect(() => {
    const onChange = () => {
      const fs = !!document.fullscreenElement;
      setIsFullscreen(fs);
      setZoom(1);
      setPan({ x: 0, y: 0 });
      requestAnimationFrame(() => requestAnimationFrame(measure));
    };
    document.addEventListener("fullscreenchange", onChange);
    return () => document.removeEventListener("fullscreenchange", onChange);
  }, [measure]);

  const toggleFullscreen = () => {
    if (!document.fullscreenElement)
      containerRef.current.requestFullscreen().catch(console.error);
    else document.exitFullscreen();
  };

  // ── scroll-wheel zoom ───────────────────────────────────────────────────────
  const onWheel = useCallback((e) => {
    e.preventDefault();
    const con = mapAreaRef.current;
    if (!con) return;
    const rect = con.getBoundingClientRect();
    // Cursor offset from container centre (matches our pan coordinate space)
    const cx = e.clientX - (rect.left + rect.width  / 2);
    const cy = e.clientY - (rect.top  + rect.height / 2);

    setZoom(prev => {
      const next = e.deltaY < 0
        ? Math.min(MAX_ZOOM, prev * ZOOM_FACTOR)
        : Math.max(MIN_ZOOM, prev / ZOOM_FACTOR);
      const ratio = next / prev;
      setPan(prevPan => clamp(
        prevPan.x + cx * (1 - ratio),
        prevPan.y + cy * (1 - ratio),
        next
      ));
      return next;
    });
  }, [clamp]);

  useEffect(() => {
    const el = mapAreaRef.current;
    if (!el) return;
    el.addEventListener("wheel", onWheel, { passive: false });
    return () => el.removeEventListener("wheel", onWheel);
  }, [onWheel, isFullscreen]);

  // ── drag-to-pan ─────────────────────────────────────────────────────────────
  const onMouseDown = useCallback((e) => {
    if (zoom <= 1 || e.button !== 0) return;
    e.preventDefault();
    dragRef.current = { mx: e.clientX, my: e.clientY, px: pan.x, py: pan.y };
  }, [zoom, pan]);

  const onMouseMove = useCallback((e) => {
    if (!dragRef.current) return;
    setPan(clamp(
      dragRef.current.px + (e.clientX - dragRef.current.mx),
      dragRef.current.py + (e.clientY - dragRef.current.my),
      zoom
    ));
  }, [zoom, clamp]);

  const onMouseUp = useCallback(() => { dragRef.current = null; }, []);

  // Touch
  const onTouchStart = useCallback((e) => {
    if (zoom <= 1) return;
    touchRef.current = { tx: e.touches[0].clientX, ty: e.touches[0].clientY, px: pan.x, py: pan.y };
  }, [zoom, pan]);

  const onTouchMove = useCallback((e) => {
    if (!touchRef.current) return;
    e.preventDefault();
    setPan(clamp(
      touchRef.current.px + (e.touches[0].clientX - touchRef.current.tx),
      touchRef.current.py + (e.touches[0].clientY - touchRef.current.ty),
      zoom
    ));
  }, [zoom, clamp]);

  const onTouchEnd = useCallback(() => { touchRef.current = null; }, []);

  // ── visibility flags ────────────────────────────────────────────────────────
  const showTCards     = settings?.showTCards     !== false;
  const showCTCards    = settings?.showCTCards    !== false;
  const showScoreboard = settings?.showScoreboard !== false;

  const mapTransform = `translate(calc(-50% + ${pan.x}px), calc(-50% + ${pan.y}px)) scale(${zoom})`;
  const mapW = imgStyle.w > 0 ? imgStyle.w : null;
  const mapH = imgStyle.h > 0 ? imgStyle.h : null;
  const entitiesReady = imgStyle.w > 0 && imgStyle.h > 0;

  const mapCursor = zoom > 1 ? (dragRef.current ? "grabbing" : "grab") : "default";

  return (
    <div
      ref={containerRef}
      id="radar"
      className={`flex flex-col overflow-hidden ${
        isFullscreen ? "w-screen h-screen bg-[#07141e]" : "w-full h-full rounded-xl"
      }`}
      style={{ minHeight: "300px" }}
    >
      {isFullscreen && (
        <div
          onMouseDown={e => e.stopPropagation()}
          style={{
            flexShrink: 0,
            display: "grid",
            gridTemplateColumns: "1fr auto 1fr",
            alignItems: "center",
            gap: 12,
            padding: "8px 16px",
            minHeight: 56,
            background: "rgba(13,17,23,0.92)",
            borderBottom: "1px solid rgba(30,39,54,0.8)",
            backdropFilter: "blur(6px)",
            zIndex: 300,
          }}
        >
          <div />
          <div style={{ justifySelf: "center" }}>
            {showScoreboard && (
              <Scoreboard roundInfo={roundInfo} localTeam={localTeam} bombData={bombData} compact />
            )}
          </div>
          <div style={{ display: "flex", alignItems: "center", gap: 8, justifySelf: "end" }}>
            <SupportButton />
            {publicIP && copyAddress && (
              <button onClick={copyAddress} disabled={copied} style={{
                display: "flex", alignItems: "center", gap: 6,
                padding: "5px 11px", borderRadius: 6, background: "transparent",
                border: `1px solid ${copied ? "var(--hp-high)" : "var(--bg-border-dim)"}`,
                color: copied ? "var(--hp-high)" : "var(--text-secondary)",
                fontSize: 12, fontWeight: 700, cursor: "pointer",
                fontFamily: "inherit", letterSpacing: "0.04em", transition: "all 0.15s",
              }}>
                <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                  {copied
                    ? <path d="M20 6L9 17l-5-5" />
                    : <><rect x="9" y="9" width="13" height="13" rx="2" /><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1" /></>}
                </svg>
                {copied ? "Copied!" : "Share"}
              </button>
            )}
            {setSettings && <Latency settings={settings} setSettings={setSettings} />}
            <button onClick={toggleFullscreen} style={{
              display: "flex", alignItems: "center", gap: 5,
              padding: "5px 10px", borderRadius: 6, background: "transparent",
              border: "1px solid var(--bg-border-dim)", color: "var(--text-secondary)",
              fontSize: 12, fontWeight: 700, cursor: "pointer",
              fontFamily: "inherit", letterSpacing: "0.04em",
            }}>
              <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                <path d="M8 3H5a2 2 0 0 0-2 2v3m18 0V5a2 2 0 0 0-2-2h-3m0 18h3a2 2 0 0 0 2-2v-3M3 16v3a2 2 0 0 0 2 2h3"/>
              </svg>
              Exit
            </button>
          </div>
        </div>
      )}

      <div
        ref={mapAreaRef}
        className="relative flex-1 min-h-0 overflow-hidden"
        style={{ cursor: mapCursor }}
        onMouseDown={onMouseDown}
        onMouseMove={onMouseMove}
        onMouseUp={onMouseUp}
        onMouseLeave={onMouseUp}
        onTouchStart={onTouchStart}
        onTouchMove={onTouchMove}
        onTouchEnd={onTouchEnd}
      >
      {isFullscreen && settings?.showKillFeed !== false && (
        <div style={{
          position: "absolute", bottom: 20,
          right: showCTCards ? 310 : 20, zIndex: 200, pointerEvents: "none",
        }}>
          <KillFeed playerArray={playerArray} />
        </div>
      )}

      {/* ── FULLSCREEN PLAYER PANELS ─────────────────────────────────────────── */}
      {isFullscreen && (
        <>
          <div className="absolute left-0 top-0 bottom-0 w-[350px] bg-gradient-to-r from-black/60 to-transparent pointer-events-none z-[170]" />
          {showTCards && (
            <div
              onMouseDown={e => e.stopPropagation()}
              className="absolute left-6 bottom-10 w-[280px] z-[180] flex flex-col gap-3 overflow-y-auto pr-2 no-scrollbar scroll-smooth"
              style={{ top: 12 }}
            >
              <h2 className="text-orange-500 font-black text-[11px] uppercase border-b border-orange-500/40 pb-2 mb-3 tracking-[0.2em] drop-shadow-md">
                Terrorists
              </h2>
              {playerArray.filter(p => p.m_team === 2).map(p => (
                <div key={p.m_idx} className="transition-transform duration-300 hover:scale-[1.02]">
                  <PlayerCard playerData={p} isOnRightSide={false} settings={settings} />
                </div>
              ))}
            </div>
          )}
          <div className="absolute right-0 top-0 bottom-0 w-[350px] bg-gradient-to-l from-black/60 to-transparent pointer-events-none z-[170]" />
          {showCTCards && (
            <div
              onMouseDown={e => e.stopPropagation()}
              className="absolute right-6 bottom-10 w-[280px] z-[180] flex flex-col gap-3 overflow-y-auto pl-2 no-scrollbar scroll-smooth"
              style={{ top: 12 }}
            >
              <h2 className="text-blue-500 font-black text-[11px] uppercase border-b border-blue-500/40 pb-2 mb-3 tracking-[0.2em] text-right drop-shadow-md">
                Counter-Terrorists
              </h2>
              {playerArray.filter(p => p.m_team === 3).map(p => (
                <div key={p.m_idx} className="transition-transform duration-300 hover:scale-[1.02]">
                  <PlayerCard playerData={p} isOnRightSide={true} settings={settings} />
                </div>
              ))}
            </div>
          )}
        </>
      )}

      {/* ── MAP IMAGE + ENTITY LAYER ──────────────────────────────────────────
           Single wrapper div that contains BOTH the image and all entities.
           It sits at left:50% top:50% so its own origin is the container
           centre. The transform then does: shift to centre, apply pan, scale.
           transformOrigin "0 0" means scale happens around that already-centred
           origin — no additional math needed.                                 */}
      <div
        className="absolute pointer-events-none"
        style={{
          width:           mapW ?? "100%",
          height:          mapH ?? "100%",
          left:            "50%",
          top:             "50%",
          transform:       mapTransform,
          transformOrigin: "center center",
          willChange:      "transform",
        }}
      >
        <img
          ref={radarImageRef}
          src={radarImage}
          alt="Radar Map"
          draggable={false}
          onLoad={measure}
          style={{
            position: "absolute", inset: 0,
            width: "100%", height: "100%",
            display: "block", userSelect: "none",
          }}
        />

        {entitiesReady && playerArray.map(player => (
          <Player
            key={player.m_idx}
            playerData={player}
            mapData={mapData}
            mapWidth={imgStyle.w}
            mapHeight={imgStyle.h}
            localTeam={localTeam}
            averageLatency={averageLatency}
            settings={settings}
          />
        ))}

        {entitiesReady && grenadeArray.map((grenade, i) => (
          <Grenade
            key={`gren-${i}`}
            grenadeData={grenade}
            mapData={mapData}
            mapWidth={imgStyle.w}
            mapHeight={imgStyle.h}
            settings={settings}
          />
        ))}

        {entitiesReady && bombData && (
          <Bomb
            bombData={bombData}
            mapData={mapData}
            mapWidth={imgStyle.w}
            mapHeight={imgStyle.h}
            localTeam={localTeam}
            averageLatency={averageLatency}
            settings={settings}
          />
        )}
      </div>

      </div>
    </div>
  );
};

export default Radar;