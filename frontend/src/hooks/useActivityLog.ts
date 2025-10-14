import { useState } from "react";
import { ActivityEntry } from "../types/GenTypes";

export function useActivityLog() {
  const [activityLog, setActivityLog] = useState<ActivityEntry[]>([]);

  const logActivity = (type: "sent" | "received", message: string) => {
    setActivityLog((prev) => [
      { type, message, timestamp: Date.now() },
      ...prev.slice(0, 49),
    ]);
  };

  return {
    activityLog,
    logActivity,
  };
}

