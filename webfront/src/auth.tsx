import { createContext, useContext, useEffect, useState, ReactNode } from "react";
import { api, tokenStore, User } from "./api";

interface JwtPayload { impersonatedBy?: number; email?: string; role?: string; }

function decodeJwt(token: string): JwtPayload {
  try {
    const payload = token.split(".")[1];
    return JSON.parse(atob(payload.replace(/-/g, "+").replace(/_/g, "/")));
  } catch {
    return {};
  }
}

interface AuthCtx {
  user: User | null;
  loading: boolean;
  impersonatedBy: number | null;
  login: (email: string, password: string) => Promise<void>;
  logout: () => void;
}

const Ctx = createContext<AuthCtx>(null!);

export function AuthProvider({ children }: { children: ReactNode }) {
  const [user, setUser] = useState<User | null>(null);
  const [loading, setLoading] = useState(true);
  const [impersonatedBy, setImpersonatedBy] = useState<number | null>(null);

  const refresh = async () => {
    const token = tokenStore.get();
    if (!token) { setLoading(false); return; }
    setImpersonatedBy(decodeJwt(token).impersonatedBy ?? null);
    try { setUser(await api.me()); } catch { tokenStore.clear(); }
    setLoading(false);
  };

  useEffect(() => { void refresh(); }, []);

  const login = async (email: string, password: string) => {
    const { access_token } = await api.login(email, password);
    tokenStore.set(access_token);
    setImpersonatedBy(decodeJwt(access_token).impersonatedBy ?? null);
    setUser(await api.me());
  };

  const logout = () => { tokenStore.clear(); setUser(null); setImpersonatedBy(null); };

  return <Ctx.Provider value={{ user, loading, impersonatedBy, login, logout }}>{children}</Ctx.Provider>;
}

export const useAuth = () => useContext(Ctx);
