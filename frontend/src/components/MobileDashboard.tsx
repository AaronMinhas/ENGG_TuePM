import React from "react";
import { BridgeStatus, CarTrafficStatus, BoatTrafficStatus, SystemStatus, CarTrafficState, BoatTrafficState } from "../lib/schema";
import { Icon, ActivityEntry } from "../types/GenTypes";
import { timeAgo } from "../utils/timeAgo";
import DashCard from "./DashCard";
import BridgeCard from "./BridgeCard";
import ActivitySec from "./ActivitySec";

interface MobileDashboardProps {
  readonly bridgeStatus: BridgeStatus | null;
  readonly carTrafficStatus: CarTrafficStatus | null;
  readonly boatTrafficStatus: BoatTrafficStatus | null;
  readonly systemStatus: SystemStatus | null;
  readonly packetsSent: number;
  readonly packetsReceived: number;
  readonly lastSentAt: number | null;
  readonly lastReceivedAt: number | null;
  readonly activityLog: ActivityEntry[];
  readonly handleOpenBridge: () => void;
  readonly handleCloseBridge: () => void;
  readonly handleCarTraffic: (value: CarTrafficState) => void;
  readonly handleBoatTraffic: (side: "left" | "right", value: BoatTrafficState) => void;
  readonly handleFetchSystem: () => void;
  readonly controlsDisabled?: boolean;
}

export default function MobileDashboard({
  bridgeStatus,
  carTrafficStatus,
  boatTrafficStatus,
  systemStatus,
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
  controlsDisabled = false,
}: MobileDashboardProps) {
  return (
    <div className="grid lg:hidden grid-cols-2 grid-rows-8 h-[calc(100dvh - 300px)] min-h-0 w-full gap-4 mx-4 mb-4">
      <div className="col-span-2 row-span-2">
        <ActivitySec log={activityLog} />
      </div>

      <div className="col-span-2 row-span-2">
        <BridgeCard bridgeStatus={bridgeStatus} />
      </div>

      <DashCard
        title="Bridge State"
        variant="STATE"
        iconT={Icon.BRIDGE}
        options={[
          { id: "m-b-o", label: "Open", action: handleOpenBridge },
          { id: "m-b-c", label: "Close", action: handleCloseBridge },
        ]}
        description={bridgeStatus?.state || ""}
        updatedAt={bridgeStatus?.receivedAt ? timeAgo(bridgeStatus?.receivedAt) : ""}
        status={bridgeStatus?.state ? { kind: "bridge", value: bridgeStatus.state } : undefined}
        disabled={controlsDisabled}
      />
      <DashCard
        title="System State"
        variant="STATE"
        iconT={Icon.SYSTEM}
        options={[{ id: "m-s", label: "Update Status", action: handleFetchSystem }]}
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
        disabled={controlsDisabled}
      />
      <DashCard
        title="Boat Traffic"
        variant="DUAL_STATE"
        iconT={Icon.BOAT}
        leftLabel="Left"
        leftOptions={[
          { id: "m-bt-l-r", label: "Red", action: () => handleBoatTraffic("left", "Red") },
          { id: "m-bt-l-g", label: "Green", action: () => handleBoatTraffic("left", "Green") },
        ]}
        leftStatus={
          boatTrafficStatus?.left.value
            ? { kind: "boat", value: boatTrafficStatus.left.value }
            : undefined
        }
        rightLabel="Right"
        rightOptions={[
          { id: "m-bt-r-r", label: "Red", action: () => handleBoatTraffic("right", "Red") },
          { id: "m-bt-r-g", label: "Green", action: () => handleBoatTraffic("right", "Green") },
        ]}
        rightStatus={
          boatTrafficStatus?.right.value
            ? { kind: "boat", value: boatTrafficStatus.right.value }
            : undefined
        }
        updatedAt={boatTrafficStatus?.left.receivedAt ? timeAgo(boatTrafficStatus?.left.receivedAt) : ""}
        disabled={controlsDisabled}
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
  );
}
