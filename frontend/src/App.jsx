import TopNav from "./components/TopNav";
import DashCard from "./components/DashCard";
import BridgeCard from "./components/BridgeCard";
import ActivitySec from "./components/ActivitySec";

function App() {
  return (
    <div className="bg-gray-100 min-h-screen w-full">
      <TopNav />

      <div className="flex justify-center">
        <div className="grid grid-cols-4 grid-rows-4 gap-4 max-w-[1700px] w-full mx-4 mt-4">
          <DashCard
            title="Bridge State"
            description="Closed"
            updatedAt="3 minutes ago"
            cardType="STATE"
            iconType="BRIDGE"
            bridgeStateType="CLOSED"
          />
          <DashCard
            title="Car Traffic"
            description="Green"
            updatedAt="3 minutes ago"
            cardType="STATE"
            iconType="CAR"
            carStateType="GREEN"
          />
          <DashCard
            title="Boat Traffic"
            description="Red"
            updatedAt="3 minutes ago"
            cardType="STATE"
            iconType="BOAT"
            boatStateType="RED"
          />
          <ActivitySec />

          <DashCard
            title="System State"
            description="Connected"
            updatedAt="3 minutes ago"
            cardType="STATE"
            iconType="SYSTEM"
            systemStateType="CONNECTED"
          />
          <BridgeCard />

          <DashCard
            title="Packets Sent"
            description="403"
            updatedAt="3 minutes ago"
            iconType="PACKETS_SEND"
          />
          <DashCard
            title="Packets Received"
            description="394"
            updatedAt="3 minutes ago"
            iconType="PACKETS_REC"
          />
        </div>
      </div>
    </div>
  );
}

export default App;
