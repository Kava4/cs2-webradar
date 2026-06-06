import { useRef, useState, useEffect } from "react";
import { getRadarPosition, getEntityTranslate, playerColors } from "../utilities/utilities";

// ── Smooth rotation tracker ───────────────────────────────────────────────────
let playerRotations = [];
const getPlayerRotation = (playerData) => {
  const target = 270 - playerData.m_eye_angle;
  const idx    = playerData.m_idx;
  playerRotations[idx] = (playerRotations[idx] || 0) % 360;
  playerRotations[idx] += ((target - playerRotations[idx] + 540) % 360) - 180;
  return playerRotations[idx];
};

// ── Player dot ────────────────────────────────────────────────────────────────
const Player = ({ playerData, mapData, mapWidth = 0, mapHeight = 0, localTeam, averageLatency, settings }) => {
  const playerRef = useRef();
  const [lastPos, setLastPos] = useState(null);

  const radarPos = getRadarPosition(mapData, playerData.m_position) || { x: 0, y: 0 };
  const invalid  = radarPos.x <= 0 && radarPos.y <= 0;
  const rotation = getPlayerRotation(playerData);

  const avatarUrl = playerData.m_avatar_url || null;

  useEffect(() => {
    if (playerData.m_is_dead) { if (!lastPos) setLastPos(radarPos); }
    else setLastPos(null);
  }, [playerData.m_is_dead]);

  const effectivePos = playerData.m_is_dead ? (lastPos || { x: 0, y: 0 }) : radarPos;

  const dotPx  = Math.max(10, mapWidth * 0.014 * (settings?.dotSize ?? 1));
  const { x: tx, y: ty } = getEntityTranslate(mapWidth, mapHeight, effectivePos, dotPx, dotPx);

  const isAlly    = playerData.m_team === localTeam;
  const teamColor = isAlly ? (playerColors[playerData.m_color] ?? "#84c8ed") : "#FF4444";
  const isDead    = playerData.m_is_dead;
  const coneScale = settings?.coneLength ?? 1;

  return (
    <div
      ref={playerRef}
      className="absolute left-0 top-0"
      style={{
        width: dotPx, height: dotPx,
        transform: `translate(${tx}px, ${ty}px)`,
        transition: `transform ${averageLatency}ms linear`,
        zIndex: isDead ? 1 : 10,
        opacity: invalid ? 0 : 1,
        filter: isDead ? "grayscale(100%) opacity(0.45)" : "none",
        pointerEvents: "none",
      }}
    >
      {!isDead && ((settings?.showAllNames && isAlly) || (settings?.showEnemyNames && !isAlly)) && (
        <div style={{
          position: "absolute", bottom: "108%", left: "50%",
          transform: "translateX(-50%)",
          whiteSpace: "nowrap", pointerEvents: "none",
        }}>
          <span style={{
            fontSize: Math.max(9, dotPx * 0.45), fontWeight: 900, lineHeight: 1,
            color: teamColor,
            textShadow: "0 1px 4px rgba(0,0,0,1), 0 0 8px rgba(0,0,0,0.9)",
            background: "rgba(0,0,0,0.55)",
            padding: "1px 4px", borderRadius: 3,
          }}>
            {playerData.m_name}
          </span>
        </div>
      )}

      <div style={{
        width: dotPx, height: dotPx,
        transform: `rotate(${isDead ? 0 : rotation}deg)`,
        transition: `transform ${averageLatency}ms linear`,
      }}>
        {settings?.showViewCones && !isDead && (
          <div style={{
            position: "absolute",
            left: "50%",
            top: "50%",
            width:  dotPx * coneScale * 3.5,
            height: dotPx * coneScale * 5.5,
            transform: "translate(-50%, 0%)",
            clipPath: "polygon(50% 0%, 15% 100%, 85% 100%)",
            background: `linear-gradient(to bottom, ${teamColor}cc 0%, ${teamColor}22 60%, transparent 100%)`,
            zIndex: 1,
            pointerEvents: "none",
          }} />
        )}

        {!isDead && (
          <div style={{
            position: "absolute",
            bottom: "-35%",
            left: "50%",
            transform: "translateX(-50%)",
            width: 0,
            height: 0,
            borderLeft:  `${dotPx * 0.22}px solid transparent`,
            borderRight: `${dotPx * 0.22}px solid transparent`,
            borderTop: `${dotPx * 0.38}px solid ${teamColor}`,
            filter: `drop-shadow(0 0 2px ${teamColor})`,
            zIndex: 4,
          }} />
        )}

        <div style={{
          position: "absolute", inset: 0,
          borderRadius: "50%",
          overflow: "hidden",
          border: `${Math.max(1, dotPx * 0.13)}px solid ${teamColor}`,
          boxShadow: isDead
            ? "none"
            : `0 0 0 1px rgba(0,0,0,0.8), 0 0 ${dotPx * 3}px ${teamColor}55`,
          background: "#0a0e1a",
          zIndex: 2,
        }}>
          {avatarUrl && (
            <img
              src={avatarUrl}
              alt=""
              style={{
                width: "100%", height: "100%",
                objectFit: "cover",
                filter: isDead ? "grayscale(100%) brightness(0.4)" : "none",
              }}
              onError={(e) => { e.currentTarget.style.display = "none"; }}
            />
          )}

          {!avatarUrl && playerData.m_model_name && (
            <img
              src={`./assets/characters/${playerData.m_model_name}.png`}
              alt=""
              style={{
                position: "absolute", inset: 0,
                width: "100%", height: "100%",
                objectFit: "cover", objectPosition: "top center",
                transform: "scale(1.8) translateY(-10%)",
                transformOrigin: "top center",
                filter: isDead ? "grayscale(100%) brightness(0.4)" : "saturate(0.9)",
              }}
              onError={(e) => { e.currentTarget.style.display = "none"; }}
            />
          )}

          {!avatarUrl && !playerData.m_model_name && (
            <div style={{
              position: "absolute", inset: 0,
              background: `radial-gradient(circle at 40% 35%, ${teamColor}cc, ${teamColor}66)`,
            }} />
          )}
        </div>
      </div>

      {!isDead && playerData.m_health < 100 && (
        <div style={{
          position: "absolute", top: "110%", left: "50%",
          transform: "translateX(-50%)",
          width: "130%", height: 2,
          background: "rgba(0,0,0,0.7)",
          borderRadius: 1,
          overflow: "hidden",
        }}>
          <div style={{
            height: "100%",
            width: `${playerData.m_health}%`,
            background: playerData.m_health > 50 ? "#22c55e"
              : playerData.m_health > 25 ? "#eab308" : "#ef4444",
            transition: "width 0.3s",
          }} />
        </div>
      )}
    </div>
  );
};

export default Player;
