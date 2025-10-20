import { useState } from "react";
import CustomButton from "./CustomButton";

function CarTrafficSection({ title }) {
  const [activeLight, setActiveLight] = useState("red");

  const lights = ["green", "yellow", "red"];

  const activeColours = {
    red: "bg-red-600",
    yellow: "bg-yellow-600",
    green: "bg-green-600",
  };

  const inactiveColours = {
    red: "bg-red-200",
    yellow: "bg-yellow-200",
    green: "bg-green-200",
  };

  return (
    <section className="flex flex-col justify-center items-center space-y-2">
      <h2 className="text-lg font-bold">{title}</h2>

      <div className="flex items-center justify-center gap-2">
        {lights.map((colour) => (
          <button
            key={colour}
            className={`w-3 h-3 rounded-full ${
              colour === activeLight ? activeColours[colour] : inactiveColours[colour]
            }`}
          />
        ))}
      </div>

      <div className="flex items-center justify-center gap-2">
        <CustomButton text={"Green"} />
        <CustomButton text={"Yellow"} />
        <CustomButton text={"Red"} />
      </div>
    </section>
  );
}

export default CarTrafficSection;
