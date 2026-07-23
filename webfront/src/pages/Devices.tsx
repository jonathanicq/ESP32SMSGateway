import { FormEvent, useEffect, useState } from "react";
import { api, Device } from "../api";

export default function Devices() {
  const [devices, setDevices] = useState<Device[]>([]);
  const [name, setName] = useState("");
  const [claimCode, setClaimCode] = useState("");
  const [claimMsg, setClaimMsg] = useState("");
  const [error, setError] = useState("");
  const [revealed, setRevealed] = useState<number | null>(null);

  const load = () => api.listDevices().then(setDevices).catch((e) => setError(e.message));
  useEffect(() => { void load(); }, []);

  const create = async (e: FormEvent) => {
    e.preventDefault();
    setError("");
    try { await api.createDevice(name); setName(""); await load(); }
    catch (err) { setError(err instanceof Error ? err.message : "Failed"); }
  };

  const claim = async (e: FormEvent) => {
    e.preventDefault();
    setError(""); setClaimMsg("");
    try {
      const d = await api.claimDevice(claimCode.trim());
      setClaimCode("");
      setClaimMsg(`Paired "${d.name}" — the phone will connect automatically.`);
      await load();
    } catch (err) { setError(err instanceof Error ? err.message : "Pairing failed"); }
  };

  const remove = async (id: number) => {
    if (!confirm("Delete this device? Its API key stops working immediately.")) return;
    await api.deleteDevice(id);
    await load();
  };

  return (
    <div className="space-y-4">
      <h1 className="text-xl font-bold">Devices</h1>

      <form onSubmit={claim} className="rounded-xl border border-sky-200 bg-sky-50 p-4">
        <div className="mb-2 text-sm font-semibold text-sky-900">Pair a phone</div>
        <p className="mb-3 text-xs text-sky-800">Open the SMS Gateway app on the phone, tap “Pair with backoffice”, and enter the 6-character code it shows here.</p>
        <div className="flex gap-2">
          <input required placeholder="Code (e.g. 923CYF)" value={claimCode}
            onChange={(e) => setClaimCode(e.target.value.toUpperCase())}
            maxLength={6}
            className="w-40 rounded-lg border border-slate-300 px-3 py-2 font-mono uppercase tracking-widest" />
          <button className="rounded-lg bg-sky-600 px-4 py-2 text-sm font-semibold text-white hover:bg-sky-700">Pair</button>
        </div>
        {claimMsg && <div className="mt-2 text-sm text-emerald-700">{claimMsg}</div>}
      </form>

      <form onSubmit={create} className="flex gap-2 rounded-xl border border-slate-200 bg-white p-4">
        <input required placeholder="Or add a device manually (name)" value={name} onChange={(e) => setName(e.target.value)}
          className="flex-1 rounded-lg border border-slate-300 px-3 py-2 text-sm" />
        <button className="rounded-lg bg-slate-700 px-4 py-2 text-sm font-semibold text-white hover:bg-slate-800">Add device</button>
      </form>

      {error && <div className="rounded-lg bg-red-50 px-3 py-2 text-sm text-red-700">{error}</div>}

      <div className="overflow-hidden rounded-xl border border-slate-200 bg-white">
        <table className="w-full text-sm">
          <thead className="bg-slate-50 text-left text-xs uppercase text-slate-500">
            <tr>
              <th className="px-4 py-2">Name</th>
              <th className="px-4 py-2">API key</th>
              <th className="px-4 py-2">Last seen</th>
              <th className="px-4 py-2"></th>
            </tr>
          </thead>
          <tbody className="divide-y divide-slate-100">
            {devices.map((d) => (
              <tr key={d.id} className="hover:bg-slate-50">
                <td className="px-4 py-2 font-medium">{d.name}</td>
                <td className="px-4 py-2 font-mono text-xs">
                  {revealed === d.id ? d.api_key : "••••••••••••"}
                  <button onClick={() => setRevealed(revealed === d.id ? null : d.id)}
                    className="ml-2 text-sky-600 hover:underline">{revealed === d.id ? "hide" : "show"}</button>
                </td>
                <td className="px-4 py-2 text-slate-500">{d.last_seen ? new Date(d.last_seen).toLocaleString() : "never"}</td>
                <td className="px-4 py-2 text-right">
                  <button onClick={() => remove(d.id)} className="text-red-600 hover:underline">Delete</button>
                </td>
              </tr>
            ))}
            {devices.length === 0 && <tr><td colSpan={4} className="px-4 py-8 text-center text-slate-400">No devices yet</td></tr>}
          </tbody>
        </table>
      </div>
    </div>
  );
}
