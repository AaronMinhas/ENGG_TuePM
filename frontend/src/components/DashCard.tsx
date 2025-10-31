/**
 * This is a reusable component that will set the default layout of most of the cards on the dashboard.
 * Refer to the dashboard design in the repository README.
 */

import React from "react";
import { type LucideIcon, Car as CarIcon, Ship, Send, SendToBack, Activity, RadioTower } from "lucide-react";
import { CustomDropdown } from "./CustomDropdown";
import { Icon as IconKey } from "../types/GenTypes";
import { DashCardProp, Status } from "../types/DashCard";
import BridgeIcon from "./BridgeIcon";

export default function DashCard(props: Readonly<DashCardProp>) {
  const Icon = props.iconT ? iconMap[props.iconT] : null;

  if (props.variant === "DUAL_STATE") {
    const leftColour = getStatusColour(props.leftStatus);
    const rightColour = getStatusColour(props.rightStatus);
    const disabled = props.disabled ?? false;

    return (
      <div className="w-full h-full bg-white p-4 flex flex-col justify-center rounded-md border cursor-default border-base-400 shadow-[0_0_2px_rgba(0,0,0,0.25)] space-y-2">
        <div className="flex justify-between items-center">
          <div className="flex gap-2">
            {Icon ? <Icon size={22} className="text-gray-700" /> : null}
            <span className="font-semibold text-gray-900">{props.title}</span>
          </div>
        </div>

        <div className="grid grid-cols-2 gap-2">
          <div className="flex flex-col gap-1">
            <span className="text-xs text-gray-600">{props.leftLabel}</span>
            <CustomDropdown
              options={props.leftOptions}
              selected={props.leftStatus?.value ?? ""}
              colour={leftColour}
              disabled={disabled}
            />
          </div>
          <div className="flex flex-col gap-1">
            <span className="text-xs text-gray-600">{props.rightLabel}</span>
            <CustomDropdown
              options={props.rightOptions}
              selected={props.rightStatus?.value ?? ""}
              colour={rightColour}
              disabled={disabled}
            />
          </div>
        </div>

        <span className="text-sm text-gray-600">{props.updatedAt}</span>
      </div>
    );
  }

  const { title, description, options, updatedAt, variant, status } = props;
  const statusColour = getStatusColour(status);
  const disabled = props.variant === "STATE" ? props.disabled ?? false : false;

  return (
    <div className="w-full h-full bg-white p-4 flex flex-col justify-center rounded-md border cursor-default border-base-400 shadow-[0_0_2px_rgba(0,0,0,0.25)] space-y-2">
      <div className="flex justify-between items-center">
        <div className="flex gap-2">
          {Icon ? <Icon size={22} className="text-gray-700" /> : null}
          <span className="font-semibold text-gray-900">{title}</span>
        </div>
      </div>

      <div>
        {variant === "STATE" ? (
          <CustomDropdown
            options={options ?? []}
            selected={description}
            colour={statusColour}
            disabled={disabled}
          />
        ) : (
          <span className="text-2xl font-bold text-gray-900">{description}</span>
        )}
      </div>

      <span className="text-sm text-gray-600">{updatedAt}</span>
    </div>
  );
}

const iconMap: Record<IconKey, LucideIcon> = {
  BRIDGE: BridgeIcon,
  CAR: CarIcon,
  BOAT: Ship,
  SYSTEM: RadioTower,
  PACKETS_SEND: Send,
  PACKETS_REC: SendToBack,
  ACTIVITY: Activity,
};

function getStatusColour(status?: Status): string {
  if (!status) return "bg-gray-400";

  switch (status.kind) {
    case "car":
      if (status.value === "Green") return "bg-green-500";
      if (status.value === "Yellow") return "bg-yellow-400";
      if (status.value === "Red") return "bg-red-500";
      break;

    case "boat":
      if (status.value === "Green") return "bg-green-500";
      if (status.value === "Red") return "bg-red-500";
      break;

    case "system":
      if (status.value === "Connected") return "bg-green-500";
      if (status.value === "Connecting") return "bg-blue-500";
      if (status.value === "Disconnected") return "bg-red-500";
      break;

    case "bridge":
      // Idle/Safe states
      if (status.value === "IDLE") return "bg-green-500";
      // Traffic management states
      if (status.value === "STOPPING_TRAFFIC") return "bg-yellow-500";
      if (status.value === "RESUMING_TRAFFIC") return "bg-yellow-400";
      // Bridge operation states
      if (status.value === "OPENING") return "bg-orange-500";
      if (status.value === "OPEN") return "bg-orange-300";
      if (status.value === "CLOSING") return "bg-yellow-400";
      // Manual control states
      if (status.value === "MANUAL_MODE") return "bg-blue-400";
      if (status.value === "MANUAL_OPENING") return "bg-blue-500";
      if (status.value === "MANUAL_OPEN") return "bg-blue-300";
      if (status.value === "MANUAL_CLOSING") return "bg-blue-400";
      if (status.value === "MANUAL_CLOSED") return "bg-blue-300";
      // Error state
      if (status.value === "FAULT") return "bg-red-500";
      break;
  }
  return "bg-gray-400";
}
