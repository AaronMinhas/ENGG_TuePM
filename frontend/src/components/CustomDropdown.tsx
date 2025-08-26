import { useState, useRef, useEffect } from "react";
import { ChevronDown } from "lucide-react";

type CustomDropdownProps = {
  options: string[];
  onSelect: (value: string) => void;
  selected?: string;
};

export function CustomDropdown({ options, onSelect, selected }: CustomDropdownProps) {
  const [open, setOpen] = useState(false);
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const handler = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) {
        setOpen(false);
      }
    };
    document.addEventListener("mousedown", handler);
    return () => document.removeEventListener("mousedown", handler);
  }, []);

  return (
    <div ref={ref} className="relative w-full">
      <button
        onClick={() => setOpen(!open)}
        className="w-full bg-white group hover:bg-gray-100 rounded-md px-3 py-2 flex justify-between items-center border border-white hover:border-base-400"
      >
        <span>{selected || "Select..."}</span>
        <ChevronDown
          className={`opacity-0 group-hover:opacity-100 transition-transform ${
            open ? "rotate-180" : ""
          }`}
        />
      </button>

      {open && (
        <ul className="absolute z-10 mt-1 w-full bg-white border border-base-400 rounded-md shadow-md">
          {options.map((opt) => (
            <li
              key={opt}
              onClick={() => {
                onSelect(opt);
                setOpen(false);
              }}
              className="px-3 py-2 cursor-pointer hover:bg-gray-100 rounded-md"
            >
              {opt}
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}
