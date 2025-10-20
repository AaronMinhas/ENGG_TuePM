import LoadingAnimation from "./LoadingAnimation";

export default function CustomButton({ text, onClick, loading = false }) {
  return (
    <button
      onClick={onClick}
      disabled={loading}
      className={`px-4 py-2 bg-gray-300 flex justify-center items-center rounded-md font-medium h-10 min-w-24 max-h-10 cursor-pointer ${
        loading ? "opacity-50" : "opacity-100"
      }`}
    >
      {loading ? <LoadingAnimation colour="text-gray-700" /> : text}
    </button>
  );
}
