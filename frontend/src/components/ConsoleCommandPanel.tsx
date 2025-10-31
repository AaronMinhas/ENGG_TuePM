import React, { useEffect, useState } from "react";
import { 
  Terminal, 
  Power, 
  Waves, 
  Radio, 
  Sailboat, 
  AlertTriangle, 
  Settings,
  ArrowUp,
  ArrowDown,
  Square,
  Gauge,
  FileText,
  Target
} from "lucide-react";
import TrafficLightsCard from "./TrafficLightsCard";
import {
  CarTrafficStatus,
  BoatTrafficStatus,
  CarTrafficState,
  BoatTrafficState,
  SimulationSensorsStatus,
  UltrasonicStreamingStatus,
  LogLevel,
} from "../lib/schema";

interface ConsoleCommandPanelProps {
  onSend: (command: string) => Promise<{ ok: boolean; message?: string; error?: string }>;
  sending: boolean;
  simulationActive: boolean;
  simulationSensors: SimulationSensorsStatus;
  ultrasonicStreaming?: UltrasonicStreamingStatus;
  carTrafficStatus: CarTrafficStatus | null;
  boatTrafficStatus: BoatTrafficStatus | null;
  logLevel?: LogLevel;
  onSimCarTraffic: (value: CarTrafficState) => Promise<{ ok: boolean; message?: string; error?: string }>;
  onSimBoatTraffic: (
    side: "left" | "right",
    value: BoatTrafficState
  ) => Promise<{ ok: boolean; message?: string; error?: string }>;
  onSimSensors: (updates: {
    ultrasonicLeft?: boolean;
    ultrasonicRight?: boolean;
    beamBreak?: boolean;
  }) => Promise<{ ok: boolean; message?: string; error?: string }>;
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
  simulationSensors,
  ultrasonicStreaming,
  carTrafficStatus,
  boatTrafficStatus,
  logLevel,
  onSimCarTraffic,
  onSimBoatTraffic,
  onSimSensors,
}: Readonly<ConsoleCommandPanelProps>) {
  const [feedback, setFeedback] = useState<FeedbackState | null>(null);
  const [trafficPending, setTrafficPending] = useState(false);
  const [sensorsPending, setSensorsPending] = useState(false);

  const boatEventSection: CommandSection = {
    title: "Boat Event Simulation",
    description: "Trigger synthetic boat events for testing.",
    commands: [
      { label: "Boat Detected Left", command: "test boat left" },
      { label: "Boat Detected Right", command: "test boat right" },
      { label: "Boat Passed", command: "test boat pass" },
    ],
  };

  const motorSection: CommandSection = {
    title: "Motor Control",
    description: "Manually move the bridge motor while in simulation.",
    commands: [
      { label: "Raise Bridge", command: "raise" },
      { label: "Lower Bridge", command: "lower" },
      { label: "Halt Motor", command: "halt" },
      { label: "Test Limit Switch", command: "test limit" },
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

  const getSectionIcon = (title: string) => {
    switch (title) {
      case "Boat Event Simulation":
        return <Sailboat size={14} className="text-blue-600" />;
      case "Motor Control":
        return <Settings size={14} className="text-purple-600" />;
      case "Safety Test Fault":
        return <AlertTriangle size={14} className="text-orange-600" />;
      default:
        return null;
    }
  };

  const getButtonIcon = (label: string) => {
    if (label.includes("Raise")) return <ArrowUp size={12} className="text-gray-700" />;
    if (label.includes("Lower")) return <ArrowDown size={12} className="text-gray-700" />;
    if (label.includes("Halt")) return <Square size={12} className="text-gray-700" />;
    if (label.includes("Test Limit")) return <Target size={12} className="text-gray-700" />;
    return null;
  };

  const renderCommandSection = (section: CommandSection) => (
    <div key={section.title} className="rounded-md border border-base-300 p-3 space-y-2 bg-gray-50">
      <div className="flex items-center gap-2">
        {getSectionIcon(section.title)}
        <p className="text-sm font-semibold text-gray-900">{section.title}</p>
      </div>
      <div className="flex flex-wrap gap-1.5">
        {section.commands.map((entry) => (
          <button
            key={entry.command}
            type="button"
            disabled={sending}
            onClick={() => {
              void handleClick(entry.command);
            }}
            className="rounded border border-base-300 px-2.5 py-1.5 text-xs font-medium text-gray-700 hover:border-brand-500 hover:bg-brand-50 hover:text-brand-600 disabled:cursor-not-allowed disabled:opacity-50 transition-all flex items-center gap-1"
          >
            {getButtonIcon(entry.label)}
            {entry.label}
          </button>
        ))}
      </div>
    </div>
  );

  const handleUltrasonicToggle = async (mode: "both" | "left" | "right") => {
    const command = mode === "both" ? "us" : mode === "left" ? "usl" : "usr";
    await handleClick(command);
  };

  const handleSimulationSensorToggle = async (
    updates: { ultrasonicLeft?: boolean; ultrasonicRight?: boolean; beamBreak?: boolean },
    successText: string
  ) => {
    if (sensorsPending) return;
    setFeedback(null);
    setSensorsPending(true);
    try {
      const result = await onSimSensors(updates);
      if (result.ok) {
        setFeedback({ type: "success", text: successText });
      } else {
        setFeedback({ type: "error", text: result.error ?? "Failed to update simulation sensors." });
      }
    } catch (err) {
      const message = err instanceof Error ? err.message : "Failed to update simulation sensors.";
      setFeedback({ type: "error", text: message });
    } finally {
      setSensorsPending(false);
    }
  };

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
      setSensorsPending(false);
    }
  }, [simulationActive]);

  const leftActive = ultrasonicStreaming?.left ?? false;
  const rightActive = ultrasonicStreaming?.right ?? false;
  const bothActive = leftActive && rightActive;

  return (
    <div className="flex w-full justify-center px-4 pb-6 mt-4">
      <div className="w-full max-w-[1700px] rounded-lg border border-base-300 bg-white p-4 shadow-sm space-y-3">
        <div className="flex items-center gap-2">
          <Terminal size={16} className="text-gray-700" />
          <span className="font-semibold text-gray-900">Console Commands</span>
        </div>
        
        {/* Top Row: Simulation Mode + Sensor Sources + Log Level */}
        <div className="flex flex-wrap gap-3">
          {/* Simulation Mode Section */}
          <div className="rounded-md border border-base-300 bg-gray-50 p-3 inline-flex items-center gap-3">
            <div className="flex items-center gap-2">
              <Power size={14} className="text-green-600" />
              <p className="text-sm font-semibold text-gray-900">Simulation Mode</p>
            </div>
            <div className="flex gap-2">
              <button
                type="button"
                disabled={sending}
                onClick={() => {
                  void handleClick("sim on");
                }}
                className={`rounded px-3 py-1.5 text-xs font-medium border transition-all flex items-center gap-1.5 ${
                  simulationActive
                    ? "bg-green-600 border-green-600 text-white"
                    : "border-base-300 text-gray-700 hover:border-green-500 hover:bg-green-50 disabled:opacity-60"
                } disabled:cursor-not-allowed`}
              >
                <Power size={12} className={simulationActive ? "text-white" : "text-gray-700"} />
                On
              </button>
              <button
                type="button"
                disabled={sending || !simulationActive}
                onClick={() => {
                  void handleClick("sim off");
                }}
                className={`rounded px-3 py-1.5 text-xs font-medium border transition-all flex items-center gap-1.5 ${
                  simulationActive
                    ? "border-base-300 text-gray-700 hover:border-red-500 hover:bg-red-50"
                    : "border-base-200 text-gray-400 bg-gray-100"
                } disabled:cursor-not-allowed disabled:opacity-60`}
              >
                <Power size={12} className={simulationActive ? "text-gray-700" : "text-gray-400"} />
                Off
              </button>
            </div>
          </div>
          
          {/* Sensor Sources Section */}
          {simulationActive && (
            <div className="rounded-md border border-base-300 bg-gray-50 p-3">
              <div className="flex items-center gap-2 mb-2">
                <Radio size={14} className="text-green-600" />
                <div>
                  <p className="text-sm font-semibold text-gray-900">Sensor Sources</p>
                  <p className="text-[10px] text-gray-500">Enable/disable sensor inputs</p>
                </div>
              </div>
              <div className="flex flex-wrap gap-2">
                <button
                  type="button"
                  disabled={sensorsPending}
                  onClick={() =>
                    void handleSimulationSensorToggle(
                      { ultrasonicLeft: !simulationSensors.ultrasonicLeft },
                      `Left ultrasonic ${simulationSensors.ultrasonicLeft ? "disabled" : "enabled"}.`
                    )
                  }
                  className={`rounded px-3 py-1.5 text-xs font-medium border transition-all flex items-center gap-1.5 ${
                    simulationSensors.ultrasonicLeft
                      ? "bg-green-600 border-green-600 text-white"
                      : "border-base-300 text-gray-700 hover:border-green-500 hover:bg-green-50"
                  } disabled:cursor-not-allowed disabled:opacity-60`}
                >
                  <Waves size={12} className={simulationSensors.ultrasonicLeft ? "text-white" : "text-gray-700"} />
                  Left Ultrasonic
                </button>
                <button
                  type="button"
                  disabled={sensorsPending}
                  onClick={() =>
                    void handleSimulationSensorToggle(
                      { ultrasonicRight: !simulationSensors.ultrasonicRight },
                      `Right ultrasonic ${simulationSensors.ultrasonicRight ? "disabled" : "enabled"}.`
                    )
                  }
                  className={`rounded px-3 py-1.5 text-xs font-medium border transition-all flex items-center gap-1.5 ${
                    simulationSensors.ultrasonicRight
                      ? "bg-green-600 border-green-600 text-white"
                      : "border-base-300 text-gray-700 hover:border-green-500 hover:bg-green-50"
                  } disabled:cursor-not-allowed disabled:opacity-60`}
                >
                  <Waves size={12} className={simulationSensors.ultrasonicRight ? "text-white" : "text-gray-700"} />
                  Right Ultrasonic
                </button>
                <button
                  type="button"
                  disabled={sensorsPending}
                  onClick={() =>
                    void handleSimulationSensorToggle(
                      { beamBreak: !simulationSensors.beamBreak },
                      `Beam break ${simulationSensors.beamBreak ? "disabled" : "enabled"}.`
                    )
                  }
                  className={`rounded px-3 py-1.5 text-xs font-medium border transition-all flex items-center gap-1.5 ${
                    simulationSensors.beamBreak
                      ? "bg-green-600 border-green-600 text-white"
                      : "border-base-300 text-gray-700 hover:border-green-500 hover:bg-green-50"
                  } disabled:cursor-not-allowed disabled:opacity-60`}
                >
                  <Radio size={12} className={simulationSensors.beamBreak ? "text-white" : "text-gray-700"} />
                  Beam Break
                </button>
              </div>
            </div>
          )}
          
          {/* Log Level Control */}
          <div className="rounded-md border border-base-300 bg-gray-50 p-3 inline-flex items-center gap-3 flex-wrap">
            <div className="flex items-center gap-2">
              <FileText size={14} className="text-indigo-600" />
              <p className="text-sm font-semibold text-gray-900">Log Level</p>
            </div>
            <div className="flex gap-1.5">
              <button
                type="button"
                disabled={sending}
                onClick={() => {
                  void handleClick("log level debug");
                }}
                className={`rounded border px-2.5 py-1.5 text-xs font-medium transition-all disabled:cursor-not-allowed disabled:opacity-50 ${
                  logLevel === "DEBUG"
                    ? "bg-indigo-600 border-indigo-600 text-white"
                    : "border-base-300 text-gray-700 hover:border-indigo-500 hover:bg-indigo-50 hover:text-indigo-700"
                }`}
              >
                Debug
              </button>
              <button
                type="button"
                disabled={sending}
                onClick={() => {
                  void handleClick("log level info");
                }}
                className={`rounded border px-2.5 py-1.5 text-xs font-medium transition-all disabled:cursor-not-allowed disabled:opacity-50 ${
                  logLevel === "INFO"
                    ? "bg-indigo-600 border-indigo-600 text-white"
                    : "border-base-300 text-gray-700 hover:border-indigo-500 hover:bg-indigo-50 hover:text-indigo-700"
                }`}
              >
                Info
              </button>
              <button
                type="button"
                disabled={sending}
                onClick={() => {
                  void handleClick("log level warn");
                }}
                className={`rounded border px-2.5 py-1.5 text-xs font-medium transition-all disabled:cursor-not-allowed disabled:opacity-50 ${
                  logLevel === "WARN"
                    ? "bg-indigo-600 border-indigo-600 text-white"
                    : "border-base-300 text-gray-700 hover:border-indigo-500 hover:bg-indigo-50 hover:text-indigo-700"
                }`}
              >
                Warn
              </button>
              <button
                type="button"
                disabled={sending}
                onClick={() => {
                  void handleClick("log level error");
                }}
                className={`rounded border px-2.5 py-1.5 text-xs font-medium transition-all disabled:cursor-not-allowed disabled:opacity-50 ${
                  logLevel === "ERROR"
                    ? "bg-indigo-600 border-indigo-600 text-white"
                    : "border-base-300 text-gray-700 hover:border-indigo-500 hover:bg-indigo-50 hover:text-indigo-700"
                }`}
              >
                Error
              </button>
              <button
                type="button"
                disabled={sending}
                onClick={() => {
                  void handleClick("log level none");
                }}
                className={`rounded border px-2.5 py-1.5 text-xs font-medium transition-all disabled:cursor-not-allowed disabled:opacity-50 ${
                  logLevel === "NONE"
                    ? "bg-indigo-600 border-indigo-600 text-white"
                    : "border-base-300 text-gray-700 hover:border-indigo-500 hover:bg-indigo-50 hover:text-indigo-700"
                }`}
              >
                None
              </button>
            </div>
          </div>
        </div>
        
        {simulationActive ? (
          <div className="space-y-3">
            <div className="grid gap-3 sm:grid-cols-2 lg:grid-cols-4">
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
              
              {renderCommandSection(boatEventSection)}
              {renderCommandSection(motorSection)}
              {renderCommandSection(safetyTestSection)}
            </div>
            
            {/* Ultrasonic Streaming */}
            <div className="rounded-md border border-base-300 p-3 bg-gray-50">
              <div className="flex items-center justify-between gap-2 mb-2">
                <div className="flex items-center gap-2">
                  <Gauge size={14} className="text-cyan-600" />
                  <p className="text-sm font-semibold text-gray-900">Ultrasonic Streaming</p>
                </div>
                <span className={`text-[10px] px-2 py-0.5 rounded font-medium ${
                  bothActive || leftActive || rightActive
                    ? "bg-blue-100 text-blue-700"
                    : "bg-gray-100 text-gray-600"
                }`}>
                  {bothActive || leftActive || rightActive
                    ? `${leftActive ? "L" : ""}${leftActive && rightActive ? "+" : ""}${rightActive ? "R" : ""}`
                    : "Off"}
                </span>
              </div>
              <div className="flex flex-wrap gap-1.5">
                <button
                  type="button"
                  disabled={sending}
                  onClick={() => {
                    void handleUltrasonicToggle("both");
                  }}
                  className={`rounded border px-2.5 py-1.5 text-xs font-medium transition-all flex items-center gap-1 ${
                    bothActive 
                      ? "bg-blue-600 border-blue-600 text-white" 
                      : "border-base-300 text-gray-700 hover:border-blue-500 hover:bg-blue-50"
                  } disabled:cursor-not-allowed disabled:opacity-60`}
                >
                  <Waves size={12} className={bothActive ? "text-white" : "text-gray-700"} />
                  Both Sensors
                </button>
                <button
                  type="button"
                  disabled={sending}
                  onClick={() => {
                    void handleUltrasonicToggle("left");
                  }}
                  className={`rounded border px-2.5 py-1.5 text-xs font-medium transition-all flex items-center gap-1 ${
                    leftActive 
                      ? "bg-blue-600 border-blue-600 text-white" 
                      : "border-base-300 text-gray-700 hover:border-blue-500 hover:bg-blue-50"
                  } disabled:cursor-not-allowed disabled:opacity-60`}
                >
                  <Waves size={12} className={leftActive ? "text-white" : "text-gray-700"} />
                  Left Sensor
                </button>
                <button
                  type="button"
                  disabled={sending}
                  onClick={() => {
                    void handleUltrasonicToggle("right");
                  }}
                  className={`rounded border px-2.5 py-1.5 text-xs font-medium transition-all flex items-center gap-1 ${
                    rightActive 
                      ? "bg-blue-600 border-blue-600 text-white" 
                      : "border-base-300 text-gray-700 hover:border-blue-500 hover:bg-blue-50"
                  } disabled:cursor-not-allowed disabled:opacity-60`}
                >
                  <Waves size={12} className={rightActive ? "text-white" : "text-gray-700"} />
                  Right Sensor
                </button>
              </div>
            </div>
          </div>
        ) : null}
        
        {/* Feedback messages at bottom */}
        {feedback ? (
          <div className={`rounded border-l-2 px-3 py-2 text-xs font-medium ${
            feedback.type === "success" 
              ? "bg-green-50 border-green-500 text-green-700" 
              : "bg-red-50 border-red-500 text-red-700"
          }`}>
            {feedback.text}
          </div>
        ) : null}
      </div>
    </div>
  );
}
