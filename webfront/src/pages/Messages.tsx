import { FormEvent, useEffect, useState } from "react";
import { api, Device, Message } from "../api";

const STATUS_COLORS: Record<string, string> = {
  pending: "bg-amber-100 text-amber-800",
  sent: "bg-sky-100 text-sky-800",
  delivered: "bg-emerald-100 text-emerald-800",
  failed: "bg-red-100 text-red-800",
  received: "bg-violet-100 text-violet-800",
};

export default function Messages() {
  const [messages, setMessages] = useState<Message[]>([]);
  const [devices, setDevices] = useState<Device[]>([]);
  const [filters, setFilters] = useState({ device_id: "", status: "", direction: "" });
  const [search, setSearch] = useState("");
  const [error, setError] = useState("");
  const [showSend, setShowSend] = useState(false);

  const load = async () => {
    setError("");
    const params: Record<string, string> = {};
    if (filters.device_id) params.device_id = filters.device_id;
    if (filters.status) params.status = filters.status;
    if (filters.direction) params.direction = filters.direction;
    try {
      setMessages(await api.listMessages(params));
    } catch (e) { setError(e instanceof Error ? e.message : "Failed to load"); }
  };

  useEffect(() => { void api.listDevices().then(setDevices).catch(() => {}); }, []);
  useEffect(() => { void load(); /* eslint-disable-next-line */ }, [filters]);

  const visible = messages.filter(
    (m) => !search ||
      m.phone_number.includes(search) ||
      m.text.toLowerCase().includes(search.toLowerCase())
  );

  return (
    <div className="space-y-4">
      <div className="flex items-center justify-between">
        <h1 className="text-xl font-bold">Messages</h1>
        <button onClick={() => setShowSend((s) => !s)} className="rounded-lg bg-sky-600 px-4 py-2 text-sm font-semibold text-white hover:bg-sky-700">
          {showSend ? "Close" : "Send SMS"}
        </button>
      </div>

      {showSend && <SendForm devices={devices} onSent={() => { setShowSend(false); void load(); }} />}

      <div className="flex flex-wrap gap-2">
        <input
          placeholder="Search number or text…" value={search}
          onChange={(e) => setSearch(e.target.value)}
          className="flex-1 min-w-[180px] rounded-lg border border-slate-300 px-3 py-1.5 text-sm"
        />
        <select value={filters.device_id} onChange={(e) => setFilters((f) => ({ ...f, device_id: e.target.value }))}
          className="rounded-lg border border-slate-300 px-3 py-1.5 text-sm">
          <option value="">All devices</option>
          {devices.map((d) => <option key={d.id} value={d.id}>{d.name}</option>)}
        </select>
        <select value={filters.direction} onChange={(e) => setFilters((f) => ({ ...f, direction: e.target.value }))}
          className="rounded-lg border border-slate-300 px-3 py-1.5 text-sm">
          <option value="">Any direction</option>
          <option value="outbound">Outbound</option>
          <option value="inbound">Inbound</option>
        </select>
        <select value={filters.status} onChange={(e) => setFilters((f) => ({ ...f, status: e.target.value }))}
          className="rounded-lg border border-slate-300 px-3 py-1.5 text-sm">
          <option value="">Any status</option>
          {["pending", "sent", "delivered", "failed", "received"].map((s) => <option key={s} value={s}>{s}</option>)}
        </select>
      </div>

      {error && <div className="rounded-lg bg-red-50 px-3 py-2 text-sm text-red-700">{error}</div>}

      <div className="overflow-hidden rounded-xl border border-slate-200 bg-white">
        <table className="w-full text-sm">
          <thead className="bg-slate-50 text-left text-xs uppercase text-slate-500">
            <tr>
              <th className="px-4 py-2">When</th>
              <th className="px-4 py-2">Dir</th>
              <th className="px-4 py-2">Number</th>
              <th className="px-4 py-2">Text</th>
              <th className="px-4 py-2">Status</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-slate-100">
            {visible.map((m) => (
              <tr key={m.id} className="hover:bg-slate-50">
                <td className="whitespace-nowrap px-4 py-2 text-slate-500">{new Date(m.created_at).toLocaleString()}</td>
                <td className="px-4 py-2">{m.direction === "inbound" ? "⬇︎ in" : "⬆︎ out"}</td>
                <td className="whitespace-nowrap px-4 py-2 font-mono">{m.phone_number}</td>
                <td className="max-w-md truncate px-4 py-2">{m.text}</td>
                <td className="px-4 py-2">
                  <span className={`rounded-full px-2 py-0.5 text-xs font-medium ${STATUS_COLORS[m.status]}`}>{m.status}</span>
                  {m.error && <span className="ml-2 text-xs text-red-500">{m.error}</span>}
                </td>
              </tr>
            ))}
            {visible.length === 0 && (
              <tr><td colSpan={5} className="px-4 py-8 text-center text-slate-400">No messages</td></tr>
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
}

function SendForm({ devices, onSent }: { devices: Device[]; onSent: () => void }) {
  const [deviceId, setDeviceId] = useState("");
  const [number, setNumber] = useState("");
  const [text, setText] = useState("");
  const [error, setError] = useState("");

  const submit = async (e: FormEvent) => {
    e.preventDefault();
    setError("");
    try {
      await api.sendMessage(Number(deviceId), number, text);
      setNumber(""); setText("");
      onSent();
    } catch (err) { setError(err instanceof Error ? err.message : "Send failed"); }
  };

  return (
    <form onSubmit={submit} className="flex flex-wrap items-end gap-2 rounded-xl border border-slate-200 bg-white p-4">
      <select required value={deviceId} onChange={(e) => setDeviceId(e.target.value)}
        className="rounded-lg border border-slate-300 px-3 py-2 text-sm">
        <option value="">Device…</option>
        {devices.map((d) => <option key={d.id} value={d.id}>{d.name}</option>)}
      </select>
      <input required placeholder="+447700900123" value={number} onChange={(e) => setNumber(e.target.value)}
        className="rounded-lg border border-slate-300 px-3 py-2 font-mono text-sm" />
      <input required placeholder="Message text" value={text} onChange={(e) => setText(e.target.value)}
        className="flex-1 min-w-[200px] rounded-lg border border-slate-300 px-3 py-2 text-sm" />
      <button className="rounded-lg bg-sky-600 px-4 py-2 text-sm font-semibold text-white hover:bg-sky-700">Queue</button>
      {error && <span className="text-sm text-red-600">{error}</span>}
    </form>
  );
}
