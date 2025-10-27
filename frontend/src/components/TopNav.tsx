import React from "react";

interface TopNavProps {
  onReset?: () => void | Promise<void>;
  resetting?: boolean;
}

export default function TopNav({ onReset, resetting = false }: TopNavProps) {
  const handleResetClick = () => {
    void onReset?.();
  };

  return (
    <div className="w-full h-12 bg-white border border-l-0 border-r-0 border-t-0 border-b-base-400 flex justify-between items-center px-5">
      <h1 className="font-bold text-2xl">Group 32</h1>
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
  );
}
