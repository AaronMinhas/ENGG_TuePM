import React, { useState } from "react";
import TopNav from "./components/TopNav";
import DesktopDashboard from "./components/DesktopDashboard";
import MobileDashboard from "./components/MobileDashboard";
import ConsoleCommandPanel from "./components/ConsoleCommandPanel";
import { usePacketTracking } from "./hooks/usePacketTracking";
import { useActivityLog } from "./hooks/useActivityLog";
import { useESPStatus } from "./hooks/useESPStatus";
import { useESPWebSocket } from "./hooks/useESPWebSocket";
import { useESPStatusMock } from "./hooks/useESPStatusMock";
import { useESPWebSocketMock } from "./hooks/useESPWebSocketMock";
import { useTestModeFSM } from "./hooks/useTestModeFSM";
import { sendConsoleCommand } from "./lib/api";
import { CarTrafficState, BoatTrafficState, SimulationSensorsStatus } from "./lib/schema";
import { isTestMode } from "./utils/testMode";

function App() {
  const testMode = isTestMode();
  
  const { packetsSent, packetsReceived, lastSentAt, lastReceivedAt, incrementSent, incrementReceived } =
    usePacketTracking();

  const { activityLog, logActivity } = useActivityLog();

  // Use mock hooks in test mode, real hooks otherwise
  const espStatusHook = testMode
    ? useESPStatusMock(incrementSent, incrementReceived, logActivity)
    : useESPStatus(incrementSent, incrementReceived, logActivity);

  const {
    bridgeStatus,
    setBridgeStatus,
    carTrafficStatus,
    setCarTrafficStatus,
    boatTrafficStatus,
    setBoatTrafficStatus,
    systemStatus,
    setSystemStatus,
    sensorStatus,
    setSensorStatus,
    handleFetchSystem,
    handleOpenBridge,
    handleCloseBridge,
    handleCarTraffic,
    handleBoatTraffic,
    handleResetSystem,
    handleSimulationSensors,
  } = espStatusHook;

  const { reconnect } = testMode
    ? useESPWebSocketMock({
        setBridgeStatus,
        setCarTrafficStatus,
        setBoatTrafficStatus,
        setSystemStatus,
        setSensorStatus,
        incrementReceived,
        logActivity,
        carTrafficStatus,
        boatTrafficStatus,
      })
    : useESPWebSocket({
        setBridgeStatus,
        setCarTrafficStatus,
        setBoatTrafficStatus,
        setSystemStatus,
        setSensorStatus,
        incrementReceived,
        logActivity,
        carTrafficStatus,
        boatTrafficStatus,
      });

  // Test mode FSM for realistic state transitions 
  const testModeFSM = useTestModeFSM(
    setBridgeStatus,
    setCarTrafficStatus,
    setBoatTrafficStatus,
    setSensorStatus,
    logActivity
  );

  const [resetting, setResetting] = useState(false);

  const handleReset = async () => {
    setResetting(true);
    try {
      if (testMode) {
        // In test mode, use FSM reset
        testModeFSM.resetFSM();
        await handleResetSystem();
      } else {
        await handleResetSystem();
      }
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
    sensorStatus,
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
      // In test mode, simulate console command responses
      if (testMode) {
        await new Promise(resolve => setTimeout(resolve, 300));
        incrementReceived();
        
        const lowerCommand = trimmed.toLowerCase();
        
        // Expanded list of recognised commands for test mode
        const recognisedCommands = [
          "help", "status", "reset", 
          "sim on", "sim off", "simulation on", "simulation off",
          "raise", "lower", "halt",
          "test boat left", "test boat right", "test boat pass",
          "test fault", "test clear", "test status", "test limit",
          "us", "usl", "usr", 
          "log level debug", "log level info", "log level warn", "log level error", "log level none"
        ];
        
        const handled = recognisedCommands.some(cmd => lowerCommand.includes(cmd));
        
        // Simulate some commands actually doing something visible
        if (lowerCommand.includes("sim on")) {
          setSystemStatus(prev => prev ? { ...prev, simulation: true, receivedAt: Date.now() } : {
            connection: "Connected",
            simulation: true,
            receivedAt: Date.now(),
          });
          logActivity("received", "Simulation mode enabled");
        } else if (lowerCommand.includes("sim off")) {
          setSystemStatus(prev => prev ? { ...prev, simulation: false, receivedAt: Date.now() } : {
            connection: "Connected",
            simulation: false,
            receivedAt: Date.now(),
          });
          logActivity("received", "Simulation mode disabled");
        } else if (lowerCommand.includes("raise")) {
          setBridgeStatus(prev => prev ? { ...prev, state: "OPENING", lastChangeMs: Date.now(), lockEngaged: false, receivedAt: Date.now() } : {
            state: "OPENING",
            lastChangeMs: Date.now(),
            lockEngaged: false,
            receivedAt: Date.now(),
          });
          setTimeout(() => {
            setBridgeStatus(prev => prev ? { ...prev, state: "OPEN", receivedAt: Date.now() } : {
              state: "OPEN",
              lastChangeMs: Date.now(),
              lockEngaged: false,
              receivedAt: Date.now(),
            });
          }, 2000);
          logActivity("received", "Bridge raising command acknowledged");
        } else if (lowerCommand.includes("lower")) {
          setBridgeStatus(prev => prev ? { ...prev, state: "CLOSING", lastChangeMs: Date.now(), lockEngaged: false, receivedAt: Date.now() } : {
            state: "CLOSING",
            lastChangeMs: Date.now(),
            lockEngaged: false,
            receivedAt: Date.now(),
          });
          setTimeout(() => {
            setBridgeStatus(prev => prev ? { ...prev, state: "IDLE", lockEngaged: true, receivedAt: Date.now() } : {
              state: "IDLE",
              lastChangeMs: Date.now(),
              lockEngaged: true,
              receivedAt: Date.now(),
            });
          }, 2000);
          logActivity("received", "Bridge lowering command acknowledged");
        } else if (lowerCommand.includes("test boat left")) {
          // Use FSM to simulate full bridge cycle
          if (testMode) {
            testModeFSM.startBoatCycle("left");
          } else {
            setBoatTrafficStatus(prev => ({
              left: { value: "Green", receivedAt: Date.now() },
              right: prev?.right || { value: "Red", receivedAt: Date.now() },
            }));
            logActivity("received", "Boat detected from left - traffic lights updated");
          }
        } else if (lowerCommand.includes("test boat right")) {
          // Use FSM to simulate full bridge cycle
          if (testMode) {
            testModeFSM.startBoatCycle("right");
          } else {
            setBoatTrafficStatus(prev => ({
              left: prev?.left || { value: "Red", receivedAt: Date.now() },
              right: { value: "Green", receivedAt: Date.now() },
            }));
            logActivity("received", "Boat detected from right - traffic lights updated");
          }
        } else if (lowerCommand.includes("test boat pass")) {
          // Use FSM to handle boat passed event
          if (testMode) {
            testModeFSM.handleBoatPassed();
          } else {
            setBoatTrafficStatus({
              left: { value: "Red", receivedAt: Date.now() },
              right: { value: "Red", receivedAt: Date.now() },
            });
            logActivity("received", "Boat passed - traffic lights reset");
          }
        } else if (lowerCommand.includes("log level")) {
          const level = lowerCommand.includes("debug") ? "DEBUG" :
                       lowerCommand.includes("info") ? "INFO" :
                       lowerCommand.includes("warn") ? "WARN" :
                       lowerCommand.includes("error") ? "ERROR" :
                       lowerCommand.includes("none") ? "NONE" : null;
          if (level) {
            setSystemStatus(prev => prev ? { ...prev, logLevel: level, receivedAt: Date.now() } : {
              connection: "Connected",
              logLevel: level,
              receivedAt: Date.now(),
            });
            logActivity("received", `Log level set to ${level}`);
          }
        } else if (lowerCommand.includes("us")) {
          const streaming = lowerCommand.includes("usl") ? { left: true, right: false } :
                           lowerCommand.includes("usr") ? { left: false, right: true } :
                           { left: true, right: true };
          setSystemStatus(prev => prev ? {
            ...prev,
            ultrasonicStreaming: streaming,
            receivedAt: Date.now()
          } : {
            connection: "Connected",
            ultrasonicStreaming: streaming,
            receivedAt: Date.now(),
          });
          logActivity("received", `Ultrasonic streaming ${lowerCommand.includes("usl") ? "left" : lowerCommand.includes("usr") ? "right" : "both"} enabled`);
        } else {
          // Generic acknowledgment for other recognised commands
          logActivity("received", `Console acknowledged '${trimmed}'`);
        }
        
        const message = handled
          ? "Command accepted by the device."
          : "The device did not recognise that command.";
        
        if (!handled) {
          logActivity("received", `Console ignored '${trimmed}'`);
        }
        
        return handled
          ? { ok: true, message }
          : { ok: false, message };
      }
      
      // Real mode - use actual API
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
      {testMode && (
        <div className="bg-yellow-500 text-black text-center py-2 px-4 font-semibold">
          TEST MODE
        </div>
      )}
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
