import { useState } from "react";
import {
  BridgeStatus,
  CarTrafficStatus,
  BoatTrafficStatus,
  SystemStatus,
  CarTrafficState,
  BoatTrafficState,
} from "../lib/schema";
import {
  getSystemState,
  setBridgeState,
  setCarTrafficState,
  setBoatTrafficState,
} from "../lib/api";

/**
 * Hook for managing ESP bridge/traffic/system status and control commands
 */
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
    logActivity("received", `Car traffic lights set to: ${value}`);
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
    logActivity("received", `Boat traffic ${side} state: ${value}`);
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
  };
}

