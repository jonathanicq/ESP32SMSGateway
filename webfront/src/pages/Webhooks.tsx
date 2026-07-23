import { FormEvent, useEffect, useState } from "react";
import { api, Webhook } from "../api";

const EVENTS = ["sms:received", "sms:sent", "sms:delivered", "sms:failed"];

export default function Webhooks() {
  const [hooks, setHooks] = useState<Webhook[]>([]);
  const [url, setUrl] = useState("");
  const [event, setEvent] = useState(EVENTS[0]);
  const [error, setError] = useState("");

  const load = () => api.listWebhooks().then(setHooks).catch((e) => setError(e.message));
  useEffect(() => { void load(); }, []);

  const create = async (e: FormEvent) => {
    e.preventDefault();
    setError("");
    try { await api.createWebhook(url, event); setUrl(""); await load(); }
    catch (err) { setError(err instanceof Error ? err.message : "Failed"); }
  };

  const remove = async (id: number) => { await api.deleteWebhook(id); await load(); };

  return (
    <div className="space-y-4">
      <h1 className="text-xl font-bold">Webhooks</h1>

      <form onSubmit={create} className="flex flex-wrap gap-2 rounded-xl border border-slate-200 bg-white p-4">
        <input required type="url" placeholder="https://example.com/hook" value={url} onChange={(e) => setUrl(e.target.value)}
          className="flex-1 min-w-[220px] rounded-lg border border-slate-300 px-3 py-2 text-sm" />
        <select value={event} onChange={(e) => setEvent(e.target.value)}
          className="rounded-lg border border-slate-300 px-3 py-2 text-sm">
          {EVENTS.map((ev) => <option key={ev} value={ev}>{ev}</option>)}
        </select>
        <button className="rounded-lg bg-sky-600 px-4 py-2 text-sm font-semibold text-white hover:bg-sky-700">Add webhook</button>
      </form>

      {error && <div className="rounded-lg bg-red-50 px-3 py-2 text-sm text-red-700">{error}</div>}

      <div className="overflow-hidden rounded-xl border border-slate-200 bg-white">
        <table className="w-full text-sm">
          <thead className="bg-slate-50 text-left text-xs uppercase text-slate-500">
            <tr><th className="px-4 py-2">Event</th><th className="px-4 py-2">URL</th><th className="px-4 py-2"></th></tr>
          </thead>
          <tbody className="divide-y divide-slate-100">
            {hooks.map((h) => (
              <tr key={h.id} className="hover:bg-slate-50">
                <td className="px-4 py-2"><span className="rounded-full bg-slate-100 px-2 py-0.5 text-xs font-medium">{h.event}</span></td>
                <td className="px-4 py-2 font-mono text-xs">{h.url}</td>
                <td className="px-4 py-2 text-right"><button onClick={() => remove(h.id)} className="text-red-600 hover:underline">Delete</button></td>
              </tr>
            ))}
            {hooks.length === 0 && <tr><td colSpan={3} className="px-4 py-8 text-center text-slate-400">No webhooks</td></tr>}
          </tbody>
        </table>
      </div>
    </div>
  );
}
