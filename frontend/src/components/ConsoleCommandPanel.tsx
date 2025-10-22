import React, { useEffect, useState } from "react";
import { Terminal } from "lucide-react";
import TrafficLightsCard from "./TrafficLightsCard";
import {
  CarTrafficStatus,
  BoatTrafficStatus,
  CarTrafficState,
  BoatTrafficState,
} from "../lib/schema";

interface ConsoleCommandPanelProps {
  onSend: (command: string) => Promise<{ ok: boolean; message?: string; error?: string }>;
  sending: boolean;
  simulationActive: boolean;
  carTrafficStatus: CarTrafficStatus | null;
  boatTrafficStatus: BoatTrafficStatus | null;
  onSimCarTraffic: (value: CarTrafficState) => Promise<{ ok: boolean; message?: string; error?: string }>;
  onSimBoatTraffic: (
    side: "left" | "right",
    value: BoatTrafficState
  ) => Promise<{ ok: boolean; message?: string; error?: string }>;
}

interface FeedbackState {
  type: "success" | "error";
  text: string;
}

interface CommandEntry {
  label: string;
  command: string;
}

interface CommandSection {
  title: string;
  description?: string;
  commands: CommandEntry[];
}

export default function ConsoleCommandPanel({
  onSend,
  sending,
  simulationActive,
  carTrafficStatus,
  boatTrafficStatus,
  onSimCarTraffic,
  onSimBoatTraffic,
}: Readonly<ConsoleCommandPanelProps>) {
  const [feedback, setFeedback] = useState<FeedbackState | null>(null);
  const [trafficPending, setTrafficPending] = useState(false);
  const [ultraSensors, setUltraSensors] = useState<Set<"left" | "right">>(new Set());

  const boatEventSection: CommandSection = {
    title: "Boat Event Simulation",
    description: "Trigger synthetic boat events on either side.",
    commands: [
      { label: "Boat Detected Left", command: "test boat left" },
      { label: "Boat Detected Right", command: "test boat right" },
      { label: "Boat Passed Left", command: "test boat pass left" },
      { label: "Boat Passed Right", command: "test boat pass right" },
    ],
  };

  const motorSection: CommandSection = {
    title: "Motor Control",
    description: "Manually move the bridge motor while in simulation.",
    commands: [
      { label: "Raise Bridge", command: "raise" },
      { label: "Lower Bridge", command: "lower" },
      { label: "Halt Motor", command: "halt" },
    ],
  };

  const safetyTestSection: CommandSection = {
    title: "Safety Test Fault",
    description: "Manually trigger or clear the safety test fault state.",
    commands: [
      { label: "Trigger Test Fault", command: "test fault" },
      { label: "Clear Test Fault", command: "test clear" },
      { label: "Test Fault Status", command: "test status" },
    ],
  };

  const renderCommandSection = (section: CommandSection) => (
    <div key={section.title} className="rounded-md border border-base-300 p-4 space-y-3 bg-gray-50">
      <div>
        <p className="font-semibold">{section.title}</p>
        {section.description ? <p className="text-xs text-gray-600 mt-1">{section.description}</p> : null}
      </div>
      <div className="flex flex-wrap gap-2">
        {section.commands.map((entry) => (
          <button
            key={entry.command}
            type="button"
            disabled={sending}
            onClick={() => {
              void handleClick(entry.command);
            }}
            className="rounded-md border border-base-300 px-3 py-1.5 text-sm text-gray-700 hover:bg-gray-100 disabled:cursor-not-allowed disabled:opacity-60"
          >
            {entry.label}
          </button>
        ))}
      </div>
    </div>
  );

  const handleUltrasonicToggle = async (mode: "both" | "left" | "right") => {
    const command = mode === "both" ? "us" : mode === "left" ? "usl" : "usr";
    const success = await handleClick(command);
    if (!success) return;
    setUltraSensors((prev) => {
      if (mode === "both") {
        return prev.size === 2 ? new Set() : new Set<"left" | "right">(["left", "right"]);
      }
      const next = new Set(prev);
      if (next.has(mode)) {
        next.delete(mode);
      } else {
        next.add(mode);
      }
      return next;
    });
  };

  const ultrasonicButtonClass = (active: boolean) =>
    `rounded-md px-3 py-1.5 text-sm font-medium border transition ${
      active ? "bg-blue-600 border-blue-600 text-white" : "border-base-300 text-gray-700 hover:bg-gray-100"
    } disabled:cursor-not-allowed disabled:opacity-60`;

  const handleClick = async (command: string) => {
    if (sending) return false;
    setFeedback(null);
    try {
      const result = await onSend(command);
      if (result.ok) {
        setFeedback({ type: "success", text: result.message ?? "Command sent successfully." });
        return true;
      } else {
        const message = result.error ?? result.message ?? "The device did not accept that command.";
        setFeedback({ type: "error", text: message });
        return false;
      }
    } catch (err) {
      const message = err instanceof Error ? err.message : "Failed to send command.";
      setFeedback({ type: "error", text: message });
      return false;
    }
  };

  useEffect(() => {
    if (!simulationActive) {
      setFeedback(null);
      setUltraSensors(new Set());
    }
  }, [simulationActive]);

  const bothActive = ultraSensors.size === 2;
  const leftActive = ultraSensors.has("left");
  const rightActive = ultraSensors.has("right");

  return (
    <div className="flex w-full justify-center px-4 pb-8 mt-6">
      <div className="w-full max-w-[1700px] rounded-md border border-base-400 bg-white p-4 shadow-[0_0_2px_rgba(0,0,0,0.25)] space-y-4">
        <div className="flex items-center gap-2">
          <Terminal size={20} />
          <span className="font-semibold">Console Commands</span>
        </div>
        <div className="rounded-md border border-base-300 bg-gray-50 p-4 space-y-3">
          <div>
            <p className="font-semibold">Simulation Mode</p>
            <p className="text-xs text-gray-600 mt-1">
              Enable simulation to access manual control commands. Disabling simulation restores standard dashboard
              control.
            </p>
          </div>
          <div className="flex flex-wrap gap-2">
            <button
              type="button"
              disabled={sending}
              onClick={() => {
                void handleClick("sim on");
              }}
              className={`rounded-md px-4 py-2 text-sm font-medium border ${
                simulationActive
                  ? "bg-green-600 border-green-600 text-white"
                  : "border-base-300 text-gray-700 hover:bg-gray-100 disabled:opacity-60"
              } disabled:cursor-not-allowed`}
            >
              Simulation Mode On
            </button>
            <button
              type="button"
              disabled={sending || !simulationActive}
              onClick={() => {
                void handleClick("sim off");
              }}
              className={`rounded-md px-4 py-2 text-sm font-medium border ${
                simulationActive
                  ? "border-base-300 text-gray-700 hover:bg-gray-100"
                  : "border-base-200 text-gray-400 bg-gray-100"
              } disabled:cursor-not-allowed disabled:opacity-60`}
            >
              Simulation Mode Off
            </button>
          </div>
        </div>
        {feedback ? (
          <p className={`text-sm ${feedback.type === "success" ? "text-green-600" : "text-red-600"}`}>
            {feedback.text}
          </p>
        ) : null}
        {simulationActive ? (
          <div className="space-y-4">
            <TrafficLightsCard
              carTrafficStatus={carTrafficStatus}
              boatTrafficStatus={boatTrafficStatus}
              onCarTrafficChange={async (value: CarTrafficState) => {
                setFeedback(null);
                setTrafficPending(true);
                try {
                  const result = await onSimCarTraffic(value);
                  setFeedback(
                    result.ok
                      ? { type: "success", text: result.message ?? `Car traffic set to ${value}.` }
                      : { type: "error", text: result.error ?? "Failed to update car traffic." }
                  );
                } finally {
                  setTrafficPending(false);
                }
              }}
              onBoatTrafficChange={async (side: "left" | "right", value: BoatTrafficState) => {
                setFeedback(null);
                setTrafficPending(true);
                try {
                  const result = await onSimBoatTraffic(side, value);
                  setFeedback(
                    result.ok
                      ? {
                          type: "success",
                          text: result.message ?? `Boat ${side} light set to ${value}.`,
                        }
                      : { type: "error", text: result.error ?? "Failed to update boat light." }
                  );
                } finally {
                  setTrafficPending(false);
                }
              }}
              disabled={sending || trafficPending}
            />
            <div className="grid gap-4 md:grid-cols-2 xl:grid-cols-3">
              {renderCommandSection(boatEventSection)}
              {renderCommandSection(safetyTestSection)}
              <div className="rounded-md border border-base-300 p-4 space-y-3 bg-gray-50">
                <div>
                  <p className="font-semibold">Ultrasonic Streaming</p>
                  <p className="text-xs text-gray-600 mt-1">
                    Toggle live ultrasonic distance streaming per sensor.
                  </p>
                </div>
                <div className="flex flex-wrap gap-2">
                  <button
                    type="button"
                    disabled={sending}
                    onClick={() => {
                      void handleUltrasonicToggle("both");
                    }}
                    className={ultrasonicButtonClass(bothActive)}
                  >
                    Both Sensors
                  </button>
                  <button
                    type="button"
                    disabled={sending}
                    onClick={() => {
                      void handleUltrasonicToggle("left");
                    }}
                    className={ultrasonicButtonClass(leftActive)}
                  >
                    Left Sensor
                  </button>
                  <button
                    type="button"
                    disabled={sending}
                    onClick={() => {
                      void handleUltrasonicToggle("right");
                    }}
                    className={ultrasonicButtonClass(rightActive)}
                  >
                    Right Sensor
                  </button>
                </div>
                <p className="text-xs text-gray-500">
                  {bothActive || leftActive || rightActive
                    ? `Streaming: ${leftActive ? "Left" : ""}${
                        leftActive && rightActive ? " & " : ""
                      }${rightActive ? "Right" : ""}`
                    : "Streaming disabled"}
                </p>
              </div>
              {renderCommandSection(motorSection)}
            </div>
          </div>
        ) : null}
      </div>
    </div>
  );
}
