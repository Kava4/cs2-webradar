import { getRadarPosition } from "../utilities/utilities";

// NEW FEATURE 8: Smoke Grenade Visualization
export const SmokeOverlay = ({ smokeData, mapData, radarImage }) => {
  if (!smokeData || !mapData || !radarImage) return null;

  const position = { x: smokeData.m_x, y: smokeData.m_y };
  const radarPosition = getRadarPosition(mapData, position);
  
  if (!radarPosition || radarPosition.x <= 0 && radarPosition.y <= 0) return null;

  const radarImageBounding = radarImage.getBoundingClientRect() || { width: 0, height: 0 };
  
  const translation = {
    x: radarImageBounding.width * radarPosition.x,
    y: radarImageBounding.height * radarPosition.y,
  };

  // Smoke radius in game units (~144 units), scale to map
  const smokeRadius = 144 * (radarImageBounding.width / 10000);

  return (
    <div
      className="absolute pointer-events-none"
      style={{
        left: `${translation.x}px`,
        top: `${translation.y}px`,
        transform: 'translate(-50%, -50%)',
        zIndex: 50,
      }}
    >
      {/* Smoke cloud effect */}
      <div 
        className="rounded-full animate-pulse"
        style={{
          width: `${smokeRadius}px`,
          height: `${smokeRadius}px`,
          background: 'radial-gradient(circle, rgba(156, 163, 175, 0.6) 0%, rgba(156, 163, 175, 0.3) 50%, transparent 100%)',
          backdropFilter: 'blur(4px)',
          boxShadow: '0 0 20px rgba(156, 163, 175, 0.5), inset 0 0 20px rgba(156, 163, 175, 0.3)',
        }}
      />
      
      {/* Smoke icon */}
      <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 text-2xl">
        💨
      </div>
    </div>
  );
};

// NEW FEATURE 9: Molotov/Incendiary Fire Visualization
export const InfernoOverlay = ({ infernoData, mapData, radarImage }) => {
  if (!infernoData || !mapData || !radarImage) return null;

  const position = { x: infernoData.m_x, y: infernoData.m_y };
  const radarPosition = getRadarPosition(mapData, position);
  
  if (!radarPosition || radarPosition.x <= 0 && radarPosition.y <= 0) return null;

  const radarImageBounding = radarImage.getBoundingClientRect() || { width: 0, height: 0 };
  
  const translation = {
    x: radarImageBounding.width * radarPosition.x,
    y: radarImageBounding.height * radarPosition.y,
  };

  // Fire spread radius (~150 units)
  const fireRadius = 150 * (radarImageBounding.width / 10000);

  return (
    <div
      className="absolute pointer-events-none"
      style={{
        left: `${translation.x}px`,
        top: `${translation.y}px`,
        transform: 'translate(-50%, -50%)',
        zIndex: 50,
      }}
    >
      {/* Fire effect with multiple layers */}
      <div 
        className="rounded-full"
        style={{
          width: `${fireRadius}px`,
          height: `${fireRadius}px`,
          background: 'radial-gradient(circle, rgba(249, 115, 22, 0.7) 0%, rgba(239, 68, 68, 0.5) 40%, rgba(220, 38, 38, 0.3) 70%, transparent 100%)',
          animation: 'pulse 1s ease-in-out infinite',
          boxShadow: '0 0 30px rgba(249, 115, 22, 0.8), inset 0 0 20px rgba(249, 115, 22, 0.4)',
        }}
      />
      
      {/* Inner fire core */}
      <div 
        className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 rounded-full"
        style={{
          width: `${fireRadius * 0.5}px`,
          height: `${fireRadius * 0.5}px`,
          background: 'radial-gradient(circle, rgba(251, 191, 36, 0.9) 0%, rgba(249, 115, 22, 0.6) 100%)',
          animation: 'ping 2s cubic-bezier(0, 0, 0.2, 1) infinite',
        }}
      />
      
      {/* Fire icon */}
      <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 text-2xl animate-pulse">
        🔥
      </div>
    </div>
  );
};