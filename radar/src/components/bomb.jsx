import { useRef } from "react";
import { getRadarPosition, getEntityTranslate, teamEnum } from "../utilities/utilities";
import MaskedIcon from "./maskedicon";

const Bomb = ({ bombData, mapData, mapWidth = 0, mapHeight = 0, localTeam, averageLatency, settings }) => {
  const radarPosition = getRadarPosition(mapData, bombData);

  const bombRef = useRef();
  const scaledSize = Math.max(12, mapWidth * 0.018 * (settings?.bombSize ?? 1));
  const { x, y } = getEntityTranslate(mapWidth, mapHeight, radarPosition, scaledSize, scaledSize);
  const bg = (bombData.m_is_defused && "#50904c")
    || (localTeam == teamEnum.counterTerrorist && "#6492b4")
    || "#c90b0b";

  return (
    <div
      className="absolute origin-center left-0 top-0"
      ref={bombRef}
      style={{
        width: scaledSize,
        height: scaledSize,
        transform: `translate(${x}px, ${y}px)`,
        transition: `transform ${averageLatency}ms linear`,
        zIndex: 250,
        display: "flex",
        alignItems: "center",
        justifyContent: "center",
      }}
    >
      <div style={{
        width: scaledSize,
        height: scaledSize,
        borderRadius: "50%",
        backgroundColor: bg,
        border: "1px solid rgba(0,0,0,0.65)",
        display: "flex",
        alignItems: "center",
        justifyContent: "center",
      }}>
        <MaskedIcon path="./assets/icons/c4.svg" height={scaledSize * 0.68} color="bg-white" />
      </div>
    </div>
  );
};

export default Bomb;
