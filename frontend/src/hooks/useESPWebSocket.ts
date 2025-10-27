import { useEffect, useRef, useState } from "react";
import {
  getESPClient,
  getBridgeState,
  getCarTrafficState,
  getBoatTrafficState,
  getVehicleTrafficStatus,
  getSystemState,
} from "../lib/api";
import {
  BridgeStatus,
  CarTrafficStatus,
  BoatTrafficStatus,
  SystemStatus,
  EventMsgT,
  VehicleTrafficStatus,
} from "../lib/schema";
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
  setVehicleTrafficStatus: React.Dispatch<React.SetStateAction<VehicleTrafficStatus | null>>;
  setSystemStatus: React.Dispatch<React.SetStateAction<SystemStatus | null>>;
  incrementReceived: (count?: number) => void;
  logActivity: (type: "sent" | "received", message: string) => void;
  carTrafficStatus: CarTrafficStatus | null;
  boatTrafficStatus: BoatTrafficStatus | null;
  vehicleTrafficStatus: VehicleTrafficStatus | null;
}

export function useESPWebSocket({
  setBridgeStatus,
  setCarTrafficStatus,
  setBoatTrafficStatus,
  setVehicleTrafficStatus,
  setSystemStatus,
  incrementReceived,
  logActivity,
  carTrafficStatus,
  boatTrafficStatus,
  vehicleTrafficStatus,
}: UseESPWebSocketProps) {
  const lastBridgeStateRef = useRef<string | null>(null);
  const lastLogSeqRef = useRef<number>(-1);
  const legacyLogRef = useRef<Set<string>>(new Set());

  useEffect(() => {
    const client = getESPClient(IP.AARON_4);

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
      if (traffic.vehicles) {
        const leftCount = typeof traffic.vehicles.left === "number" ? traffic.vehicles.left : vehicleTrafficStatus?.left ?? 0;
        const rightCount = typeof traffic.vehicles.right === "number" ? traffic.vehicles.right : vehicleTrafficStatus?.right ?? 0;
        setVehicleTrafficStatus({
          left: leftCount,
          right: rightCount,
          receivedAt: Date.now(),
        });
      }
      if (sys.connection)
        setSystemStatus({
          connection: sys.connection,
          rssi: sys.rssi,
          uptimeMs: sys.uptimeMs,
          receivedAt: Date.now(),
        });

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
        const [b, ct, bt, s, vc] = await Promise.all([
          getBridgeState(),
          getCarTrafficState(),
          getBoatTrafficState(),
          getSystemState(),
          getVehicleTrafficStatus(),
        ]);
        incrementReceived(5);
        setBridgeStatus({ ...b, receivedAt: Date.now() });
        setCarTrafficStatus({
          left: { ...ct.left, receivedAt: Date.now() },
          right: { ...ct.right, receivedAt: Date.now() },
        });
        setBoatTrafficStatus({
          left: { ...bt.left, receivedAt: Date.now() },
          right: { ...bt.right, receivedAt: Date.now() },
        });
        setSystemStatus({ ...s, receivedAt: Date.now() });
        setVehicleTrafficStatus({ ...vc, receivedAt: Date.now() });
      } catch (err) {
        console.error("Poll error:", err);
      }
    };

    poll();
    const id = setInterval(poll, 5000);

    return () => clearInterval(id);
  }, []);
}
