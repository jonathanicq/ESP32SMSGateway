import { NavLink, useNavigate } from "react-router-dom";
import { ReactNode } from "react";
import { useAuth } from "../auth";

const links = [
  { to: "/", label: "Messages", end: true },
  { to: "/devices", label: "Devices" },
  { to: "/webhooks", label: "Webhooks" },
  { to: "/bridge", label: "WhatsApp" },
];

export default function Layout({ children }: { children: ReactNode }) {
  const { user, logout, impersonatedBy } = useAuth();
  const navigate = useNavigate();

  return (
    <div className="min-h-full">
      {impersonatedBy !== null && (
        // Non-dismissable impersonation banner (non-negotiable #20).
        <div className="bg-amber-500 px-4 py-2 text-center text-sm font-semibold text-amber-950">
          ⚠️ You are impersonating <strong>{user?.email}</strong> (admin #{impersonatedBy}). All actions are audited.
        </div>
      )}
      <header className="border-b border-slate-200 bg-white">
        <div className="mx-auto flex max-w-6xl items-center justify-between px-4 py-3">
          <div className="flex items-center gap-6">
            <span className="text-lg font-bold">📱 SMS Gateway</span>
            <nav className="flex gap-1">
              {links.map((l) => (
                <NavLink
                  key={l.to}
                  to={l.to}
                  end={l.end}
                  className={({ isActive }) =>
                    `rounded-md px-3 py-1.5 text-sm font-medium ${
                      isActive ? "bg-sky-100 text-sky-700" : "text-slate-600 hover:bg-slate-100"
                    }`
                  }
                >
                  {l.label}
                </NavLink>
              ))}
            </nav>
          </div>
          <div className="flex items-center gap-3 text-sm">
            <span className="text-slate-500">
              {user?.email} · <span className="uppercase">{user?.tier}</span>
            </span>
            <button
              onClick={() => { logout(); navigate("/login"); }}
              className="rounded-md bg-slate-100 px-3 py-1.5 font-medium text-slate-700 hover:bg-slate-200"
            >
              Logout
            </button>
          </div>
        </div>
      </header>
      <main className="mx-auto max-w-6xl px-4 py-6">{children}</main>
    </div>
  );
}
