/**
 * This section aims to display a list of the past few commands sent and received to the frontend in the UI
 */

import { Activity } from "lucide-react";

type ActivityEntry = {
  type: "sent" | "received";
  message: string;
  timestamp: number;
};

type Props = {
  log: ActivityEntry[];
};

export default function ActivitySec({ log }: Readonly<Props>) {
  return (
    <div className="bg-white rounded-md p-4 row-span-2 col-span-2 lg:col-span-1 lg:row-span-4 border border-base-400 shadow-[0_0_2px_rgba(0,0,0,0.25)] h-full min-h-0 w-full flex flex-col">
      <div className="flex gap-2 mb-4">
        <Activity size={22} />
        <span className="font-semibold">Activity</span>
      </div>

      <div className="cursor-default max-h-[200px] lg:max-h-none rounded-lg min-h-0 border border-base-200 h-full p-4 overflow-y-auto flex-1 space-y-2">
        {log.length === 0 ? (
          <p className="text-gray-500">No activity yet...</p>
        ) : (
          log.map((entry, idx) => (
            <div
              key={idx}
              className={`flex justify-between items-center text-sm px-2 py-1 rounded ${
                entry.type === "sent" ? "bg-blue-100" : "bg-green-100"
              }`}
            >
              <span>
                <strong>{entry.type === "sent" ? "Sent:" : "Received:"}</strong> {entry.message}
              </span>
              <span className="text-xs text-gray-500">{new Date(entry.timestamp).toLocaleTimeString()}</span>
            </div>
          ))
        )}
      </div>
    </div>
  );
}
