import React from "react";
import { Users } from "lucide-react";
import { VehicleTrafficStatus } from "../lib/schema";
import { timeAgo } from "../utils/timeAgo";

interface VehicleQueueCardProps {
  readonly data: VehicleTrafficStatus | null;
}

export function VehicleQueueCard({ data }: VehicleQueueCardProps) {
  const left = data?.left ?? 0;
  const right = data?.right ?? 0;
  const updatedAt = data?.receivedAt ? timeAgo(data.receivedAt) : "Waiting for data";

  return (
    <div className="w-full h-full bg-white p-4 flex flex-col justify-between rounded-md border border-base-400 shadow-[0_0_2px_rgba(0,0,0,0.25)] space-y-3">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <Users size={22} />
          <span className="font-semibold">Vehicle Queue</span>
        </div>
      </div>

      <div className="grid grid-cols-2 gap-4">
        <QueueColumn label="Left" value={left} tone="text-blue-600" />
        <QueueColumn label="Right" value={right} tone="text-purple-600" />
      </div>

      <span className="text-sm text-gray-600">{updatedAt}</span>
    </div>
  );
}

interface QueueColumnProps {
  label: string;
  value: number;
  tone: string;
}

function QueueColumn({ label, value, tone }: QueueColumnProps) {
  return (
    <div className="flex flex-col gap-1">
      <span className="text-xs uppercase tracking-wide text-gray-500">{label}</span>
      <span className={`text-3xl font-semibold ${tone}`}>{value}</span>
    </div>
  );
}

