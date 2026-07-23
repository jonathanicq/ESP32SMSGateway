import { FormEvent, useEffect, useState } from "react";
import { api, Device } from "../api";

interface BridgeSettings {
  id: number; enabled: boolean; waha_session: string;
  forward_to_chat: string | null; send_device_id: number | null; created_at: string;
}
interface BridgeStatus { waha_configured: boolean; trigger_keyword: string; premium: boolean; }

export default function Bridge() {
  const [status, setStatus] = useState<BridgeStatus | null>(null);
  const [bridge, setBridge] = useState<BridgeSettings | null>(null);
  const [devices, setDevices] = useState<Device[]>([]);
  const [error, setError] = useState("");
  const [saved, setSaved] = useState(false);
  const [locked, setLocked] = useState(false); // 402 → not premium

  useEffect(() => {
    void (async () => {
      try {
        const st = await fetchJson<BridgeStatus>("/whatsapp/status");
        setStatus(st);
        setDevices(await api.listDevices());
        const b = await fetchJson<BridgeSettings>("/whatsapp/bridge");
        setBridge(b);
      } catch (e) {
        if (e instanceof Error && e.message.toLowerCase().includes("premium")) setLocked(true);
        else setError(e instanceof Error ? e.message : "Failed");
      }
    })();
  }, []);

  const save = async (e: FormEvent) => {
    e.preventDefault();
    if (!bridge) return;
    setError(""); setSaved(false);
    try {
      const updated = await putJson<BridgeSettings>("/whatsapp/bridge", {
        enabled: bridge.enabled,
        waha_session: bridge.waha_session,
        forward_to_chat: bridge.forward_to_chat || null,
        send_device_id: bridge.send_device_id,
      });
      setBridge(updated);
      setSaved(true);
    } catch (err) { setError(err instanceof Error ? err.message : "Save failed"); }
  };

  if (locked) {
    return (
      <div className="rounded-xl border border-amber-200 bg-amber-50 p-6">
        <h1 className="text-xl font-bold">WhatsApp Bridge</h1>
        <p className="mt-2 text-amber-800">This is a <strong>Premium</strong> feature. Upgrade to forward SMS to WhatsApp and send SMS from WhatsApp.</p>
      </div>
    );
  }

  return (
    <div className="space-y-4">
      <h1 className="text-xl font-bold">WhatsApp Bridge <span className="rounded-full bg-violet-100 px-2 py-0.5 text-xs font-semibold text-violet-700">Premium</span></h1>

      {status && (
        <div className="rounded-xl border border-slate-200 bg-white p-4 text-sm">
          <p>WAHA backend: {status.waha_configured
            ? <span className="font-semibold text-emerald-600">configured</span>
            : <span className="font-semibold text-red-600">not configured (server-side)</span>}</p>
          <p className="text-slate-500">WhatsApp→SMS command: <code className="rounded bg-slate-100 px-1">{status.trigger_keyword} +E164 your message</code></p>
        </div>
      )}

      {error && <div className="rounded-lg bg-red-50 px-3 py-2 text-sm text-red-700">{error}</div>}
      {saved && <div className="rounded-lg bg-emerald-50 px-3 py-2 text-sm text-emerald-700">Saved.</div>}

      {bridge && (
        <form onSubmit={save} className="space-y-4 rounded-xl border border-slate-200 bg-white p-4">
          <label className="flex items-center gap-2">
            <input type="checkbox" checked={bridge.enabled}
              onChange={(e) => setBridge({ ...bridge, enabled: e.target.checked })} />
            <span className="font-medium">Bridge enabled</span>
          </label>

          <div>
            <label className="mb-1 block text-sm font-medium">WAHA session</label>
            <input value={bridge.waha_session} onChange={(e) => setBridge({ ...bridge, waha_session: e.target.value })}
              className="w-full rounded-lg border border-slate-300 px-3 py-2 text-sm" />
          </div>

          <div>
            <label className="mb-1 block text-sm font-medium">Forward inbound SMS to WhatsApp chat</label>
            <input value={bridge.forward_to_chat ?? ""} placeholder="447700900111@c.us"
              onChange={(e) => setBridge({ ...bridge, forward_to_chat: e.target.value })}
              className="w-full rounded-lg border border-slate-300 px-3 py-2 font-mono text-sm" />
          </div>

          <div>
            <label className="mb-1 block text-sm font-medium">Device for WhatsApp→SMS sends</label>
            <select value={bridge.send_device_id ?? ""}
              onChange={(e) => setBridge({ ...bridge, send_device_id: e.target.value ? Number(e.target.value) : null })}
              className="w-full rounded-lg border border-slate-300 px-3 py-2 text-sm">
              <option value="">None</option>
              {devices.map((d) => <option key={d.id} value={d.id}>{d.name}</option>)}
            </select>
          </div>

          <button className="rounded-lg bg-sky-600 px-4 py-2 text-sm font-semibold text-white hover:bg-sky-700">Save</button>
        </form>
      )}
    </div>
  );
}

// Small fetch helpers (the typed api client doesn't cover the bridge endpoints).
async function fetchJson<T>(path: string): Promise<T> { return req<T>("GET", path); }
async function putJson<T>(path: string, body: unknown): Promise<T> { return req<T>("PUT", path, body); }
async function req<T>(method: string, path: string, body?: unknown): Promise<T> {
  const token = localStorage.getItem("smsgw_access_token");
  const res = await fetch(`/api${path}`, {
    method,
    headers: { "Content-Type": "application/json", ...(token ? { Authorization: `Bearer ${token}` } : {}) },
    body: body !== undefined ? JSON.stringify(body) : undefined,
  });
  if (!res.ok) {
    let detail = res.statusText;
    try { detail = (await res.json()).detail ?? detail; } catch { /* ignore */ }
    throw new Error(typeof detail === "string" ? detail : "Request failed");
  }
  return res.json() as Promise<T>;
}
