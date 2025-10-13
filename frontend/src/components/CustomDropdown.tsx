import React, { useState, useRef, useEffect } from "react";
import { CustomDropdownProps } from "../types/CustomDropdown";
import { ChevronDown } from "lucide-react";

export function CustomDropdown(props: Readonly<CustomDropdownProps>) {
  const { options, selected, colour } = props;

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
        className={`w-full bg-white group cursor-pointer hover:bg-gray-100 rounded-md px-3 py-2 flex justify-between items-center border border-base-200 lg:border-white hover:border-base-400 ${
          open ? "outline-2 outline-blue-600" : ""
        }`}
      >
        <div className="flex gap-2 items-center">
          {colour && <span className={`w-5 h-5 rounded-full ${colour}`} />}
          <span>{selected || "---"}</span>
        </div>
        <ChevronDown
          className={`opacity-100 lg:opacity-0 text-base-600 group-hover:opacity-100 transition-transform ${
            open ? "rotate-180" : ""
          }`}
        />
      </button>

      {open && (
        <ul className="absolute z-10 mt-1 w-full bg-white border border-base-400 rounded-md shadow-md">
          {options.map((opt) => (
            <li
              key={opt.id}
              onClick={() => {
                opt.action();
                setOpen(false);
              }}
              className="px-3 py-2 cursor-pointer hover:bg-gray-100 rounded-md"
            >
              {opt.label}
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}
