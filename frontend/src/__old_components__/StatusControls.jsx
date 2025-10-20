import { useState } from "react";
import CustomButton from "./CustomButton";

const BackendStatus = {
  CONNECTED: "CONNECTED",
  CONNECTING: "CONNECTING",
  DISCONNECTED: "DISCONNECTED",
};

const EspStatus = {
  CONNECTED: "CONNECTED",
  DISCONNECTED: "DISCONNECTED",
};

const BridgeStatus = {
  OPEN: "OPEN",
  CLOSED: "CLOSED",
  DISCONNECTED: "DISCONNECTED",
};

function StatusControls({ setBridgeStatus }) {
  const [backendLoading, setBackendLoading] = useState(false);
  const [backendStatus, setBackendStatus] = useState(() => {
    return sessionStorage.getItem("backend-status") || BackendStatus.DISCONNECTED;
  });
  const [espStatus, setEspStatus] = useState(() => {
    return sessionStorage.getItem("esp-status") || EspStatus.DISCONNECTED;
  });

  const getColour = (status) => {
    switch (status) {
      case "CONNECTED":
        return "bg-green-600";
      case "CONNECTING":
        return "bg-blue-600";
      case "DISCONNECTED":
      default:
        return "bg-red-600";
    }
  };

  const handleConnectToBackend = async () => {
    setBackendLoading(true);
    setBackendStatus(BackendStatus.CONNECTING);
    sessionStorage.setItem("backend-status", BackendStatus.CONNECTING);

    try {
      const result = await fetch("http://localhost:9002/api/status");
      const data = await result.json();

      setBackendStatus(data.backend);
      setBridgeStatus(data.bridge);
      setEspStatus(data.esp);

      sessionStorage.setItem("backend-status", data.backend);
      sessionStorage.setItem("bridge-status", data.bridge);
      sessionStorage.setItem("esp-status", data.esp);
    } catch (error) {
      console.error("Connection error:", error);
      setBackendStatus(BackendStatus.DISCONNECTED);
      sessionStorage.setItem("backend-status", BackendStatus.DISCONNECTED);
    } finally {
      setBackendLoading(false);
    }
  };

  return (
    <section className="flex flex-col justify-center items-center">
      <h2 className="text-lg font-bold mb-1">Connect Controls</h2>
      <div className="flex items-center justify-center gap-2 mb-3">
        <h2 className="text-md font-medium">Backend Status:</h2>
        <button className={`w-3 h-3 mt-1 rounded-full ${getColour(backendStatus)}`} />
        <h2 className="text-md font-medium">ESP Status:</h2>
        <button className={`w-3 h-3 mt-1 rounded-full ${getColour(espStatus)}`} />
      </div>

      <div className="flex items-center justify-center gap-2">
        <CustomButton text={"Backend"} onClick={handleConnectToBackend} loading={backendLoading} />
        <CustomButton text={"ESP"} />
      </div>
    </section>
  );
}

export default StatusControls;
