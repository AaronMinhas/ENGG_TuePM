import CustomButton from "./CustomButton";

function BridgeControls({ bridgeStatus }) {
  return (
    <section className="flex flex-col justify-center items-center">
      <h2 className="text-lg font-bold mb-1">Bridge Controls</h2>
      <h2 className="text-md font-medium mb-3">Status: {bridgeStatus || "Unavailable"}</h2>

      <div className="flex items-center justify-center gap-2">
        <CustomButton text={"Open"} />
        <CustomButton text={"Close"} />
      </div>
    </section>
  );
}

export default BridgeControls;
