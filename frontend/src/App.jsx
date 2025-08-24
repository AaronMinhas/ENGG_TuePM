import { useState, useRef, useEffect } from "react";
import Divider from "./components/Divider";
import StatusControls from "./components/StatusControls";
import CarTrafficSection from "./components/CarTrafficSection";
import BoatTrafficSection from "./components/BoatTrafficSection";
import BridgeControls from "./components/BridgeControls";

function App() {
  const [bridgeStatus, setBridgeStatus] = useState(() => {
    return sessionStorage.getItem("bridge-status") || "DISCONNECTED";
  });

  const wsRef = useRef(null);

  useEffect(() => {
    const ws = new WebSocket("ws://192.168.0.173/ws");

    ws.onopen = () => {
      console.log("Connected to ESP32");
      setBridgeStatus("CONNECTED");
    };

    ws.onmessage = (event) => {
      console.log("From ESP:", event.data);

      try {
        const data = JSON.parse(event.data);
        if (data.bridge) {
          setBridgeStatus(data.bridge.toUpperCase());
          sessionStorage.setItem("bridge-status", data.bridge.toUpperCase());
        }
      } catch {
        setBridgeStatus(event.data);
        sessionStorage.setItem("bridge-status", event.data);
      }
    };

    ws.onclose = () => {
      console.log("Disconnected");
      setBridgeStatus("DISCONNECTED");
    };

    ws.onerror = (err) => {
      console.error("WebSocket error", err);
      setBridgeStatus("ERROR");
    };

    wsRef.current = ws;

    return () => ws.close();
  }, []);

  const sendCommand = (cmd) => {
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      wsRef.current.send(cmd);
      console.log("Sent:", cmd);
    } else {
      console.warn("WebSocket not open");
    }
  };

  return (
    <div className="min-h-screen flex flex-col justify-center items-center mx-2">
      <div className="w-full max-w-xl bg-gray-100 flex flex-col justify-center items-center space-y-4 rounded-lg p-4">
        <h1 className="text-2xl font-bold">Bridge User Interface</h1>

        {/* TEST BUTTON */}
        <button
          onClick={sendCommand}
          className="p-4 bg-black text-white font-medium cursor-pointer hover:bg-gray-500"
        >
          TEST WEBSOCKET
        </button>

        <Divider />

        <StatusControls setBridgeStatus={setBridgeStatus} />

        <Divider />

        <BridgeControls bridgeStatus={bridgeStatus} />

        <Divider />

        <CarTrafficSection title={"Road Traffic Left Status"} />
        <CarTrafficSection title={"Road Traffic Right Status"} />
        <BoatTrafficSection title={"Boat Traffic Light Status 1"} />
        <BoatTrafficSection title={"Boat Traffic Light Status 2"} />

        <Divider />
      </div>
    </div>
  );
}

export default App;
