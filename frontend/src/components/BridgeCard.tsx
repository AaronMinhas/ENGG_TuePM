/**
 * This component will be in the centre of the dashboard.
 * Its purpose is to display the bridges current state in an interesting way for the user, such as
 * showing a "bridge" open or closed based on the state.
 */

export default function BridgeCard() {
  return (
    <div className="bg-white rounded-md p-4 col-span-2 row-span-2 flex items-center justify-center border-base-400 shadow-[0_0_2px_rgba(0,0,0,0.25)] border">
      <span>Big Bridge</span>
    </div>
  );
}
