/**
 * This is a reusable component that will set the default layout of most of the cards on the dashboard.
 * Refer to the dashboard design in the repository README.
 */

import { Settings, Car, Ship, Send, SendToBack, Activity, RadioTower } from "lucide-react";
import { CustomDropdown } from "./CustomDropdown";
import BridgeIcon from "./BridgeIcon";

type DashCardProp = {
  title: string;
  description: string;
  options?: string[];
  updatedAt: string;
  cardType?: "STATE" | null;
  iconType?:
    | "BRIDGE"
    | "CAR"
    | "BOAT"
    | "SYSTEM"
    | "PACKETS_SEND"
    | "PACKETS_REC"
    | "ACTIVITY"
    | "";
  bridgeStateType?: "Opening" | "Closing" | "Closed";
  systemStateType?: "Connected" | "Connecting" | "Disconnected";
  carStateType?: "Green" | "Yellow" | "Red";
  boatStateType?: "Green" | "Red";
};

export default function DashCard(props: Readonly<DashCardProp>) {
  const {
    title,
    description,
    options,
    updatedAt,
    cardType,
    iconType = "",
    bridgeStateType,
    systemStateType,
    carStateType,
    boatStateType,
  } = props;

  const Icon = iconMap[iconType] ?? (() => null);

  const statusColour = getStateColour(
    carStateType,
    boatStateType,
    systemStateType,
    bridgeStateType
  );

  return (
    <div className="w-full h-35 bg-white p-4 flex flex-col justify-center rounded-md border cursor-default border-base-400 shadow-[0_0_2px_rgba(0,0,0,0.25)] space-y-2">
      <div className="flex justify-between items-center">
        <div className="flex gap-2">
          <Icon size={22} />
          <span className="font-semibold">{title}</span>
        </div>

        {/* <button className="cursor-pointer">
          <Settings size={20} />
        </button> */}
      </div>

      <div>
        {cardType === "STATE" ? (
          <CustomDropdown
            options={options}
            selected={description}
            onSelect={(val) => {
              // need a central var --> map it to thing
            }}
          />
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
      case "Green":
        return "bg-green-500";
      case "Yellow":
        return "bg-yellow-400";
      case "Red":
        return "bg-red-500";
    }
  }
  if (boatStateType) {
    switch (boatStateType) {
      case "Green":
        return "bg-green-500";
      case "Red":
        return "bg-red-500";
    }
  }
  if (systemStateType) {
    switch (systemStateType) {
      case "Connected":
        return "bg-green-500";
      case "Connecting":
        return "bg-blue-500";
      case "Disconnected":
        return "bg-red-500";
    }
  }
  if (bridgeStateType) {
    switch (bridgeStateType) {
      case "Opening":
        return "bg-orange-400";
      case "Closing":
        return "bg-yellow-400";
      case "Closed":
        return "bg-gray-400";
    }
  }
  return "bg-gray-400";
}

function Dropdown() {
  return (
    <div>
      <div></div>
    </div>
  );
}
