import { useEffect, useState } from "react";

/**
 * Hook for tracking packets sent/received with localStorage persistence
 */
export function usePacketTracking() {
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

  // Persist packets sent to localStorage
  useEffect(() => {
    localStorage.setItem("packetsSent", packetsSent.toString());
  }, [packetsSent]);

  // Persist packets received to localStorage
  useEffect(() => {
    localStorage.setItem("packetsReceived", packetsReceived.toString());
  }, [packetsReceived]);

  const incrementSent = () => {
    setPacketsSent((prev) => prev + 1);
    setLastSentAt(Date.now());
  };

  const incrementReceived = (count = 1) => {
    setPacketsReceived((prev) => prev + count);
    setLastReceivedAt(Date.now());
  };

  return {
    packetsSent,
    packetsReceived,
    lastSentAt,
    lastReceivedAt,
    incrementSent,
    incrementReceived,
  };
}

