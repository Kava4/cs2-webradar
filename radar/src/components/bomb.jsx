import { useRef } from "react";
import { getRadarPosition, getEntityTranslate, teamEnum } from "../utilities/utilities";

const Bomb = ({ bombData, mapData, mapWidth = 0, mapHeight = 0, localTeam, averageLatency, settings }) => {
  const radarPosition = getRadarPosition(mapData, bombData);

  const bombRef = useRef();
  const scaledSize = Math.max(12, mapWidth * 0.018 * (settings?.bombSize ?? 1));
  const { x, y } = getEntityTranslate(mapWidth, mapHeight, radarPosition, scaledSize, scaledSize);

  return (
    <div
      className="absolute origin-center rounded-[100%] left-0 top-0"
      ref={bombRef}
      style={{
        width: scaledSize,
        height: scaledSize,
        transform: `translate(${x}px, ${y}px)`,
        transition: `transform ${averageLatency}ms linear`,
        backgroundColor: `${
          (bombData.m_is_defused && `#50904c`) ||
          (localTeam == teamEnum.counterTerrorist && `#6492b4`) ||
          `#c90b0b`
        }`,
        WebkitMask: `url('./assets/icons/c4_sml.png') no-repeat center / contain`,
        opacity: `1`,
        zIndex: `250`,
      }}
    />
  );
};

export default Bomb;
