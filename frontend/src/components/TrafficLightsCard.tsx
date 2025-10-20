import React from "react";
import {
  CarTrafficStatus,
  BoatTrafficStatus,
  CarTrafficState,
  BoatTrafficState,
} from "../lib/schema";
import { CustomDropdown } from "./CustomDropdown";
import { timeAgo } from "../utils/timeAgo";

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
      className={`w-full bg-white px-4 py-3 flex flex-col rounded-md border border-base-300 shadow-[0_0_1px_rgba(0,0,0,0.2)] space-y-3 ${
        disabled ? "opacity-80" : ""
      } ${className}`}
    >
      <div className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <span className="font-semibold">Traffic Lights</span>
          {carUpdatedAt || boatUpdatedAt ? (
            <p className="text-xs text-gray-500 mt-0.5">
              Car: {carUpdatedAt || "—"} • Boat: {boatUpdatedAt || "—"}
            </p>
          ) : null}
        </div>
      </div>

      <div className="grid gap-3 md:grid-cols-[1fr_1fr]">
        <section className="flex flex-col gap-2">
          <p className="text-xs font-semibold text-gray-600 uppercase tracking-wide">Car Traffic</p>
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
          <p className="text-[11px] text-gray-500">
            L:{carTrafficStatus?.left.value ?? "?"} • R:{carTrafficStatus?.right.value ?? "?"}
          </p>
        </section>

        <section className="flex flex-col gap-2">
          <p className="text-xs font-semibold text-gray-600 uppercase tracking-wide">Boat Traffic</p>
          <div className="grid gap-2 sm:grid-cols-2">
            <CustomDropdown
              options={[
                { id: "traffic-boat-left-red", label: "Left Red", action: () => onBoatTrafficChange("left", "Red") },
                { id: "traffic-boat-left-green", label: "Left Green", action: () => onBoatTrafficChange("left", "Green") },
              ]}
              selected={boatLeftValue}
              colour={getBoatColour(boatLeftValue as BoatTrafficState | "")}
              disabled={disabled}
              compact
            />
            <CustomDropdown
              options={[
                { id: "traffic-boat-right-red", label: "Right Red", action: () => onBoatTrafficChange("right", "Red") },
                { id: "traffic-boat-right-green", label: "Right Green", action: () => onBoatTrafficChange("right", "Green") },
              ]}
              selected={boatRightValue}
              colour={getBoatColour(boatRightValue as BoatTrafficState | "")}
              disabled={disabled}
              compact
            />
          </div>
        </section>
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
