import { useState, useCallback } from "react";
import {
  BridgeStatus,
  CarTrafficStatus,
  BoatTrafficStatus,
  SystemStatus,
  SensorStatus,
  CarTrafficState,
  BoatTrafficState,
} from "../lib/schema";

/**
 * Provides fake data and simulates API responses
 */
export function useESPStatusMock(
  incrementSent: () => void,
  incrementReceived: (count?: number) => void,
  logActivity: (type: "sent" | "received", message: string) => void
) {
  const [bridgeStatus, setBridgeStatus] = useState<BridgeStatus | null>({
    state: "IDLE",
    lastChangeMs: Date.now() - 5000,
    lockEngaged: true,
    receivedAt: Date.now(),
  });

  const [carTrafficStatus, setCarTrafficStatus] = useState<CarTrafficStatus | null>({
    left: { value: "Green", receivedAt: Date.now() },
    right: { value: "Green", receivedAt: Date.now() },
  });

  const [boatTrafficStatus, setBoatTrafficStatus] = useState<BoatTrafficStatus | null>({
    left: { value: "Red", receivedAt: Date.now() },
    right: { value: "Red", receivedAt: Date.now() },
  });

  const [systemStatus, setSystemStatus] = useState<SystemStatus | null>({
    connection: "Connected",
    rssi: -65,
    uptimeMs: 3600000, // 1 hour
    simulation: false,
    simulationSensors: {
      ultrasonicLeft: false,
      ultrasonicRight: false,
      beamBreak: false,
    },
    ultrasonicStreaming: {
      left: false,
      right: false,
    },
    logLevel: "INFO",
    receivedAt: Date.now(),
  });

  const [sensorStatus, setSensorStatus] = useState<SensorStatus | null>({
    leftUltrasonic: {
      distanceCm: 60,
      zone: "near",
    },
    rightUltrasonic: {
      distanceCm: 60,
      zone: "near",
    },
    beamBreak: false,
    direction: "none",
    receivedAt: Date.now(),
  });

  const handleFetchSystem = useCallback(async () => {
    incrementSent();
    logActivity("sent", "System status request");
    
    // Simulate network delay
    await new Promise(resolve => setTimeout(resolve, 200));
    
    incrementReceived();
    setSystemStatus(prev => ({
      ...prev!,
      receivedAt: Date.now(),
    }));
    logActivity("received", "System status: Connected");
  }, [incrementSent, incrementReceived, logActivity]);

  const handleOpenBridge = useCallback(async () => {
    incrementSent();
    logActivity("sent", "Open bridge");
    
    await new Promise(resolve => setTimeout(resolve, 300));
    
    incrementReceived();
    setBridgeStatus({
      state: "OPENING",
      lastChangeMs: Date.now(),
      lockEngaged: false,
      receivedAt: Date.now(),
    });
    
    // Simulate bridge opening sequence
    setTimeout(() => {
      setBridgeStatus(prev => prev ? {
        ...prev,
        state: "OPEN",
        receivedAt: Date.now(),
      } : null);
    }, 2000);
  }, [incrementSent, incrementReceived, logActivity]);

  const handleCloseBridge = useCallback(async () => {
    incrementSent();
    logActivity("sent", "Close bridge");
    
    await new Promise(resolve => setTimeout(resolve, 300));
    
    incrementReceived();
    setBridgeStatus({
      state: "CLOSING",
      lastChangeMs: Date.now(),
      lockEngaged: false,
      receivedAt: Date.now(),
    });
    
    // Simulate bridge closing sequence
    setTimeout(() => {
      setBridgeStatus(prev => prev ? {
        ...prev,
        state: "IDLE",
        lockEngaged: true,
        receivedAt: Date.now(),
      } : null);
    }, 2000);
  }, [incrementSent, incrementReceived, logActivity]);

  const handleCarTraffic = useCallback(async (value: CarTrafficState) => {
    incrementSent();
    logActivity("sent", `Change car traffic lights to: ${value}`);
    
    await new Promise(resolve => setTimeout(resolve, 200));
    
    incrementReceived();
    setCarTrafficStatus({
      left: { value, receivedAt: Date.now() },
      right: { value, receivedAt: Date.now() },
    });
  }, [incrementSent, incrementReceived, logActivity]);

  const handleBoatTraffic = useCallback(async (side: "left" | "right", value: BoatTrafficState) => {
    incrementSent();
    logActivity("sent", `Change ${side} boat traffic light: ${value}`);
    
    await new Promise(resolve => setTimeout(resolve, 200));
    
    incrementReceived();
    setBoatTrafficStatus(prev => ({
      left: side === "left" 
        ? { value, receivedAt: Date.now() }
        : prev?.left || { value: "Red", receivedAt: Date.now() },
      right: side === "right"
        ? { value, receivedAt: Date.now() }
        : prev?.right || { value: "Red", receivedAt: Date.now() },
    }));
  }, [incrementSent, incrementReceived, logActivity]);

  const handleResetSystem = useCallback(async () => {
    incrementSent();
    logActivity("sent", "Reset system to idle defaults");
    
    await new Promise(resolve => setTimeout(resolve, 300));
    
    incrementReceived();
    const now = Date.now();
    
    setBridgeStatus({
      state: "IDLE",
      lastChangeMs: now,
      lockEngaged: true,
      receivedAt: now,
    });
    
    setCarTrafficStatus({
      left: { value: "Green", receivedAt: now },
      right: { value: "Green", receivedAt: now },
    });
    
    setBoatTrafficStatus({
      left: { value: "Red", receivedAt: now },
      right: { value: "Red", receivedAt: now },
    });
    
    logActivity("received", "System reset applied. Bridge returning to idle.");
  }, [incrementSent, incrementReceived, logActivity]);

  const handleSimulationSensors = useCallback(async (updates: {
    ultrasonicLeft?: boolean;
    ultrasonicRight?: boolean;
    beamBreak?: boolean;
  }) => {
    incrementSent();
    logActivity("sent", "Adjust simulation sensors");
    
    await new Promise(resolve => setTimeout(resolve, 200));
    
    incrementReceived();
    setSystemStatus(prev => ({
      ...prev!,
      simulationSensors: {
        ...prev!.simulationSensors!,
        ...updates,
      },
      receivedAt: Date.now(),
    }));
  }, [incrementSent, incrementReceived, logActivity]);

  return {
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
  };
}

