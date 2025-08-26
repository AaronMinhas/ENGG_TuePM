/**
 * What needs to be done here:
 * - Need to add an indication of WIFI strength --> Using RSSI range -- closer to 0 the stronger
 * - Need to wire in dropdown buttons that fetch each relevant status of the system
 * - Activity log in UI
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
import type { BridgeStatus, CarTrafficStatus, BoatTrafficStatus, SystemStatus } from "./lib/schema";

import TopNav from "./components/TopNav";
import DashCard from "./components/DashCard";
import BridgeCard from "./components/BridgeCard";
import ActivitySec from "./components/ActivitySec";

function App() {
  const [packetsSent, setPacketsSent] = useState(0);
  const [packetsReceived, setPacketsReceived] = useState(0);
  const [lastSentAt, setLastSentAt] = useState<number | null>(null);
  const [lastReceivedAt, setLastReceivedAt] = useState<number | null>(null);

  const [wsStatus, setWsStatus] = useState<"Connecting" | "Open" | "Closed">("Connecting");
  const [bridge, setBridge] = useState<BridgeStatus | null>(null);
  const [carTraffic, setCarTrafficState] = useState<CarTrafficStatus | null>(null);
  const [boatTraffic, setBoatTrafficState] = useState<BoatTrafficStatus | null>(null);
  const [system, setSystem] = useState<SystemStatus | null>(null);

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
      setCarTrafficState({ ...data, receivedAt: Date.now() });
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
      setBoatTrafficState({ ...data, receivedAt: Date.now() });
    } catch (e) {
      console.error("Boat traffic status error:", e);
    }
  };

  const handleFetchSystem = async () => {
    try {
      setPacketsSent((prev) => prev + 1);
      setLastSentAt(Date.now());
      const data = await getSystemStatus();
      setPacketsReceived((prev) => prev + 1);
      setLastReceivedAt(Date.now());
      setSystem({ ...data, receivedAt: Date.now() });
    } catch (e) {
      console.error("System status error:", e);
    }
  };

  const handleOpenBridge = async () => {
    setPacketsSent((prev) => prev + 1);
    setLastSentAt(Date.now());
    const data = await setBridgeState("Open");
    setPacketsReceived((prev) => prev + 1);
    setLastReceivedAt(Date.now());
    setBridge({ ...data, receivedAt: Date.now() });
  };

  const handleCloseBridge = async () => {
    setPacketsSent((prev) => prev + 1);
    setLastSentAt(Date.now());
    const data = await setBridgeState("Closed");
    setPacketsReceived((prev) => prev + 1);
    setLastReceivedAt(Date.now());
    setBridge({ ...data, receivedAt: Date.now() });
  };

  const handleCarTraffic = async (side: "left" | "right", colour: "Red" | "Yellow" | "Green") => {
    setPacketsSent((prev) => prev + 1);
    setLastSentAt(Date.now());
    const data = await setCarTraffic(side, colour);
    setPacketsReceived((prev) => prev + 1);
    setLastReceivedAt(Date.now());
    setCarTrafficState({ ...data, receivedAt: Date.now() });
  };

  const handleBoatTraffic = async (side: "left" | "right", colour: "Red" | "Green") => {
    setPacketsSent((prev) => prev + 1);
    setLastSentAt(Date.now());
    const data = await setBoatTraffic(side, colour);
    setPacketsReceived((prev) => prev + 1);
    setLastReceivedAt(Date.now());
    setBoatTrafficState({ ...data, receivedAt: Date.now() });
  };

  useEffect(() => {
    // const client = getESPClient("ws://192.168.0.173/ws");
    const client = getESPClient("ws://172.20.10.7/ws");
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
        setCarTrafficState({ ...ct, receivedAt: Date.now() });
        setBoatTrafficState({ ...bt, receivedAt: Date.now() });
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

      <div className="flex justify-center">
        <div className="grid grid-cols-2 lg:grid-cols-4 lg:grid-rows-4 gap-4 max-w-[1700px] w-full mx-4 mt-4">
          <DashCard
            title="Bridge State"
            options={["Open", "Close"]}
            description={bridge?.state || "..."}
            updatedAt={timeAgo(bridge?.receivedAt)}
            cardType="STATE"
            iconType="BRIDGE"
            bridgeStateType={bridge?.state || undefined}
          />
          <DashCard
            title="Left Car Traffic"
            options={["Red", "Yellow", "Green"]}
            description={carTraffic?.left || "..."}
            updatedAt={timeAgo(carTraffic?.left.receivedAt)}
            cardType="STATE"
            iconType="CAR"
            carStateType={carTraffic?.left || undefined}
          />
          <DashCard
            title="Left Boat Traffic"
            options={["Red", "Green"]}
            description={boatTraffic?.left || "..."}
            updatedAt={timeAgo(boatTraffic?.left.receivedAt)}
            cardType="STATE"
            iconType="BOAT"
            boatStateType={boatTraffic?.left || undefined}
          />
          <ActivitySec />

          <DashCard
            title="System State"
            options={["Connect", "Disconnect"]}
            description={system?.connection || "..."}
            updatedAt={timeAgo(system?.receivedAt)}
            cardType="STATE"
            iconType="SYSTEM"
            systemStateType={system?.connection || undefined}
          />

          <DashCard
            title="Right Car Traffic"
            options={["Red", "Yellow", "Green"]}
            description={carTraffic?.right || "..."}
            updatedAt={timeAgo(carTraffic?.right.receivedAt)}
            cardType="STATE"
            iconType="CAR"
            carStateType={carTraffic?.right || undefined}
          />
          <DashCard
            title="Right Boat Traffic"
            options={["Red", "Green"]}
            description={boatTraffic?.right || "..."}
            updatedAt={timeAgo(boatTraffic?.right.receivedAt)}
            cardType="STATE"
            iconType="BOAT"
            boatStateType={boatTraffic?.right || undefined}
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
      </div>

      {/* Turn into dropdown buttons */}
      {/* <div className="mt-20">
        <p>
          <strong>WebSocket:</strong> {wsStatus}
        </p>
        <button className="px-3 py-1 m-1 bg-gray-200 rounded" onClick={handleFetchSystem}>
          Get System Status
        </button>
        {system && (
          <pre className="bg-gray-100 p-2 rounded mt-2">{JSON.stringify(system, null, 2)}</pre>
        )}
      </div>

      <p>
        <strong>Bridge State:</strong> {bridge?.state ?? "?"}
      </p>
      <button className="px-3 py-1 m-1 bg-green-200 rounded" onClick={handleOpenBridge}>
        Open Bridge
      </button>
      <button className="px-3 py-1 m-1 bg-red-200 rounded" onClick={handleCloseBridge}>
        Close Bridge
      </button>
      <button className="px-3 py-1 m-1 bg-gray-200 rounded" onClick={handleFetchBridge}>
        Get Bridge Status
      </button>
      {bridge && (
        <pre className="bg-gray-100 p-2 rounded mt-2">{JSON.stringify(bridge, null, 2)}</pre>
      )} */}
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
