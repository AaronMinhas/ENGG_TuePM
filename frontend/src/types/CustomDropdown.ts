export type DropdownOption = {
  id: string;
  label: string;
  action: () => void | Promise<void>;
};

export type CustomDropdownProps = {
  options: DropdownOption[];
  selected?: string;
  colour?: string;
  disabled?: boolean;
  compact?: boolean;
};
