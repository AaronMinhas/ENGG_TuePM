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

import { useEffect, useState } from "react";
import {
  getESPClient,
  getBridgeStatus,
  getCarTrafficStatus,
  getBoatTrafficStatus,
  getSystemStatus,
  setBridgeState,
  setCarTraffic,
  setBoatTraffic,
} from "./lib/api";
import type {
  BridgeStatus,
  CarTrafficStatus,
  BoatTrafficStatus,
  SystemStatus,
  BoatTrafficLight,
  CarTrafficLight,
} from "./lib/schema";

import TopNav from "./components/TopNav";
import DashCard from "./components/DashCard";
import BridgeCard from "./components/BridgeCard";
import ActivitySec from "./components/ActivitySec";

type ActivityEntry = {
  type: "sent" | "received";
  message: string;
  timestamp: number;
};

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
  const [bridge, setBridge] = useState<BridgeStatus | null>(null);
  const [carTraffic, setCarTrafficState] = useState<CarTrafficStatus | null>(null);
  const [boatTraffic, setBoatTrafficState] = useState<BoatTrafficStatus | null>(null);
  const [system, setSystem] = useState<SystemStatus | null>(null);

  const [activityLog, setActivityLog] = useState<ActivityEntry[]>([]);

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
      const data = await getBridgeStatus();
      setLastReceivedAt(Date.now());
      setBridge({ ...data, receivedAt: Date.now() });
    } catch (e) {
      console.error("Bridge status error:", e);
    }
  };

  const handleFetchCarTraffic = async () => {
    try {
      setPacketsSent((prev) => prev + 1);
      setLastSentAt(Date.now());
      const data = await getCarTrafficStatus();
      setPacketsReceived((prev) => prev + 1);
      setLastReceivedAt(Date.now());
      setCarTrafficState({
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
      const data = await getBoatTrafficStatus();
      setPacketsReceived((prev) => prev + 1);
      setLastReceivedAt(Date.now());
      setBoatTrafficState({
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

      const data = await getSystemStatus();
      setPacketsReceived((prev) => prev + 1);
      setLastReceivedAt(Date.now());
      setSystem({ ...data, receivedAt: Date.now() });
      logActivity("received", `System status: ${data.connection}`);
    } catch (e) {
      console.error("System status error:", e);
    }
  };

  const handleOpenBridge = async () => {
    setPacketsSent((prev) => prev + 1);
    setLastSentAt(Date.now());
    logActivity("sent", "Open bridge");

    const data = await setBridgeState("Open");
    setPacketsReceived((prev) => prev + 1);
    setLastReceivedAt(Date.now());
    setBridge({ ...data, receivedAt: Date.now() });
    logActivity("received", `Bridge state: ${data.state}`);
  };

  const handleCloseBridge = async () => {
    setPacketsSent((prev) => prev + 1);
    setLastSentAt(Date.now());
    logActivity("sent", "Close bridge");

    const data = await setBridgeState("Closed");
    setPacketsReceived((prev) => prev + 1);
    setLastReceivedAt(Date.now());
    setBridge({ ...data, receivedAt: Date.now() });
    logActivity("received", `Bridge state: ${data.state}`);
  };

  const handleCarTraffic = async (side: "left" | "right", value: CarTrafficLight) => {
    setPacketsSent((prev) => prev + 1);
    setLastSentAt(Date.now());
    logActivity("sent", `Change ${side} car traffic light to: ${value}`);

    const data = await setCarTraffic(side, value);
    setPacketsReceived((prev) => prev + 1);
    setLastReceivedAt(Date.now());
    setCarTrafficState(() => ({
      left: { ...data.left, receivedAt: Date.now() },
      right: { ...data.right, receivedAt: Date.now() },
    }));
    logActivity(
      "received",
      `Car traffic ${side} state: ${side === "left" ? `${data.left.value}` : `${data.right.value}`}`
    );
  };

  const handleBoatTraffic = async (side: "left" | "right", value: BoatTrafficLight) => {
    setPacketsSent((prev) => prev + 1);
    setLastSentAt(Date.now());
    logActivity("sent", `Change ${side} boat traffic light: ${value}`);

    const data = await setBoatTraffic(side, value);
    setPacketsReceived((prev) => prev + 1);
    setLastReceivedAt(Date.now());
    setBoatTrafficState({
      left: { ...data.left, receivedAt: Date.now() },
      right: { ...data.right, receivedAt: Date.now() },
    });
    logActivity(
      "received",
      `Boat traffic ${side} state: ${side === "left" ? `${data.left.value}` : `${data.right.value}`}`
    );
  };

  useEffect(() => {
    const client = getESPClient("ws://192.168.0.173/ws");
    // const client = getESPClient("ws://172.20.10.7/ws");
    client.onStatus(setWsStatus);

    const poll = async () => {
      try {
        const [b, ct, bt, s] = await Promise.all([
          getBridgeStatus(),
          getCarTrafficStatus(),
          getBoatTrafficStatus(),
          getSystemStatus(),
        ]);
        setPacketsReceived((prev) => prev + 4);
        setBridge({ ...b, receivedAt: Date.now() });
        setCarTrafficState({
          left: { ...ct.left, receivedAt: Date.now() },
          right: { ...ct.right, receivedAt: Date.now() },
          // left: { ...normTraffic(ct.left), receivedAt: Date.now() },
          // right: { ...normTraffic(ct.right), receivedAt: Date.now() },
        });
        setBoatTrafficState({
          // left: { ...normTraffic(bt.left), receivedAt: Date.now() },
          // right: { ...normTraffic(bt.right), receivedAt: Date.now() },
          left: { ...bt.left, receivedAt: Date.now() },
          right: { ...bt.right, receivedAt: Date.now() },
        });
        setSystem({ ...s, receivedAt: Date.now() });
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
            options={[
              { label: "Open", action: () => handleOpenBridge() },
              { label: "Close", action: () => handleCloseBridge() },
            ]}
            description={bridge?.state || ""}
            updatedAt={bridge?.receivedAt ? timeAgo(bridge?.receivedAt) : ""}
            cardType="STATE"
            iconType="BRIDGE"
            bridgeStateType={bridge?.state || undefined}
          />
          <DashCard
            title="Left Car Traffic"
            options={[
              { label: "Red", action: () => handleCarTraffic("left", "Red") },
              { label: "Yellow", action: () => handleCarTraffic("left", "Yellow") },
              { label: "Green", action: () => handleCarTraffic("left", "Green") },
            ]}
            description={carTraffic?.left.value || ""}
            updatedAt={carTraffic?.left.receivedAt ? timeAgo(carTraffic?.left.receivedAt) : ""}
            cardType="STATE"
            iconType="CAR"
            carStateType={carTraffic?.left.value || undefined}
          />
          <DashCard
            title="Left Boat Traffic"
            options={[
              { label: "Red", action: () => handleBoatTraffic("left", "Red") },
              { label: "Green", action: () => handleBoatTraffic("left", "Green") },
            ]}
            description={boatTraffic?.left.value || ""}
            updatedAt={boatTraffic?.left.receivedAt ? timeAgo(boatTraffic?.left.receivedAt) : ""}
            cardType="STATE"
            iconType="BOAT"
            boatStateType={boatTraffic?.left.value || undefined}
          />
          <ActivitySec log={activityLog} />

          <DashCard
            title="System State"
            options={[{ label: "Update Status", action: () => handleFetchSystem() }]}
            description={system?.connection || ""}
            updatedAt={system?.receivedAt ? timeAgo(system?.receivedAt) : ""}
            cardType="STATE"
            iconType="SYSTEM"
            systemStateType={system?.connection || undefined}
          />

          <DashCard
            title="Right Car Traffic"
            options={[
              { label: "Red", action: () => handleCarTraffic("right", "Red") },
              { label: "Yellow", action: () => handleCarTraffic("right", "Yellow") },
              { label: "Green", action: () => handleCarTraffic("right", "Green") },
            ]}
            description={carTraffic?.right.value || ""}
            updatedAt={carTraffic?.right.receivedAt ? timeAgo(carTraffic?.right.receivedAt) : ""}
            cardType="STATE"
            iconType="CAR"
            carStateType={carTraffic?.right.value || undefined}
          />
          <DashCard
            title="Right Boat Traffic"
            options={[
              { label: "Red", action: () => handleBoatTraffic("right", "Red") },
              { label: "Green", action: () => handleBoatTraffic("right", "Green") },
            ]}
            description={boatTraffic?.right.value || ""}
            updatedAt={boatTraffic?.right.receivedAt ? timeAgo(boatTraffic?.right.receivedAt) : ""}
            cardType="STATE"
            iconType="BOAT"
            boatStateType={boatTraffic?.right.value || undefined}
          />

          <DashCard
            title="Packets Sent"
            description={packetsSent.toString()}
            updatedAt={lastSentAt ? timeAgo(lastSentAt) : ""}
            iconType="PACKETS_SEND"
          />
          <BridgeCard />
          <DashCard
            title="Packets Received"
            description={packetsReceived.toString()}
            updatedAt={lastReceivedAt ? timeAgo(lastReceivedAt) : ""}
            iconType="PACKETS_REC"
          />
        </div>

        {/* Mobile layout grid-rows-[repeat(8, minmax(0,1fr))]*/}
        <div className="grid lg:hidden grid-cols-2 auto-rows-auto grid-rows-[repeat(6,minmax(0,1fr))] h-[calc(100dvh - 300px)] min-h-0 w-full gap-4 mx-4 mb-4">
          <ActivitySec log={activityLog} />

          <BridgeCard />

          <DashCard
            title="Bridge State"
            options={[
              { label: "Open", action: () => handleOpenBridge() },
              { label: "Close", action: () => handleCloseBridge() },
            ]}
            description={bridge?.state || ""}
            updatedAt={bridge?.receivedAt ? timeAgo(bridge?.receivedAt) : ""}
            cardType="STATE"
            iconType="BRIDGE"
            bridgeStateType={bridge?.state || undefined}
          />
          <DashCard
            title="System State"
            options={[{ label: "Update Status", action: () => handleFetchSystem() }]}
            description={system?.connection || ""}
            updatedAt={system?.receivedAt ? timeAgo(system?.receivedAt) : ""}
            cardType="STATE"
            iconType="SYSTEM"
            systemStateType={system?.connection || undefined}
          />

          <DashCard
            title="Left Car Traffic"
            options={[
              { label: "Red", action: () => handleCarTraffic("left", "Red") },
              { label: "Yellow", action: () => handleCarTraffic("left", "Yellow") },
              { label: "Green", action: () => handleCarTraffic("left", "Green") },
            ]}
            description={carTraffic?.left.value || ""}
            updatedAt={carTraffic?.left.receivedAt ? timeAgo(carTraffic?.left.receivedAt) : ""}
            cardType="STATE"
            iconType="CAR"
            carStateType={carTraffic?.left.value || undefined}
          />
          <DashCard
            title="Left Boat Traffic"
            options={[
              { label: "Red", action: () => handleBoatTraffic("left", "Red") },
              { label: "Green", action: () => handleBoatTraffic("left", "Green") },
            ]}
            description={boatTraffic?.left.value || ""}
            updatedAt={boatTraffic?.left.receivedAt ? timeAgo(boatTraffic?.left.receivedAt) : ""}
            cardType="STATE"
            iconType="BOAT"
            boatStateType={boatTraffic?.left.value || undefined}
          />
          <DashCard
            title="Right Car Traffic"
            options={[
              { label: "Red", action: () => handleCarTraffic("right", "Red") },
              { label: "Yellow", action: () => handleCarTraffic("right", "Yellow") },
              { label: "Green", action: () => handleCarTraffic("right", "Green") },
            ]}
            description={carTraffic?.right.value || ""}
            updatedAt={carTraffic?.right.receivedAt ? timeAgo(carTraffic?.right.receivedAt) : ""}
            cardType="STATE"
            iconType="CAR"
            carStateType={carTraffic?.right.value || undefined}
          />
          <DashCard
            title="Right Boat Traffic"
            options={[
              { label: "Red", action: () => handleBoatTraffic("right", "Red") },
              { label: "Green", action: () => handleBoatTraffic("right", "Green") },
            ]}
            description={boatTraffic?.right.value || ""}
            updatedAt={boatTraffic?.right.receivedAt ? timeAgo(boatTraffic?.right.receivedAt) : ""}
            cardType="STATE"
            iconType="BOAT"
            boatStateType={boatTraffic?.right.value || undefined}
          />

          <DashCard
            title="Packets Sent"
            description={packetsSent.toString()}
            updatedAt={lastSentAt ? timeAgo(lastSentAt) : ""}
            iconType="PACKETS_SEND"
          />
          <DashCard
            title="Packets Received"
            description={packetsReceived.toString()}
            updatedAt={lastReceivedAt ? timeAgo(lastReceivedAt) : ""}
            iconType="PACKETS_REC"
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
