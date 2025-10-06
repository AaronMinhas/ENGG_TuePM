/**
 * Main dashboard that controls and monitors the ESP bridge system.
 *
 * Responsibilities:
 * - Establishes WebSocket connection using `getESPClient`.
 * - Polls the ESP status endpoints every 5 seconds to keep UI state fresh
 * - Maintains local React state for:
 *   - Bridge status (open/closed, last change time, lock engaged)
 *   - Car traffic lights (left/right)
 *   - Boat traffic lights (left/right)
 *   - System connection status (connected/connecting/disconnected, RSSI, uptime)
 *   - Packets sent/received counters with timestamps
 *   - Activity log of sent/received messages (max 50 entries)
 * - Provides handlers to send commands to the ESP32:
 *   - Open/close bridge
 *   - Change car traffic light (left/right)
 *   - Change boat traffic light (left/right)
 *   - Fetch system/bridge/traffic statuses on demand
 * - Renders a responsive dashboard layout:
 *   - Desktop (grid of DashCards, BridgeCard, ActivitySec)
 *   - Mobile (stacked grid with same components)
 * - Persists packet counters in localStorage so they survive reloads.
 *
 *
 * TODO:
 * - Persist Activity logs over refreshes
 * - Persist all states of the system over a refresh (packets already do this)
 * - Visual Bridge animation
 * - Need to add an indication of WIFI strength --> Using RSSI range -- closer to 0 the stronger
 */

import { useEffect, useRef, useState } from "react";
import {
  getESPClient,
  getBridgeState,
  getCarTrafficState,
  getBoatTrafficState,
  getSystemState,
  setBridgeState,
  setCarTrafficState,
  setBoatTrafficState,
} from "./lib/api";
import type {
  BridgeStatus,
  CarTrafficStatus,
  CarTrafficState,
  BoatTrafficStatus,
  BoatTrafficState,
  SystemStatus,
  EventMsgT,
} from "./lib/schema";
import { IP, ActivityEntry, Icon } from "./types/GenTypes";

import TopNav from "./components/TopNav";
import DashCard from "./components/DashCard";
import BridgeCard from "./components/BridgeCard";
import ActivitySec from "./components/ActivitySec";

function App() {
  const [packetsSent, setPacketsSent] = useState(() => {
    const saved = localStorage.getItem("packetsSent");
    return saved ? parseInt(saved, 10) : 0;
  });
  const [packetsReceived, setPacketsReceived] = useState(() => {
    const saved = localStorage.getItem("packetsReceived");
    return saved ? parseInt(saved, 10) : 0;
  });
  const [lastSentAt, setLastSentAt] = useState<number | null>(null);
  const [lastReceivedAt, setLastReceivedAt] = useState<number | null>(null);

  const [wsStatus, setWsStatus] = useState<"Connecting" | "Open" | "Closed">("Connecting");
  const [bridgeStatus, setBridgeStatus] = useState<BridgeStatus | null>(null);
  const [carTrafficStatus, setCarTrafficStatus] = useState<CarTrafficStatus | null>(null);
  const [boatTrafficStatus, setBoatTrafficStatus] = useState<BoatTrafficStatus | null>(null);
  const [systemStatus, setSystemStatus] = useState<SystemStatus | null>(null);

  const [activityLog, setActivityLog] = useState<ActivityEntry[]>([]);
  const lastBridgeStateRef = useRef<string | null>(null);
  const seenLogRef = useRef<Set<string>>(new Set());

  const logActivity = (type: "sent" | "received", message: string) => {
    setActivityLog((prev) => [{ type, message, timestamp: Date.now() }, ...prev.slice(0, 49)]);
  };

  useEffect(() => {
    localStorage.setItem("packetsSent", packetsSent.toString());
  }, [packetsSent]);

  useEffect(() => {
    localStorage.setItem("packetsReceived", packetsReceived.toString());
  }, [packetsReceived]);

  const handleFetchBridge = async () => {
    try {
      setPacketsSent((prev) => prev + 1);
      setLastSentAt(Date.now());
      const data = await getBridgeState();
      setLastReceivedAt(Date.now());
      setBridgeStatus({ ...data, receivedAt: Date.now() });
    } catch (e) {
      console.error("Bridge status error:", e);
    }
  };

  const handleFetchCarTraffic = async () => {
    try {
      setPacketsSent((prev) => prev + 1);
      setLastSentAt(Date.now());
      const data = await getCarTrafficState();
      setPacketsReceived((prev) => prev + 1);
      setLastReceivedAt(Date.now());
      setCarTrafficStatus({
        left: { ...data.left, receivedAt: Date.now() },
        right: { ...data.right, receivedAt: Date.now() },
      });
    } catch (e) {
      console.error("Car traffic status error:", e);
    }
  };

  const handleFetchBoatTraffic = async () => {
    try {
      setPacketsSent((prev) => prev + 1);
      setLastSentAt(Date.now());
      const data = await getBoatTrafficState();
      setPacketsReceived((prev) => prev + 1);
      setLastReceivedAt(Date.now());
      setBoatTrafficStatus({
        left: { ...data.left, receivedAt: Date.now() },
        right: { ...data.right, receivedAt: Date.now() },
      });
    } catch (e) {
      console.error("Boat traffic status error:", e);
    }
  };

  const handleFetchSystem = async () => {
    try {
      setPacketsSent((prev) => prev + 1);
      setLastSentAt(Date.now());
      logActivity("sent", "System status request");

      const data = await getSystemState();
      setPacketsReceived((prev) => prev + 1);
      setLastReceivedAt(Date.now());
      setSystemStatus({ ...data, receivedAt: Date.now() });
      logActivity("received", `System status: ${data.connection}`);
    } catch (e) {
      console.error("System status error:", e);
    }
  };

  const handleOpenBridge = async () => {
    setPacketsSent((prev) => prev + 1);
    setLastSentAt(Date.now());
    logActivity("sent", "Open bridge");

    const data: any = await setBridgeState("Open");
    setPacketsReceived((prev) => prev + 1);
    setLastReceivedAt(Date.now());
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
    setPacketsSent((prev) => prev + 1);
    setLastSentAt(Date.now());
    logActivity("sent", "Close bridge");

    const data: any = await setBridgeState("Closed");
    setPacketsReceived((prev) => prev + 1);
    setLastReceivedAt(Date.now());
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
    setPacketsSent((prev) => prev + 1);
    setLastSentAt(Date.now());
    logActivity("sent", `Change car traffic lights to: ${value}`);

    const data = await setCarTrafficState(value);
    setPacketsReceived((prev) => prev + 1);
    setLastReceivedAt(Date.now());
    setCarTrafficStatus(() => ({
      left: { ...data.left, receivedAt: Date.now() },
      right: { ...data.right, receivedAt: Date.now() },
    }));
    logActivity("received", `Car traffic lights set to: ${value}`);
  };

  const handleBoatTraffic = async (side: "left" | "right", value: BoatTrafficState) => {
    setPacketsSent((prev) => prev + 1);
    setLastSentAt(Date.now());
    logActivity("sent", `Change ${side} boat traffic light: ${value}`);

    const data = await setBoatTrafficState(side, value);
    setPacketsReceived((prev) => prev + 1);
    setLastReceivedAt(Date.now());
    setBoatTrafficStatus({
      left: { ...data.left, receivedAt: Date.now() },
      right: { ...data.right, receivedAt: Date.now() },
    });
    logActivity("received", `Boat traffic ${side} state: ${value}`);
  };

  useEffect(() => {
    const client = getESPClient(IP.AARON_4);
    client.onStatus(setWsStatus);

    // Real-time Activity from snapshot events
    client.onEvent((evt: EventMsgT) => {
      if (evt.type !== "event" || evt.path !== "/system/snapshot") return;
      const payload: any = evt.payload || {};
      const bridge = payload.bridge || {};
      const traffic = payload.traffic || {};
      const sys = payload.system || {};
      const logArr: string[] = Array.isArray(payload.log) ? payload.log : [];

      // Update tiles opportunistically
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
      if (sys.connection)
        setSystemStatus({
          connection: sys.connection,
          rssi: sys.rssi,
          uptimeMs: sys.uptimeMs,
          receivedAt: Date.now(),
        });

      // Activity: log bridge state changes
      if (bridge.state && bridge.state !== lastBridgeStateRef.current) {
        logActivity("received", `Bridge state: ${bridge.state}`);
        lastBridgeStateRef.current = bridge.state;
        setPacketsReceived((prev) => prev + 1);
        setLastReceivedAt(Date.now());
      }

      // Append new log lines
      if (logArr.length) {
        const seen = seenLogRef.current;
        for (const line of logArr) {
          if (!seen.has(line)) {
            logActivity("received", line);
            seen.add(line);
          }
        }
        // cap seen size
        if (seen.size > 256) {
          seenLogRef.current = new Set(Array.from(seen).slice(-128));
        }
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
        setPacketsReceived((prev) => prev + 4);
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
      } catch (err) {
        console.error("Poll error:", err);
      }
    };

    poll();
    const id = setInterval(poll, 5000);

    return () => clearInterval(id);
  }, []);

  return (
    <div className="bg-gray-100 min-h-screen w-full">
      <TopNav />

      <div className="flex justify-center mt-4">
        {/* Desktop layout */}
        <div
          className="hidden lg:grid grid-cols-2 lg:grid-cols-4 grid-rows-[repeat(8,minmax(0,1fr))] 
            lg:grid-rows-[repeat(4,minmax(0,1fr))] gap-4 lg:h-[calc(100dvh-670px)]
            overflow-hidden min-h-0 max-w-[1700px] w-full mx-4"
        >
          <DashCard
            title="Bridge State"
            variant="STATE"
            iconT={Icon.BRIDGE}
            options={[
              { id: "d-b-o", label: "Open", action: () => handleOpenBridge() },
              { id: "d-b-c", label: "Close", action: () => handleCloseBridge() },
            ]}
            description={bridgeStatus?.state || ""}
            updatedAt={bridgeStatus?.receivedAt ? timeAgo(bridgeStatus?.receivedAt) : ""}
            status={bridgeStatus?.state ? { kind: "bridge", value: bridgeStatus.state } : undefined}
          />
          <DashCard
            title="Car Traffic"
            variant="STATE"
            iconT={Icon.CAR}
            options={[
              { id: "d-c-r", label: "Red", action: () => handleCarTraffic("Red") },
              { id: "d-c-y", label: "Yellow", action: () => handleCarTraffic("Yellow") },
              { id: "d-c-g", label: "Green", action: () => handleCarTraffic("Green") },
            ]}
            description={`L:${carTrafficStatus?.left.value || "?"} R:${carTrafficStatus?.right.value || "?"}`}
            updatedAt={carTrafficStatus?.left.receivedAt ? timeAgo(carTrafficStatus?.left.receivedAt) : ""}
            status={
              carTrafficStatus?.left.value ? { kind: "car", value: carTrafficStatus.left.value } : undefined
            }
          />
          <DashCard
            title="Left Boat Traffic"
            variant="STATE"
            iconT={Icon.BOAT}
            options={[
              { id: "d-lb-r", label: "Red", action: () => handleBoatTraffic("left", "Red") },
              { id: "d-lb-g", label: "Green", action: () => handleBoatTraffic("left", "Green") },
            ]}
            description={boatTrafficStatus?.left.value || ""}
            updatedAt={boatTrafficStatus?.left.receivedAt ? timeAgo(boatTrafficStatus?.left.receivedAt) : ""}
            status={
              boatTrafficStatus?.left.value
                ? { kind: "boat", value: boatTrafficStatus.left.value }
                : undefined
            }
          />
          <ActivitySec log={activityLog} />

          <DashCard
            title="System State"
            variant="STATE"
            iconT={Icon.SYSTEM}
            options={[{ id: "d-s", label: "Update Status", action: () => handleFetchSystem() }]}
            description={systemStatus?.connection || ""}
            updatedAt={systemStatus?.receivedAt ? timeAgo(systemStatus?.receivedAt) : ""}
            status={systemStatus?.connection ? { kind: "system", value: systemStatus.connection } : undefined}
          />

          <DashCard
            title="Right Boat Traffic"
            variant="STATE"
            iconT={Icon.BOAT}
            options={[
              { id: "d-rb-r", label: "Red", action: () => handleBoatTraffic("right", "Red") },
              { id: "d-rb-g", label: "Green", action: () => handleBoatTraffic("right", "Green") },
            ]}
            description={boatTrafficStatus?.right.value || ""}
            updatedAt={
              boatTrafficStatus?.right.receivedAt ? timeAgo(boatTrafficStatus?.right.receivedAt) : ""
            }
            status={
              boatTrafficStatus?.right.value
                ? { kind: "boat", value: boatTrafficStatus.right.value }
                : undefined
            }
          />

          <DashCard
            title="Packets Sent"
            iconT={Icon.PACKETS_SEND}
            description={packetsSent.toString()}
            updatedAt={lastSentAt ? timeAgo(lastSentAt) : ""}
          />
          <BridgeCard />
          <DashCard
            title="Packets Received"
            iconT={Icon.PACKETS_REC}
            description={packetsReceived.toString()}
            updatedAt={lastReceivedAt ? timeAgo(lastReceivedAt) : ""}
          />
        </div>

        {/* Mobile layout grid-rows-[repeat(8, minmax(0,1fr))]*/}
        <div className="grid lg:hidden grid-cols-2 auto-rows-auto grid-rows-[repeat(6,minmax(0,1fr))] h-[calc(100dvh - 300px)] min-h-0 w-full gap-4 mx-4 mb-4">
          <ActivitySec log={activityLog} />

          <BridgeCard />

          <DashCard
            title="Bridge State"
            variant="STATE"
            iconT={Icon.BRIDGE}
            options={[
              { id: "m-b-o", label: "Open", action: () => handleOpenBridge() },
              { id: "m-b-c", label: "Close", action: () => handleCloseBridge() },
            ]}
            description={bridgeStatus?.state || ""}
            updatedAt={bridgeStatus?.receivedAt ? timeAgo(bridgeStatus?.receivedAt) : ""}
            status={bridgeStatus?.state ? { kind: "bridge", value: bridgeStatus.state } : undefined}
          />
          <DashCard
            title="System State"
            variant="STATE"
            iconT={Icon.SYSTEM}
            options={[{ id: "m-s", label: "Update Status", action: () => handleFetchSystem() }]}
            description={systemStatus?.connection || ""}
            updatedAt={systemStatus?.receivedAt ? timeAgo(systemStatus?.receivedAt) : ""}
            status={systemStatus?.connection ? { kind: "system", value: systemStatus.connection } : undefined}
          />

          <DashCard
            title="Car Traffic"
            variant="STATE"
            iconT={Icon.CAR}
            options={[
              { id: "m-c-r", label: "Red", action: () => handleCarTraffic("Red") },
              { id: "m-c-y", label: "Yellow", action: () => handleCarTraffic("Yellow") },
              { id: "m-c-g", label: "Green", action: () => handleCarTraffic("Green") },
            ]}
            description={`L:${carTrafficStatus?.left.value || "?"} R:${carTrafficStatus?.right.value || "?"}`}
            updatedAt={carTrafficStatus?.left.receivedAt ? timeAgo(carTrafficStatus?.left.receivedAt) : ""}
            status={
              carTrafficStatus?.left.value ? { kind: "car", value: carTrafficStatus.left.value } : undefined
            }
          />
          <DashCard
            title="Left Boat Traffic"
            variant="STATE"
            iconT={Icon.BOAT}
            options={[
              { id: "m-lb-r", label: "Red", action: () => handleBoatTraffic("left", "Red") },
              { id: "m-lb-g", label: "Green", action: () => handleBoatTraffic("left", "Green") },
            ]}
            description={boatTrafficStatus?.left.value || ""}
            updatedAt={boatTrafficStatus?.left.receivedAt ? timeAgo(boatTrafficStatus?.left.receivedAt) : ""}
            status={
              boatTrafficStatus?.left.value
                ? { kind: "boat", value: boatTrafficStatus.left.value }
                : undefined
            }
          />
          <DashCard
            title="Right Boat Traffic"
            variant="STATE"
            iconT={Icon.BOAT}
            options={[
              { id: "m-rb-r", label: "Red", action: () => handleBoatTraffic("right", "Red") },
              { id: "m-rb-g", label: "Green", action: () => handleBoatTraffic("right", "Green") },
            ]}
            description={boatTrafficStatus?.right.value || ""}
            updatedAt={
              boatTrafficStatus?.right.receivedAt ? timeAgo(boatTrafficStatus?.right.receivedAt) : ""
            }
            status={
              boatTrafficStatus?.right.value
                ? { kind: "boat", value: boatTrafficStatus.right.value }
                : undefined
            }
          />

          <DashCard
            title="Packets Sent"
            iconT={Icon.PACKETS_SEND}
            description={packetsSent.toString()}
            updatedAt={lastSentAt ? timeAgo(lastSentAt) : ""}
          />
          <DashCard
            title="Packets Received"
            iconT={Icon.PACKETS_REC}
            description={packetsReceived.toString()}
            updatedAt={lastReceivedAt ? timeAgo(lastReceivedAt) : ""}
          />
        </div>
      </div>
    </div>
  );
}

function timeAgo(timestampMs: number): string {
  const now = Date.now();
  const diffMs = now - timestampMs;
  const diffSec = Math.floor(diffMs / 1000);

  if (diffSec < 60) return `${diffSec} second${diffSec !== 1 ? "s" : ""} ago`;

  const diffMin = Math.floor(diffSec / 60);
  if (diffMin < 60) return `${diffMin} minute${diffMin !== 1 ? "s" : ""} ago`;

  const diffHr = Math.floor(diffMin / 60);
  if (diffHr < 24) return `${diffHr} hour${diffHr !== 1 ? "s" : ""} ago`;

  const diffDays = Math.floor(diffHr / 24);
  return `${diffDays} day${diffDays !== 1 ? "s" : ""} ago`;
}

export default App;
