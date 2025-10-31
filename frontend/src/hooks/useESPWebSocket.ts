import { useEffect, useRef, useState } from "react";
import { getESPClient, getBridgeState, getCarTrafficState, getBoatTrafficState, getSystemState, reconnectWebSocket } from "../lib/api";
import { BridgeStatus, CarTrafficStatus, BoatTrafficStatus, SystemStatus, EventMsgT } from "../lib/schema";
import { IP } from "../types/GenTypes";

function parseLogLine(line: string): { sequence: number | null; message: string } {
  const separatorIndex = line.indexOf("|");
  if (separatorIndex === -1) {
    return { sequence: null, message: line };
  }

  const possibleSeq = Number(line.slice(0, separatorIndex));
  if (!Number.isFinite(possibleSeq)) {
    return { sequence: null, message: line };
  }

  return {
    sequence: possibleSeq,
    message: line.slice(separatorIndex + 1).trim(),
  };
}

interface UseESPWebSocketProps {
  setBridgeStatus: React.Dispatch<React.SetStateAction<BridgeStatus | null>>;
  setCarTrafficStatus: React.Dispatch<React.SetStateAction<CarTrafficStatus | null>>;
  setBoatTrafficStatus: React.Dispatch<React.SetStateAction<BoatTrafficStatus | null>>;
  setSystemStatus: React.Dispatch<React.SetStateAction<SystemStatus | null>>;
  incrementReceived: (count?: number) => void;
  logActivity: (type: "sent" | "received", message: string) => void;
  carTrafficStatus: CarTrafficStatus | null;
  boatTrafficStatus: BoatTrafficStatus | null;
}

export interface UseESPWebSocketReturn {
  reconnect: () => void;
  refreshData: () => void;
}

export function useESPWebSocket({
  setBridgeStatus,
  setCarTrafficStatus,
  setBoatTrafficStatus,
  setSystemStatus,
  incrementReceived,
  logActivity,
  carTrafficStatus,
  boatTrafficStatus,
}: UseESPWebSocketProps) {
  const lastBridgeStateRef = useRef<string | null>(null);
  const lastLogSeqRef = useRef<number>(-1);
  const legacyLogRef = useRef<Set<string>>(new Set());
  const clientRef = useRef<ReturnType<typeof getESPClient> | null>(null);
  const pollFunctionRef = useRef<(() => Promise<void>) | null>(null);

  useEffect(() => {
    const client = getESPClient(IP.AARON_4);
    clientRef.current = client;

    client.onEvent((evt: EventMsgT) => {
      if (evt.type !== "event" || evt.path !== "/system/snapshot") return;
      const payload: any = evt.payload || {};
      const bridge = payload.bridge || {};
      const traffic = payload.traffic || {};
      const sys = payload.system || {};
      const logArr: string[] = Array.isArray(payload.log) ? payload.log : [];

      if (bridge.state)
        setBridgeStatus({
          state: bridge.state,
          lastChangeMs: bridge.lastChangeMs || 0,
          lockEngaged: !!bridge.lockEngaged,
          receivedAt: Date.now(),
          boatTimerStartMs: bridge.boatTimerStartMs || 0,
          boatTimerSide: bridge.boatTimerSide || "",
        });
      if (traffic.car) {
        setCarTrafficStatus({
          left: {
            value: traffic.car.left?.value ?? (carTrafficStatus?.left.value || "Green"),
            receivedAt: Date.now(),
          },
          right: {
            value: traffic.car.right?.value ?? (carTrafficStatus?.right.value || "Green"),
            receivedAt: Date.now(),
          },
        });
      }
      if (traffic.boat) {
        setBoatTrafficStatus({
          left: {
            value: traffic.boat.left?.value ?? (boatTrafficStatus?.left.value || "Red"),
            receivedAt: Date.now(),
          },
          right: {
            value: traffic.boat.right?.value ?? (boatTrafficStatus?.right.value || "Red"),
            receivedAt: Date.now(),
          },
        });
      }
      if (sys.connection)
        setSystemStatus((prev) => ({
          connection: sys.connection,
          rssi: sys.rssi,
          uptimeMs: sys.uptimeMs,
          simulation: typeof sys.simulation === "boolean" ? sys.simulation : prev?.simulation,
          simulationSensors: sys.simulationSensors ?? prev?.simulationSensors,
          receivedAt: Date.now(),
        }));

      if (bridge.state && bridge.state !== lastBridgeStateRef.current) {
        lastBridgeStateRef.current = bridge.state;
        incrementReceived();
      }

      if (logArr.length) {
        const seenLegacy = legacyLogRef.current;
        let highestSequence = lastLogSeqRef.current;

        for (const line of logArr) {
          const { sequence, message } = parseLogLine(line);
          if (sequence !== null) {
            if (sequence > highestSequence) {
              logActivity("received", message);
              highestSequence = sequence;
            }
            continue;
          }

          if (!seenLegacy.has(message)) {
            logActivity("received", message);
            seenLegacy.add(message);
          }
        }

        if (seenLegacy.size > 512) {
          legacyLogRef.current = new Set(Array.from(seenLegacy).slice(-256));
        }

        lastLogSeqRef.current = highestSequence;
      }
    });

    const poll = async () => {
      try {
        const [b, ct, bt, s] = await Promise.all([
          getBridgeState(),
          getCarTrafficState(),
          getBoatTrafficState(),
          getSystemState(),
        ]);
        incrementReceived(4);
        setBridgeStatus({ 
          ...b, 
          receivedAt: Date.now(),
          boatTimerStartMs: b.boatTimerStartMs || 0,
          boatTimerSide: b.boatTimerSide || "",
        });
        setCarTrafficStatus({
          left: { ...ct.left, receivedAt: Date.now() },
          right: { ...ct.right, receivedAt: Date.now() },
        });
        setBoatTrafficStatus({
          left: { ...bt.left, receivedAt: Date.now() },
          right: { ...bt.right, receivedAt: Date.now() },
        });
        setSystemStatus({ ...s, receivedAt: Date.now() });
      } catch (err) {
        console.error("Poll error:", err);
      }
    };

    // Store poll function reference so we can call it manually
    pollFunctionRef.current = poll;

    poll();
    const id = setInterval(poll, 5000);

    // Listen to WebSocket status changes
    client.onStatus((status) => {
      setSystemStatus((prev) => ({
        ...prev,
        connection: status === "Open" ? "Connected" : status === "Connecting" ? "Connecting" : "Disconnected",
        receivedAt: Date.now(),
      } as SystemStatus));
    });

    return () => clearInterval(id);
  }, []);

  return {
    reconnect: () => {
      // Use the global reconnect function which accesses the same client instance
      reconnectWebSocket();
      // Immediately fetch current state - no delay
      if (pollFunctionRef.current) {
        console.log("[Reconnect] Immediately fetching current state...");
        pollFunctionRef.current();
      }
    },
    refreshData: () => {
      // Allow manual refresh of data
      if (pollFunctionRef.current) {
        pollFunctionRef.current();
      }
    },
  };
}
