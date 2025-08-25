/**
 * This is a reusable component that will set the default layout of most of the cards on the dashboard.
 * Refer to the dashboard design in the repository README.
 */

import { Settings, Car, Ship, Send, SendToBack, Activity, RadioTower } from "lucide-react";
import BridgeIcon from "./BridgeIcon";

type DashCardProp = {
  title: string;
  description: string;
  updatedAt: string;
  cardType?: "STATE" | null;
  iconType?: "BRIDGE" | "CAR" | "BOAT" | "SYSTEM" | "PACKETS_SEND" | "PACKETS_REC" | "ACTIVITY" | "";
  bridgeStateType?: "OPENING" | "CLOSING" | "CLOSED";
  systemStateType?: "CONNECTED" | "CONNECTING" | "DISCONNECTED";
  carStateType?: "GREEN" | "YELLOW" | "RED";
  boatStateType?: "GREEN" | "RED";
};

export default function DashCard(props: Readonly<DashCardProp>) {
  const {
    title,
    description,
    updatedAt,
    cardType,
    iconType = "",
    bridgeStateType,
    systemStateType,
    carStateType,
    boatStateType,
  } = props;

  const Icon = iconMap[iconType] ?? (() => null);

  const statusColour = getStateColour(carStateType, boatStateType, systemStateType, bridgeStateType);

  return (
    <div className="w-full h-35 bg-white p-4 flex flex-col justify-center rounded-md border cursor-default border-base-400 shadow-[0_0_2px_rgba(0,0,0,0.25)] space-y-2">
      <div className="flex justify-between items-center">
        <div className="flex gap-2">
          <Icon size={22} />
          <span className="font-semibold">{title}</span>
        </div>

        <button className="cursor-pointer">
          <Settings size={20} />
        </button>
      </div>

      <div>
        {cardType === "STATE" ? (
          <div className="flex items-center gap-2">
            <div className={`${statusColour} w-6 h-6 rounded-full`} />
            <span className="text-2xl font-bold">{description}</span>
          </div>
        ) : (
          <span className="text-2xl font-bold">{description}</span>
        )}
      </div>

      <span className="text-sm">{updatedAt}</span>
    </div>
  );
}

const iconMap: Record<NonNullable<DashCardProp["iconType"]>, React.ElementType> = {
  BRIDGE: BridgeIcon,
  CAR: Car,
  BOAT: Ship,
  SYSTEM: RadioTower,
  PACKETS_SEND: Send,
  PACKETS_REC: SendToBack,
  ACTIVITY: Activity,
  "": () => null,
};

function getStateColour(
  carStateType?: DashCardProp["carStateType"],
  boatStateType?: DashCardProp["boatStateType"],
  systemStateType?: DashCardProp["systemStateType"],
  bridgeStateType?: DashCardProp["bridgeStateType"]
) {
  if (carStateType) {
    switch (carStateType) {
      case "GREEN":
        return "bg-green-500";
      case "YELLOW":
        return "bg-yellow-400";
      case "RED":
        return "bg-red-500";
    }
  }
  if (boatStateType) {
    switch (boatStateType) {
      case "GREEN":
        return "bg-green-500";
      case "RED":
        return "bg-red-500";
    }
  }
  if (systemStateType) {
    switch (systemStateType) {
      case "CONNECTED":
        return "bg-green-500";
      case "CONNECTING":
        return "bg-blue-500";
      case "DISCONNECTED":
        return "bg-red-500";
    }
  }
  if (bridgeStateType) {
    switch (bridgeStateType) {
      case "OPENING":
        return "bg-orange-400";
      case "CLOSING":
        return "bg-yellow-400";
      case "CLOSED":
        return "bg-gray-400";
    }
  }
  return "bg-gray-400";
}
