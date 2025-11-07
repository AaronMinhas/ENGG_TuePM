import { useEffect, useRef, useState } from "react";
import {
  BridgeStatus,
  CarTrafficStatus,
  BoatTrafficStatus,
  SystemStatus,
  SensorStatus,
} from "../lib/schema";

interface UseESPWebSocketMockProps {
  setBridgeStatus: React.Dispatch<React.SetStateAction<BridgeStatus | null>>;
  setCarTrafficStatus: React.Dispatch<React.SetStateAction<CarTrafficStatus | null>>;
  setBoatTrafficStatus: React.Dispatch<React.SetStateAction<BoatTrafficStatus | null>>;
  setSystemStatus: React.Dispatch<React.SetStateAction<SystemStatus | null>>;
  setSensorStatus: React.Dispatch<React.SetStateAction<SensorStatus | null>>;
  incrementReceived: (count?: number) => void;
  logActivity: (type: "sent" | "received", message: string) => void;
  carTrafficStatus: CarTrafficStatus | null;
  boatTrafficStatus: BoatTrafficStatus | null;
}

export interface UseESPWebSocketMockReturn {
  reconnect: () => void;
  refreshData: () => void;
}

/**
 * Simulates periodic updates and WebSocket-like behavior
 */
export function useESPWebSocketMock({
  setBridgeStatus,
  setCarTrafficStatus,
  setBoatTrafficStatus,
  setSystemStatus,
  setSensorStatus,
  incrementReceived,
  logActivity,
}: UseESPWebSocketMockProps): UseESPWebSocketMockReturn {
  const intervalRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const updateCounterRef = useRef(0);

  // Simulate periodic data updates
  const simulateUpdate = () => {
    updateCounterRef.current += 1;
    const now = Date.now();


    if (updateCounterRef.current % 3 === 0) {
      setSensorStatus(prev => {
        if (prev?.beamBreak) {
          return prev; // Keep current sensor state, FSM is in control
        }
        
        const leftDistance = 30 + Math.random() * 30;
        const rightDistance = 30 + Math.random() * 30;
        
        return {
          leftUltrasonic: {
            distanceCm: Math.round(leftDistance),
            zone: leftDistance < 30 ? "close" : "near",
          },
          rightUltrasonic: {
            distanceCm: Math.round(rightDistance),
            zone: rightDistance < 30 ? "close" : "near",
          },
          beamBreak: false,
          direction: "none",
          receivedAt: now,
        };
      });
      incrementReceived();
    }

    // Simulate system status updates
    if (updateCounterRef.current % 5 === 0) {
      setSystemStatus(prev => ({
        ...prev!,
        uptimeMs: (prev?.uptimeMs || 0) + 5000,
        rssi: -60 - Math.random() * 20, // random RSSI
        receivedAt: now,
      }));
      incrementReceived();
    }

    // Occasionally simulate log messages
    if (updateCounterRef.current % 7 === 0) {
      const messages = [
        "Bridge state machine: IDLE",
        "Traffic lights: All systems normal",
        "Sensors: No boat detected",
        "System: All checks passed",
        "Bridge: Lock engaged",
      ];
      const message = messages[Math.floor(Math.random() * messages.length)];
      logActivity("received", message);
    }
  };

  useEffect(() => {
    // Initial data fetch simulation
    const initialFetch = async () => {
      await new Promise(resolve => setTimeout(resolve, 500));
      
      const now = Date.now();
      setBridgeStatus({
        state: "IDLE",
        lastChangeMs: now - 5000,
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
      
      setSystemStatus({
        connection: "Connected",
        rssi: -65,
        uptimeMs: 3600000,
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
        receivedAt: now,
      });
      
      setSensorStatus({
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
        receivedAt: now,
      });
      
      incrementReceived(5);
      logActivity("received", "Connected to test mode - simulating bridge system");
    };

    initialFetch();

    // Simulate periodic updates (similar to WebSocket events)
    intervalRef.current = setInterval(simulateUpdate, 2000);

    // Set connection status
    setSystemStatus(prev => ({
      ...prev!,
      connection: "Connected",
      receivedAt: Date.now(),
    }));

    return () => {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
      }
    };
  }, []);

  const reconnect = () => {
    logActivity("received", "Reconnecting in test mode...");
    if (intervalRef.current) {
      clearInterval(intervalRef.current);
    }
    
    // Simulate reconnection
    setTimeout(() => {
      setSystemStatus(prev => ({
        ...prev!,
        connection: "Connected",
        receivedAt: Date.now(),
      }));
      logActivity("received", "Reconnected successfully (test mode)");
      intervalRef.current = setInterval(simulateUpdate, 2000);
    }, 500);
  };

  const refreshData = () => {
    simulateUpdate();
    logActivity("received", "Data refreshed (test mode)");
  };

  return {
    reconnect,
    refreshData,
  };
}

