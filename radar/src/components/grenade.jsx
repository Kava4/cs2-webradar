import { useRef } from "react";
import { getRadarPosition, getEntityTranslate } from "../utilities/utilities";

const radiusToPx = (gameUnits, imgWidth, scale) => {
  if (!imgWidth || !scale) return 0;
  return (gameUnits / scale / 1024) * imgWidth * 2;
};

// ─── SMOKE — solid, opaque cloud that actually blocks view ───────────────────
const SmokeEffect = ({ diameter }) => (
  <>
    <style>{`
      @keyframes smokeRotate { from{transform:rotate(0deg)} to{transform:rotate(360deg)} }
      @keyframes smokePulse  { 0%,100%{opacity:.92;transform:scale(1)} 50%{opacity:1;transform:scale(1.03)} }
      @keyframes smokeBlob1  { 0%,100%{transform:translate(0,0)scale(1)} 50%{transform:translate(5px,-4px)scale(1.07)} }
      @keyframes smokeBlob2  { 0%,100%{transform:translate(0,0)scale(1)} 50%{transform:translate(-4px,5px)scale(1.06)} }
      @keyframes smokeBlob3  { 0%,100%{transform:translate(0,0)scale(1)} 50%{transform:translate(3px,3px)scale(1.05)} }
      @keyframes smokeBlob4  { 0%,100%{transform:translate(0,0)scale(1)} 50%{transform:translate(-3px,-3px)scale(1.04)} }
    `}</style>
    <div style={{
      position: "absolute", width: diameter, height: diameter,
      left: "50%", top: "50%",
      transform: "translate(-50%, -50%)",
      borderRadius: "50%", overflow: "visible", zIndex: 1,
    }}>
      {/* Hard outer edge ring */}
      <div style={{
        position: "absolute", inset: 0, borderRadius: "50%",
        border: "2px solid rgba(200,210,220,0.55)",
        boxShadow: "0 0 12px rgba(180,190,200,0.35), inset 0 0 20px rgba(180,190,200,0.15)",
        animation: "smokeRotate 18s linear infinite",
      }} />
      {/* Solid fill base — this is the "blocking" layer */}
      <div style={{
        position: "absolute", inset: "3%", borderRadius: "50%",
        background: "radial-gradient(circle at 50% 50%, rgba(210,220,228,0.88) 0%, rgba(185,198,208,0.82) 30%, rgba(155,170,180,0.72) 60%, rgba(120,135,145,0.45) 80%, transparent 100%)",
        backdropFilter: "blur(2px)",
        animation: "smokePulse 4s ease-in-out infinite",
      }} />
      {/* Bright center highlight */}
      <div style={{
        position: "absolute", width: "50%", height: "50%", top: "15%", left: "25%",
        borderRadius: "50%",
        background: "radial-gradient(circle, rgba(235,242,248,0.75) 0%, rgba(210,222,232,0.50) 50%, transparent 100%)",
        animation: "smokeBlob1 5s ease-in-out infinite",
      }} />
      {/* Mid blob bottom-right */}
      <div style={{
        position: "absolute", width: "52%", height: "52%", bottom: "6%", right: "8%",
        borderRadius: "50%",
        background: "radial-gradient(circle, rgba(200,212,222,0.65) 0%, rgba(165,178,188,0.40) 60%, transparent 100%)",
        animation: "smokeBlob2 6s ease-in-out infinite",
      }} />
      {/* Edge blob top-right */}
      <div style={{
        position: "absolute", width: "42%", height: "42%", top: "28%", right: "6%",
        borderRadius: "50%",
        background: "radial-gradient(circle, rgba(195,207,216,0.60) 0%, transparent 70%)",
        animation: "smokeBlob3 5.5s ease-in-out infinite",
      }} />
      {/* Edge blob bottom-left */}
      <div style={{
        position: "absolute", width: "38%", height: "38%", bottom: "12%", left: "6%",
        borderRadius: "50%",
        background: "radial-gradient(circle, rgba(188,200,210,0.55) 0%, transparent 70%)",
        animation: "smokeBlob4 7s ease-in-out infinite",
      }} />
      {/* "SMK" label in center */}
      <div style={{
        position: "absolute", top: "50%", left: "50%",
        transform: "translate(-50%, -50%)",
        fontSize: Math.max(9, diameter * 0.12),
        fontWeight: 900,
        color: "rgba(80,95,105,0.9)",
        textShadow: "0 1px 3px rgba(255,255,255,0.5)",
        letterSpacing: "0.06em",
        lineHeight: 1,
        pointerEvents: "none",
        zIndex: 3,
      }}>SMK</div>
    </div>
  </>
);

// ─── FIRE — realistic layered flame area ─────────────────────────────────────
const FireEffect = ({ diameter }) => {
  const r = diameter / 2;
  return (
    <>
      <style>{`
        @keyframes fireGlow     { from{opacity:.75;transform:scale(0.97)} to{opacity:1;transform:scale(1.03)} }
        @keyframes fireFlicker  { from{transform:scale(0.91)rotate(-3deg);opacity:.78} to{transform:scale(1.07)rotate(3deg);opacity:1} }
        @keyframes flameTongue1 { from{transform:rotate(-10deg)scaleY(0.85)} to{transform:rotate(7deg)scaleY(1.15)} }
        @keyframes flameTongue2 { from{transform:rotate(8deg)scaleY(0.88)} to{transform:rotate(-6deg)scaleY(1.12)} }
        @keyframes flameTongue3 { from{transform:rotate(-5deg)scaleY(0.80)} to{transform:rotate(5deg)scaleY(1.20)} }
        @keyframes ember        { 0%{transform:translateY(0)scale(1);opacity:1} 80%{transform:translateY(-22px)scale(0.4);opacity:.5} 100%{transform:translateY(-28px)scale(0);opacity:0} }
        @keyframes fireRing     { 0%{transform:translate(-50%,-50%)scale(1);opacity:.6} 100%{transform:translate(-50%,-50%)scale(1.5);opacity:0} }
      `}</style>
      <div style={{
        position: "absolute", width: diameter, height: diameter,
        left: "50%", top: "50%",
        transform: "translate(-50%, -50%)",
        borderRadius: "50%", overflow: "visible", zIndex: 1,
      }}>
        {/* Expanding ring pulse */}
        <div style={{
          position: "absolute", width: diameter, height: diameter,
          left: "50%", top: "50%",
          borderRadius: "50%",
          border: "2px solid rgba(255,140,0,0.6)",
          animation: "fireRing 1.2s ease-out infinite",
        }} />
        {/* Outer glow */}
        <div style={{
          position: "absolute", inset: 0, borderRadius: "50%",
          background: "radial-gradient(circle, rgba(255,130,0,0.55) 0%, rgba(220,65,0,0.35) 45%, rgba(180,30,0,0.15) 70%, transparent 90%)",
          boxShadow: `0 0 ${r * 0.8}px ${r * 0.4}px rgba(255,100,0,0.35), 0 0 ${r * 1.5}px rgba(255,50,0,0.15)`,
          animation: "fireGlow 1.2s ease-in-out infinite alternate",
        }} />
        {/* Flame core */}
        <div style={{
          position: "absolute", inset: "10%", borderRadius: "50%",
          background: "radial-gradient(ellipse at 50% 65%, rgba(255,220,60,0.90) 0%, rgba(255,140,0,0.75) 30%, rgba(220,60,0,0.50) 60%, transparent 100%)",
          animation: "fireFlicker 0.8s ease-in-out infinite alternate",
        }} />
        {/* Flame tongue 1 */}
        <div style={{
          position: "absolute", width: "32%", height: "55%", bottom: "22%", left: "18%",
          borderRadius: "50% 50% 28% 28%",
          background: "linear-gradient(to top, rgba(255,220,50,0.90), rgba(255,130,0,0.55), transparent)",
          transformOrigin: "50% 100%",
          animation: "flameTongue1 1s ease-in-out infinite alternate",
        }} />
        {/* Flame tongue 2 */}
        <div style={{
          position: "absolute", width: "30%", height: "48%", bottom: "22%", right: "16%",
          borderRadius: "50% 50% 28% 28%",
          background: "linear-gradient(to top, rgba(255,200,40,0.85), rgba(255,110,0,0.45), transparent)",
          transformOrigin: "50% 100%",
          animation: "flameTongue2 1.25s ease-in-out infinite alternate",
        }} />
        {/* Flame tongue 3 center */}
        <div style={{
          position: "absolute", width: "24%", height: "62%", bottom: "22%", left: "38%",
          borderRadius: "50% 50% 28% 28%",
          background: "linear-gradient(to top, rgba(255,240,90,0.95), rgba(255,160,20,0.60), transparent)",
          transformOrigin: "50% 100%",
          animation: "flameTongue3 0.75s ease-in-out infinite alternate",
        }} />
        {/* Ground char circle */}
        <div style={{
          position: "absolute", inset: "20%", borderRadius: "50%",
          background: "radial-gradient(circle, rgba(30,10,0,0.45) 0%, transparent 70%)",
        }} />
        {/* Embers */}
        {[
          { left: "28%", delay: "0s",   dur: "1.6s" },
          { left: "52%", delay: "0.3s", dur: "1.9s" },
          { left: "42%", delay: "0.6s", dur: "1.4s" },
          { left: "63%", delay: "1.0s", dur: "2.2s" },
          { left: "35%", delay: "1.4s", dur: "1.7s" },
        ].map((e, i) => (
          <div key={i} style={{
            position: "absolute", width: 3, height: 3, borderRadius: "50%",
            background: "rgba(255,230,80,0.95)", bottom: "28%", left: e.left,
            boxShadow: "0 0 5px 2px rgba(255,170,0,0.7)",
            animation: `ember ${e.dur} ${e.delay} ease-out infinite`,
          }} />
        ))}
        {/* FIRE label */}
        <div style={{
          position: "absolute", top: "50%", left: "50%",
          transform: "translate(-50%, -50%)",
          fontSize: Math.max(8, diameter * 0.11),
          fontWeight: 900,
          color: "rgba(255,240,180,0.95)",
          textShadow: "0 0 6px rgba(255,100,0,1), 0 1px 3px rgba(0,0,0,0.8)",
          letterSpacing: "0.06em",
          lineHeight: 1,
          pointerEvents: "none",
          zIndex: 3,
        }}>FIRE</div>
      </div>
    </>
  );
};

// ─── Config per type ──────────────────────────────────────────────────────────
const CFG = {
  smoke:      { dotColor: "#94a3b8", border: "rgba(148,163,184,0.7)", auraRadius: 144, label: "SMK" },
  flashbang:  { dotColor: "#fef08a", border: "rgba(254,240,138,0.8)", auraRadius: 0,   label: "FLB" },
  he:         { dotColor: "#fb923c", border: "rgba(251,146,60,0.8)",  auraRadius: 0,   label: "HE"  },
  molotov:    { dotColor: "#ef4444", border: "rgba(239,68,68,0.8)",   auraRadius: 150, label: "MOL" },
  incendiary: { dotColor: "#ef4444", border: "rgba(239,68,68,0.8)",   auraRadius: 150, label: "INC" },
  decoy:      { dotColor: "#a78bfa", border: "rgba(167,139,250,0.8)", auraRadius: 0,   label: "DCY" },
  inferno:    { dotColor: "#f97316", border: "rgba(249,115,22,0.9)",  auraRadius: 150, label: "FIRE"},
};

const Grenade = ({ grenadeData, mapData, mapWidth = 0, mapHeight = 0, settings }) => {
  const dotRef = useRef();

  if (!grenadeData || !mapWidth || !mapHeight || !mapData) return null;
  if (settings && !settings.showGrenades) return null;

  const type = grenadeData.type;
  const cfg  = CFG[type];
  if (!cfg) return null;

  const radarPos = getRadarPosition(mapData, { x: grenadeData.x, y: grenadeData.y });
  if (!radarPos || (radarPos.x <= 0 && radarPos.y <= 0)) return null;

  const dotSize = Math.max(12, mapWidth * 0.013);
  const { x: tx, y: ty } = getEntityTranslate(mapWidth, mapHeight, radarPos, dotSize, dotSize);

  const diamPx = radiusToPx(cfg.auraRadius, mapWidth, mapData.scale);

  const smokeDeployed = type === "smoke"   && grenadeData.m_did_smoke_effect;
  const showFire      = type === "inferno" && (settings?.showFire !== false);
  const showSmoke     = smokeDeployed      && (settings?.showSmoke !== false);
  const showDot       = !smokeDeployed;
  const isDecoyActive = type === "decoy"   && grenadeData.m_is_active;
  const showLabels    = settings?.showGrenadeLabels !== false;

  return (
    <div
      ref={dotRef}
      className="absolute left-0 top-0"
      style={{ transform: `translate(${tx}px, ${ty}px)`, zIndex: 120, pointerEvents: "none", width: dotSize, height: dotSize }}
    >
      {showSmoke && diamPx > 0 && <SmokeEffect diameter={diamPx} />}
      {showFire  && diamPx > 0 && <FireEffect  diameter={diamPx} />}

      {/* Decoy ping ring */}
      {isDecoyActive && (
        <>
          <style>{`@keyframes decoyPing{0%{transform:translate(-50%,-50%)scale(1);opacity:.8}100%{transform:translate(-50%,-50%)scale(2.5);opacity:0}}`}</style>
          <div style={{
            position: "absolute", width: 32, height: 32, left: "50%", top: "50%",
            transform: "translate(-50%, -50%)", borderRadius: "50%",
            border: `2px solid ${cfg.border}`,
            animation: "decoyPing 1.2s ease-out infinite",
          }} />
        </>
      )}

      {/* Projectile dot */}
      {showDot && (
        <div style={{
          width: dotSize, height: dotSize, borderRadius: "50%",
          background: cfg.dotColor, border: `2px solid ${cfg.border}`,
          boxShadow: `0 0 8px ${cfg.dotColor}, 0 0 3px rgba(0,0,0,0.9)`,
          position: "relative", zIndex: 2,
        }} />
      )}

      {/* Label */}
      {showDot && showLabels && (
        <div style={{
          position: "absolute", top: 16, left: "50%", transform: "translateX(-50%)",
          fontSize: 8, fontWeight: 900, color: cfg.dotColor,
          textShadow: "0 1px 4px rgba(0,0,0,1)", whiteSpace: "nowrap",
          letterSpacing: "0.04em", lineHeight: 1,
        }}>
          {cfg.label}
        </div>
      )}
    </div>
  );
};

export default Grenade;