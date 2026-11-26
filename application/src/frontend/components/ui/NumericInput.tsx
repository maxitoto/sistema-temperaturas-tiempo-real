import React, { useEffect, useRef, useState } from "react";
import {
  formatearNumeroInput,
  parsearNumeroInput,
  redondearDecimal,
} from "../../lib/format";

export interface NumericInputProps {
  value: number;
  onChange: (value: number) => void;
  decimals?: number;
  min?: number;
  max?: number;
  allowNegative?: boolean;
  disabled?: boolean;
  required?: boolean;
  placeholder?: string;
  className?: string;
  title?: string;
  id?: string;
}

export function NumericInput({
  value,
  onChange,
  decimals = 2,
  min,
  max,
  allowNegative = true,
  disabled = false,
  required = false,
  placeholder,
  className = "",
  title,
  id,
}: NumericInputProps) {
  const [texto, setTexto] = useState<string>(() =>
    formatearNumeroInput(value, decimals)
  );
  const enfocadoRef = useRef(false);

  useEffect(() => {
    if (!enfocadoRef.current) {
      setTexto(formatearNumeroInput(value, decimals));
    }
  }, [value, decimals]);

  const handleChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const raw = e.target.value;
    const regex =
      decimals > 0
        ? allowNegative
          ? /^-?\d*[.,]?\d*$/
          : /^\d*[.,]?\d*$/
        : allowNegative
          ? /^-?\d*$/
          : /^\d*$/;

    if (!regex.test(raw)) return;
    setTexto(raw);

    const parsed = parsearNumeroInput(raw, decimals);
    if (parsed !== null) {
      onChange(parsed);
    }
  };

  const handleBlur = () => {
    enfocadoRef.current = false;
    let parsed = parsearNumeroInput(texto, decimals);
    if (parsed === null) {
      parsed = Number.isFinite(value) ? redondearDecimal(value, decimals) : 0;
    }
    if (min !== undefined && parsed < min) parsed = min;
    if (max !== undefined && parsed > max) parsed = max;
    parsed = redondearDecimal(parsed, decimals);

    setTexto(formatearNumeroInput(parsed, decimals));
    onChange(parsed);
  };

  return (
    <input
      id={id}
      type="text"
      inputMode={decimals > 0 ? "decimal" : "numeric"}
      value={texto}
      disabled={disabled}
      required={required}
      placeholder={placeholder}
      title={title}
      onFocus={() => {
        enfocadoRef.current = true;
      }}
      onChange={handleChange}
      onBlur={handleBlur}
      className={className}
    />
  );
}