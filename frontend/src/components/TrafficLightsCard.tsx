import React from "react";
import {
  CarTrafficStatus,
  BoatTrafficStatus,
  CarTrafficState,
  BoatTrafficState,
} from "../lib/schema";
import { CustomDropdown } from "./CustomDropdown";
import { timeAgo } from "../utils/timeAgo";
import { TrafficCone, Anchor } from "lucide-react";

interface TrafficLightsCardProps {
  carTrafficStatus: CarTrafficStatus | null;
  boatTrafficStatus: BoatTrafficStatus | null;
  onCarTrafficChange: (value: CarTrafficState) => void;
  onBoatTrafficChange: (side: "left" | "right", value: BoatTrafficState) => void;
  disabled?: boolean;
  className?: string;
}

export default function TrafficLightsCard({
  carTrafficStatus,
  boatTrafficStatus,
  onCarTrafficChange,
  onBoatTrafficChange,
  disabled = false,
  className = "",
}: Readonly<TrafficLightsCardProps>) {
  const carValue = carTrafficStatus?.left.value ?? carTrafficStatus?.right.value ?? "";
  const carUpdatedAt = carTrafficStatus?.left.receivedAt
    ? timeAgo(carTrafficStatus.left.receivedAt)
    : "";

  const boatLeftValue = boatTrafficStatus?.left.value ?? "";
  const boatRightValue = boatTrafficStatus?.right.value ?? "";
  const boatUpdatedAt = boatTrafficStatus?.left.receivedAt
    ? timeAgo(boatTrafficStatus.left.receivedAt)
    : "";

  return (
    <div
      className={`w-full rounded-md border border-base-300 bg-gray-50 p-3 ${
        disabled ? "opacity-80" : ""
      } ${className}`}
    >
      <div className="flex items-center gap-2 mb-2">
        <TrafficCone size={14} className="text-orange-600" />
        <p className="text-sm font-semibold text-gray-900">Traffic Lights</p>
      </div>

      <div className="grid gap-2 grid-cols-1 sm:grid-cols-2">
        {/* Car Traffic */}
        <div className="space-y-1.5">
          <div className="flex items-center justify-between">
            <p className="text-[11px] font-semibold text-gray-700 uppercase">Car</p>
          </div>
          <CustomDropdown
            options={[
              { id: "traffic-car-red", label: "Red", action: () => onCarTrafficChange("Red") },
              { id: "traffic-car-yellow", label: "Yellow", action: () => onCarTrafficChange("Yellow") },
              { id: "traffic-car-green", label: "Green", action: () => onCarTrafficChange("Green") },
            ]}
            selected={carValue}
            colour={getCarColour(carValue as CarTrafficState | "")}
            disabled={disabled}
            compact
          />
        </div>

        {/* Boat Traffic */}
        <div className="space-y-1.5">
          <p className="text-[11px] font-semibold text-gray-700 uppercase">Boat</p>
          <div className="grid gap-1.5 grid-cols-2">
            <CustomDropdown
              options={[
                { id: "traffic-boat-left-red", label: "L Red", action: () => onBoatTrafficChange("left", "Red") },
                { id: "traffic-boat-left-green", label: "L Green", action: () => onBoatTrafficChange("left", "Green") },
              ]}
              selected={boatLeftValue}
              colour={getBoatColour(boatLeftValue as BoatTrafficState | "")}
              disabled={disabled}
              compact
            />
            <CustomDropdown
              options={[
                { id: "traffic-boat-right-red", label: "R Red", action: () => onBoatTrafficChange("right", "Red") },
                { id: "traffic-boat-right-green", label: "R Green", action: () => onBoatTrafficChange("right", "Green") },
              ]}
              selected={boatRightValue}
              colour={getBoatColour(boatRightValue as BoatTrafficState | "")}
              disabled={disabled}
              compact
            />
          </div>
        </div>
      </div>
    </div>
  );
}

function getCarColour(value: CarTrafficState | ""): string {
  switch (value) {
    case "Red":
      return "bg-red-500";
    case "Yellow":
      return "bg-yellow-400";
    case "Green":
      return "bg-green-500";
    default:
      return "bg-gray-400";
  }
}

function getBoatColour(value: BoatTrafficState | ""): string {
  switch (value) {
    case "Red":
      return "bg-red-500";
    case "Green":
      return "bg-green-500";
    default:
      return "bg-gray-400";
  }
}
