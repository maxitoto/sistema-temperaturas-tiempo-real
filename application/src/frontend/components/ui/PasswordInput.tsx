import React, { useState } from "react";
import { Eye, EyeOff } from "lucide-react";

export interface PasswordInputProps
  extends Omit<React.InputHTMLAttributes<HTMLInputElement>, "type"> {
  leftIcon?: React.ReactNode;
  containerClassName?: string;
}

export function PasswordInput({
  leftIcon,
  className = "",
  containerClassName = "",
  disabled,
  ...rest
}: PasswordInputProps) {
  const [visible, setVisible] = useState(false);

  return (
    <div className={`relative ${containerClassName}`}>
      {leftIcon && (
        <span className="pointer-events-none absolute top-1/2 left-3 -translate-y-1/2 text-foreground-secondary">
          {leftIcon}
        </span>
      )}
      <input
        {...rest}
        type={visible ? "text" : "password"}
        disabled={disabled}
        className={`${className} pr-9`}
      />
      <button
        type="button"
        tabIndex={-1}
        disabled={disabled}
        onClick={() => setVisible((v) => !v)}
        aria-label={visible ? "Ocultar contraseña" : "Mostrar contraseña"}
        title={visible ? "Ocultar contraseña" : "Mostrar contraseña"}
        className="absolute top-1/2 right-2.5 -translate-y-1/2 rounded p-0.5 text-foreground-secondary transition-colors hover:text-foreground disabled:cursor-not-allowed disabled:opacity-40"
      >
        {visible ? (
          <EyeOff className="h-4 w-4" />
        ) : (
          <Eye className="h-4 w-4" />
        )}
      </button>
    </div>
  );
}