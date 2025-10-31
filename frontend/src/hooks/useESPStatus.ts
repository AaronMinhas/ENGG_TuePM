import { useState } from "react";
import {
  BridgeStatus,
  CarTrafficStatus,
  BoatTrafficStatus,
  SystemStatus,
  SystemState,
  CarTrafficState,
  BoatTrafficState,
} from "../lib/schema";
import {
  getSystemState,
  setBridgeState,
  setCarTrafficState,
  setBoatTrafficState,
  resetSystem,
  setSimulationSensors,
} from "../lib/api";

export function useESPStatus(
  incrementSent: () => void,
  incrementReceived: (count?: number) => void,
  logActivity: (type: "sent" | "received", message: string) => void
) {
  const [bridgeStatus, setBridgeStatus] = useState<BridgeStatus | null>(null);
  const [carTrafficStatus, setCarTrafficStatus] = useState<CarTrafficStatus | null>(null);
  const [boatTrafficStatus, setBoatTrafficStatus] = useState<BoatTrafficStatus | null>(null);
  const [systemStatus, setSystemStatus] = useState<SystemStatus | null>(null);

  const handleFetchSystem = async () => {
    try {
      incrementSent();
      logActivity("sent", "System status request");

      const data = await getSystemState();
      incrementReceived();
      setSystemStatus({ ...data, receivedAt: Date.now() });
      logActivity("received", `System status: ${data.connection}`);
    } catch (e) {
      console.error("System status error:", e);
    }
  };

  const handleOpenBridge = async () => {
    incrementSent();
    logActivity("sent", "Open bridge");

    const data: any = await setBridgeState("Open");
    incrementReceived();
    const state = data.state ?? data.current?.state ?? data.requestedState ?? bridgeStatus?.state ?? "";
    const lastChangeMs = data.lastChangeMs ?? data.current?.lastChangeMs ?? bridgeStatus?.lastChangeMs ?? 0;
    setBridgeStatus({
      state,
      lastChangeMs,
      lockEngaged: data.lockEngaged ?? data.current?.lockEngaged ?? false,
      receivedAt: Date.now(),
    });
  };

  const handleCloseBridge = async () => {
    incrementSent();
    logActivity("sent", "Close bridge");

    const data: any = await setBridgeState("Closed");
    incrementReceived();
    const state = data.state ?? data.current?.state ?? data.requestedState ?? bridgeStatus?.state ?? "";
    const lastChangeMs = data.lastChangeMs ?? data.current?.lastChangeMs ?? bridgeStatus?.lastChangeMs ?? 0;
    setBridgeStatus({
      state,
      lastChangeMs,
      lockEngaged: data.lockEngaged ?? data.current?.lockEngaged ?? false,
      receivedAt: Date.now(),
    });
  };

  const handleCarTraffic = async (value: CarTrafficState) => {
    incrementSent();
    logActivity("sent", `Change car traffic lights to: ${value}`);

    const data = await setCarTrafficState(value);
    incrementReceived();
    setCarTrafficStatus(() => ({
      left: { ...data.left, receivedAt: Date.now() },
      right: { ...data.right, receivedAt: Date.now() },
    }));
  };

  const handleBoatTraffic = async (side: "left" | "right", value: BoatTrafficState) => {
    incrementSent();
    logActivity("sent", `Change ${side} boat traffic light: ${value}`);

    const data = await setBoatTrafficState(side, value);
    incrementReceived();
    setBoatTrafficStatus({
      left: { ...data.left, receivedAt: Date.now() },
      right: { ...data.right, receivedAt: Date.now() },
    });
  };

  const handleResetSystem = async () => {
    incrementSent();
    logActivity("sent", "Reset system to idle defaults");

    try {
      const data = await resetSystem();
      incrementReceived();
      const now = Date.now();

      if (data.bridge) {
        setBridgeStatus({
          state: data.bridge.state ?? bridgeStatus?.state ?? "IDLE",
          lastChangeMs: data.bridge.lastChangeMs ?? bridgeStatus?.lastChangeMs ?? 0,
          lockEngaged: data.bridge.lockEngaged ?? bridgeStatus?.lockEngaged ?? true,
          receivedAt: now,
        });
      }

      if (data.carTraffic) {
        setCarTrafficStatus({
          left: {
            value: data.carTraffic.left?.value ?? carTrafficStatus?.left.value ?? "Green",
            receivedAt: now,
          },
          right: {
            value: data.carTraffic.right?.value ?? carTrafficStatus?.right.value ?? "Green",
            receivedAt: now,
          },
        });
      }

      if (data.boatTraffic) {
        setBoatTrafficStatus({
          left: {
            value: data.boatTraffic.left?.value ?? boatTrafficStatus?.left.value ?? "Red",
            receivedAt: now,
          },
          right: {
            value: data.boatTraffic.right?.value ?? boatTrafficStatus?.right.value ?? "Red",
            receivedAt: now,
          },
        });
      }

      logActivity("received", "System reset applied. Bridge returning to idle.");
    } catch (err) {
      const message = err instanceof Error ? err.message : "Reset request failed.";
      logActivity("received", `Reset failed: ${message}`);
      throw err;
    }
  };

  const handleSimulationSensors = async (updates: {
    ultrasonicLeft?: boolean;
    ultrasonicRight?: boolean;
    beamBreak?: boolean;
  }) => {
    incrementSent();
    logActivity("sent", "Adjust simulation sensors");

    const data = await setSimulationSensors(updates);
    incrementReceived();

    setSystemStatus((prev) => {
      const base = prev ?? { connection: "Connected" as SystemState };
      return {
        ...base,
        simulationSensors: data.simulationSensors,
        receivedAt: Date.now(),
      };
    });
  };

  return {
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
  };
}
