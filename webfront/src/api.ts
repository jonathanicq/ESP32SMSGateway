// Minimal typed API client. All calls go to the same-origin /api (edge nginx → backend).

export interface User { id: number; email: string; role: string; tier: string; is_active: boolean; }
export interface Device { id: number; name: string; api_key: string; last_seen: string | null; created_at: string; }
export interface Message {
  id: number; device_id: number; direction: "outbound" | "inbound";
  phone_number: string; text: string;
  status: "pending" | "sent" | "delivered" | "failed" | "received";
  error: string | null; created_at: string; updated_at: string;
}
export interface Webhook { id: number; url: string; event: string; is_active: boolean; created_at: string; }

const TOKEN_KEY = "smsgw_access_token";

export const tokenStore = {
  get: () => localStorage.getItem(TOKEN_KEY),
  set: (t: string) => localStorage.setItem(TOKEN_KEY, t),
  clear: () => localStorage.removeItem(TOKEN_KEY),
};

async function request<T>(method: string, path: string, body?: unknown): Promise<T> {
  const headers: Record<string, string> = { "Content-Type": "application/json" };
  const token = tokenStore.get();
  if (token) headers.Authorization = `Bearer ${token}`;
  const res = await fetch(`/api${path}`, {
    method,
    headers,
    body: body !== undefined ? JSON.stringify(body) : undefined,
  });
  if (res.status === 401) {
    tokenStore.clear();
    if (!path.startsWith("/auth/login")) window.location.href = "/login";
  }
  if (!res.ok) {
    let detail = res.statusText;
    try { detail = (await res.json()).detail ?? detail; } catch { /* ignore */ }
    throw new Error(typeof detail === "string" ? detail : "Request failed");
  }
  if (res.status === 204) return undefined as T;
  return res.json() as Promise<T>;
}

export const api = {
  login: (email: string, password: string) =>
    request<{ access_token: string; refresh_token: string }>("POST", "/auth/login", { email, password }),
  register: (email: string, password: string) =>
    request<User>("POST", "/auth/register", { email, password }),
  me: () => request<User>("GET", "/auth/me"),

  listDevices: () => request<Device[]>("GET", "/devices"),
  createDevice: (name: string) => request<Device>("POST", "/devices", { name }),
  claimDevice: (claim_code: string, name?: string) =>
    request<Device>("POST", "/devices/claim", { claim_code, name: name || null }),
  deleteDevice: (id: number) => request<void>("DELETE", `/devices/${id}`),

  listMessages: (params: Record<string, string>) => {
    const qs = new URLSearchParams(params).toString();
    return request<Message[]>("GET", `/messages${qs ? `?${qs}` : ""}`);
  },
  sendMessage: (device_id: number, phone_number: string, text: string) =>
    request<Message>("POST", "/messages", { device_id, phone_number, text }),

  listWebhooks: () => request<Webhook[]>("GET", "/webhooks"),
  createWebhook: (url: string, event: string) => request<Webhook>("POST", "/webhooks", { url, event }),
  deleteWebhook: (id: number) => request<void>("DELETE", `/webhooks/${id}`),
};
