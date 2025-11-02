import React from "react";
import { SensorStatus } from "../lib/schema";
import { Radio, Navigation } from "lucide-react";

interface SensorsCardProps {
  readonly sensorStatus: SensorStatus | null;
}

function getZoneColor(zone: string): string {
  switch (zone) {
    case "close":
      return "text-red-500";
    case "near":
      return "text-yellow-500";
    case "far":
      return "text-blue-500";
    default:
      return "text-gray-400";
  }
}

function getZoneBgColor(zone: string): string {
  switch (zone) {
    case "close":
      return "bg-red-100 border-red-300";
    case "near":
      return "bg-yellow-100 border-yellow-300";
    case "far":
      return "bg-blue-100 border-blue-300";
    default:
      return "bg-gray-100 border-gray-300";
  }
}

export default function SensorsCard({ sensorStatus }: SensorsCardProps) {
  const leftDistance = sensorStatus?.leftUltrasonic.distanceCm ?? -1;
  const rightDistance = sensorStatus?.rightUltrasonic.distanceCm ?? -1;
  const leftZone = sensorStatus?.leftUltrasonic.zone ?? "none";
  const rightZone = sensorStatus?.rightUltrasonic.zone ?? "none";
  const beamBreak = sensorStatus?.beamBreak ?? false;
  const direction = sensorStatus?.direction ?? "none";

  return (
    <div className="bg-white rounded-lg shadow-md p-3 h-full flex flex-col overflow-hidden">
      {/* Header */}
      <div className="flex items-center gap-2 mb-2 flex-shrink-0">
        <Radio className="w-4 h-4 text-purple-600" />
        <h3 className="font-semibold text-gray-800 text-sm">Sensors</h3>
      </div>

      {/* Sensors - fill entire height */}
      <div className="flex-1 flex flex-col gap-2 min-h-0">
        {/* Left Ultrasonic */}
        <div className={`border-2 rounded-lg p-2 flex-1 flex flex-col ${getZoneBgColor(leftZone)}`}>
          <div className="flex items-center justify-between mb-0.5">
            <span className="text-xs font-medium text-gray-700">Left Ultrasonic</span>
            <span className={`text-xs font-bold uppercase ${getZoneColor(leftZone)}`}>
              {leftZone}
            </span>
          </div>
          <div className="flex-1 flex items-center justify-center text-xl font-bold text-gray-800">
            {leftDistance >= 0 ? `${leftDistance.toFixed(1)} cm` : "—"}
          </div>
        </div>

        {/* Right Ultrasonic */}
        <div className={`border-2 rounded-lg p-2 flex-1 flex flex-col ${getZoneBgColor(rightZone)}`}>
          <div className="flex items-center justify-between mb-0.5">
            <span className="text-xs font-medium text-gray-700">Right Ultrasonic</span>
            <span className={`text-xs font-bold uppercase ${getZoneColor(rightZone)}`}>
              {rightZone}
            </span>
          </div>
          <div className="flex-1 flex items-center justify-center text-xl font-bold text-gray-800">
            {rightDistance >= 0 ? `${rightDistance.toFixed(1)} cm` : "—"}
          </div>
        </div>

        {/* Beam Break Sensor */}
        <div
          className={`border-2 rounded-lg p-2 flex-1 flex flex-col ${
            beamBreak ? "bg-red-100 border-red-300" : "bg-green-100 border-green-300"
          }`}
        >
          <div className="flex items-center justify-between mb-0.5">
            <span className="text-xs font-medium text-gray-700">Beam Break</span>
            <span
              className={`text-xs font-bold uppercase ${
                beamBreak ? "text-red-500" : "text-green-500"
              }`}
            >
              {beamBreak ? "ACTIVE" : "CLEAR"}
            </span>
          </div>
          <div className="flex-1 flex items-center justify-center text-base font-bold text-gray-800">
            {beamBreak ? "Boat Crossing" : "Channel Clear"}
          </div>
        </div>

        {/* Direction Indicator */}
        {direction !== "none" && (
          <div className="border-2 rounded-lg p-2 bg-indigo-100 border-indigo-300 flex-shrink-0">
            <div className="flex items-center gap-1.5">
              <Navigation className="w-3 h-3 text-indigo-600 flex-shrink-0" />
              <span className="text-xs font-medium text-gray-700">Direction:</span>
              <span className="text-xs font-bold text-indigo-600 uppercase truncate">
                {direction.replace("-", " → ")}
              </span>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

