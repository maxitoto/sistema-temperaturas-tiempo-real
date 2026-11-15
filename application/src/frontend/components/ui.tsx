import type { ButtonHTMLAttributes, InputHTMLAttributes, ReactNode } from 'react';

export function Card({ children, className = '' }: { children: ReactNode; className?: string }) {
    return (
        <div className={`rounded-xl border border-border bg-card text-card-foreground shadow-sm ${className}`}>
            {children}
        </div>
    );
}

export function CardHeader({ title, description, action }: { title: string; description?: string; action?: ReactNode }) {
    return (
        <div className="flex items-start justify-between gap-4 border-b border-border px-5 py-4">
            <div>
                <h3 className="text-base font-semibold text-foreground">{title}</h3>
                {description && <p className="mt-0.5 text-xs text-muted-foreground">{description}</p>}
            </div>
            {action}
        </div>
    );
}

export function CardBody({ children, className = '' }: { children: ReactNode; className?: string }) {
    return <div className={`px-5 py-4 ${className}`}>{children}</div>;
}

export function Button({
    variant = 'primary',
    size = 'md',
    className = '',
    ...rest
}: ButtonHTMLAttributes<HTMLButtonElement> & {
    variant?: 'primary' | 'secondary' | 'danger' | 'ghost';
    size?: 'sm' | 'md';
}) {
    const base = 'inline-flex items-center justify-center gap-2 rounded-md font-medium transition-colors disabled:opacity-50 disabled:pointer-events-none focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-primary/50';
    const sizes = { sm: 'px-2.5 py-1 text-xs', md: 'px-3.5 py-2 text-sm' };
    const variants = {
        primary:   'bg-primary text-primary-foreground hover:brightness-95',
        secondary: 'bg-secondary text-secondary-foreground border border-border hover:bg-muted',
        danger:    'bg-destructive text-error-foreground hover:brightness-95',
        ghost:     'text-foreground hover:bg-muted',
    };
    return <button className={`${base} ${sizes[size]} ${variants[variant]} ${className}`} {...rest} />;
}

export function Input({ className = '', ...rest }: InputHTMLAttributes<HTMLInputElement>) {
    return (
        <input
            className={`w-full rounded-md border border-border bg-background px-3 py-2 text-sm text-foreground placeholder:text-muted-foreground focus:outline-none focus:ring-2 focus:ring-primary/40 ${className}`}
            {...rest}
        />
    );
}

export function Label({ children, htmlFor }: { children: ReactNode; htmlFor?: string }) {
    return <label htmlFor={htmlFor} className="mb-1 block text-xs font-medium text-foreground-secondary">{children}</label>;
}

export function Badge({ children, tone = 'neutral' }: { children: ReactNode; tone?: 'success' | 'danger' | 'warning' | 'info' | 'neutral' }) {
    const tones = {
        success: 'bg-success-light text-success border-success/30',
        danger:  'bg-error-light text-error border-error/30',
        warning: 'bg-warning-light text-warning border-warning/30',
        info:    'bg-info-light text-info border-info/30',
        neutral: 'bg-muted text-foreground-secondary border-border',
    };
    return (
        <span className={`inline-flex items-center rounded-full border px-2 py-0.5 text-xs font-medium ${tones[tone]}`}>
            {children}
        </span>
    );
}

export function EmptyState({ title, message }: { title: string; message?: string }) {
    return (
        <div className="rounded-lg border border-dashed border-border px-4 py-8 text-center">
            <p className="text-sm font-medium text-foreground">{title}</p>
            {message && <p className="mt-1 text-xs text-muted-foreground">{message}</p>}
        </div>
    );
}

export function Spinner({ className = '' }: { className?: string }) {
    return <span className={`inline-block h-3 w-3 animate-spin rounded-full border-2 border-current border-t-transparent ${className}`} />;
}

export function ErrorBanner({ message, onClose }: { message: string; onClose?: () => void }) {
    return (
        <div className="mb-3 flex items-start justify-between gap-3 rounded-md border border-error/30 bg-error-light px-3 py-2 text-xs text-error">
            <span className="break-words">{message}</span>
            {onClose && <button onClick={onClose} className="shrink-0 text-error hover:opacity-70">×</button>}
        </div>
    );
}