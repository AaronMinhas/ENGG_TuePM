import React from "react";

interface TopNavProps {
  onReset?: () => void | Promise<void>;
  resetting?: boolean;
  onReconnect?: () => void;
}

export default function TopNav({ onReset, resetting = false, onReconnect }: TopNavProps) {
  const handleResetClick = () => {
    void onReset?.();
  };

  const handleReconnectClick = () => {
    console.log("[TopNav] Reconnect button clicked");
    onReconnect?.();
  };

  return (
    <div className="w-full h-12 bg-white border border-l-0 border-r-0 border-t-0 border-b-base-400 flex justify-between items-center px-5">
      <h1 className="font-bold text-2xl text-gray-900">Group 32</h1>
      <div className="flex gap-2">
        <button
          type="button"
          onClick={handleReconnectClick}
          className="px-4 py-1.5 rounded-md bg-blue-600 text-white font-semibold shadow-sm hover:bg-blue-700 focus:outline-none focus:ring-2 focus:ring-offset-1 focus:ring-blue-500 transition-colors"
          title="Manually reconnect to the ESP32 WebSocket."
        >
          Reconnect
        </button>
        <button
          type="button"
          onClick={handleResetClick}
          disabled={resetting}
          className="px-4 py-1.5 rounded-md bg-red-600 text-white font-semibold shadow-sm hover:bg-red-700 focus:outline-none focus:ring-2 focus:ring-offset-1 focus:ring-red-500 disabled:bg-red-300 disabled:text-red-100 disabled:cursor-not-allowed transition-colors"
          title="Force the bridge to close, car lights to green, and boat lights to red."
        >
          {resetting ? "Resetting..." : "Reset System"}
        </button>
      </div>
    </div>
  );
}
