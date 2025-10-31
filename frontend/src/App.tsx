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
import { CarTrafficState, BoatTrafficState, SimulationSensorsStatus } from "./lib/schema";

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
    handleSimulationSensors,
  } = useESPStatus(incrementSent, incrementReceived, logActivity);

  const { reconnect } = useESPWebSocket({
    setBridgeStatus,
    setCarTrafficStatus,
    setBoatTrafficStatus,
    setSystemStatus,
    incrementReceived,
    logActivity,
    carTrafficStatus,
    boatTrafficStatus,
  });

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

  const simulationActive = Boolean(systemStatus?.simulation);
  const simulationSensors: SimulationSensorsStatus =
    systemStatus?.simulationSensors ?? {
      ultrasonicLeft: false,
      ultrasonicRight: false,
      beamBreak: false,
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
    controlsDisabled: simulationActive,
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
      // Do not optimistically toggle simulation locally; we reflect actual device state via WS/polling
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

  const handleSimulationSensorUpdate = async (updates: {
    ultrasonicLeft?: boolean;
    ultrasonicRight?: boolean;
    beamBreak?: boolean;
  }) => {
    try {
      await handleSimulationSensors(updates);
      return { ok: true };
    } catch (err) {
      const message = err instanceof Error ? err.message : "Failed to update simulation sensors.";
      return { ok: false, error: message };
    }
  };

  return (
    <div className="bg-gray-100 min-h-screen w-full">
      <TopNav onReset={handleReset} resetting={resetting} onReconnect={reconnect} />
      <div className="flex justify-center mt-4">
        <DesktopDashboard {...dashboardProps} />
        <MobileDashboard {...dashboardProps} />
      </div>
      <ConsoleCommandPanel
        onSend={handleConsoleCommand}
        sending={sendingConsole}
        simulationActive={simulationActive}
        simulationSensors={simulationSensors}
        ultrasonicStreaming={systemStatus?.ultrasonicStreaming}
        carTrafficStatus={carTrafficStatus}
        boatTrafficStatus={boatTrafficStatus}
        logLevel={systemStatus?.logLevel}
        onSimCarTraffic={triggerSimCarTraffic}
        onSimBoatTraffic={triggerSimBoatTraffic}
        onSimSensors={handleSimulationSensorUpdate}
      />
    </div>
  );
}

export default App;
