import React, { useState } from "react";
import TopNav from "./components/TopNav";
import DesktopDashboard from "./components/DesktopDashboard";
import MobileDashboard from "./components/MobileDashboard";
import ConsoleCommandPanel from "./components/ConsoleCommandPanel";
import { usePacketTracking } from "./hooks/usePacketTracking";
import { useActivityLog } from "./hooks/useActivityLog";
import { useESPStatus } from "./hooks/useESPStatus";
import { useESPWebSocket } from "./hooks/useESPWebSocket";
import { sendConsoleCommand } from "./lib/api";
import { CarTrafficState, BoatTrafficState } from "./lib/schema";

function App() {
  const { packetsSent, packetsReceived, lastSentAt, lastReceivedAt, incrementSent, incrementReceived } =
    usePacketTracking();

  const { activityLog, logActivity } = useActivityLog();

  const {
    bridgeStatus,
    setBridgeStatus,
    carTrafficStatus,
    setCarTrafficStatus,
    boatTrafficStatus,
    setBoatTrafficStatus,
    systemStatus,
    setSystemStatus,
    handleFetchSystem,
    handleOpenBridge,
    handleCloseBridge,
    handleCarTraffic,
    handleBoatTraffic,
    handleResetSystem,
  } = useESPStatus(incrementSent, incrementReceived, logActivity);

  useESPWebSocket({
    setBridgeStatus,
    setCarTrafficStatus,
    setBoatTrafficStatus,
    setSystemStatus,
    incrementReceived,
    logActivity,
    carTrafficStatus,
    boatTrafficStatus,
  });

  const [simulationMode, setSimulationMode] = useState(false);
  const [resetting, setResetting] = useState(false);

  const handleReset = async () => {
    setResetting(true);
    try {
      await handleResetSystem();
    } catch (err) {
      console.error("Reset request failed:", err);
    } finally {
      setResetting(false);
    }
  };

  const dashboardProps = {
    bridgeStatus,
    carTrafficStatus,
    boatTrafficStatus,
    systemStatus,
    packetsSent,
    packetsReceived,
    lastSentAt,
    lastReceivedAt,
    activityLog,
    handleOpenBridge,
    handleCloseBridge,
    handleCarTraffic,
    handleBoatTraffic,
    handleFetchSystem,
    controlsDisabled: simulationMode,
  };

  const [sendingConsole, setSendingConsole] = useState(false);

  const handleConsoleCommand = async (command: string) => {
    const trimmed = command.trim();
    if (!trimmed) {
      return { ok: false, error: "Enter a command before sending." };
    }

    incrementSent();
    logActivity("sent", `Console: ${trimmed}`);
    setSendingConsole(true);

    try {
      const response = await sendConsoleCommand(trimmed);
      incrementReceived();
      const handled = response?.handled ?? false;
      const message = handled
        ? "Command accepted by the device."
        : "The device did not recognise that command.";
      logActivity(
        "received",
        handled ? `Console acknowledged '${trimmed}'` : `Console ignored '${trimmed}'`
      );
      if (handled) {
        const normalized = trimmed.toLowerCase();
        if (normalized === "sim on" || normalized === "simulation on") {
          setSimulationMode(true);
        } else if (normalized === "sim off" || normalized === "simulation off") {
          setSimulationMode(false);
        }
      }
      return handled
        ? { ok: true, message }
        : { ok: false, message };
    } catch (err) {
      const message = err instanceof Error ? err.message : "Failed to send command.";
      logActivity("received", `Console error: ${message}`);
      return { ok: false, error: message };
    } finally {
      setSendingConsole(false);
    }
  };

  const triggerSimCarTraffic = async (value: CarTrafficState) => {
    try {
      await handleCarTraffic(value);
      return { ok: true, message: `Car traffic set to ${value}.` };
    } catch (err) {
      const message = err instanceof Error ? err.message : "Failed to set car traffic.";
      return { ok: false, error: message };
    }
  };

  const triggerSimBoatTraffic = async (side: "left" | "right", value: BoatTrafficState) => {
    try {
      await handleBoatTraffic(side, value);
      return { ok: true, message: `Boat ${side} light set to ${value}.` };
    } catch (err) {
      const message = err instanceof Error ? err.message : "Failed to set boat light.";
      return { ok: false, error: message };
    }
  };

  return (
    <div className="bg-gray-100 min-h-screen w-full">
      <TopNav onReset={handleReset} resetting={resetting} />
      <div className="flex justify-center mt-4">
        <DesktopDashboard {...dashboardProps} />
        <MobileDashboard {...dashboardProps} />
      </div>
      <ConsoleCommandPanel
        onSend={handleConsoleCommand}
        sending={sendingConsole}
        simulationActive={simulationMode}
        carTrafficStatus={carTrafficStatus}
        boatTrafficStatus={boatTrafficStatus}
        onSimCarTraffic={triggerSimCarTraffic}
        onSimBoatTraffic={triggerSimBoatTraffic}
      />
    </div>
  );
}

export default App;
