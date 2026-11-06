import { Lightbulb } from "lucide-react"
import { useState, type FormEvent } from "react"

import { AppControls } from "@/components/app-controls"
import { Button } from "@/components/button"
import { Field, Message, PasswordInput } from "@/components/page"
import { useI18n } from "@/i18n"
import { post } from "@/lib/api"
import { deviceIdentity, signedIn } from "@/lib/auth"

const defaultPassword = "whisli_gogo" // firmware: DEFAULT_PASSWORD in src/board.h

export function LoginPage() {
  const { t, code } = useI18n()
  const [password, setPassword] = useState("")
  const [remember, setRemember] = useState(false)
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState("")
  const device = deviceIdentity()

  async function login(event: FormEvent) {
    event.preventDefault()
    setBusy(true)
    setError("")
    const result = await post("/api/v1/auth/login", { password, remember: remember ? 1 : 0 })
    setBusy(false)
    if (result.ok) signedIn()
    else setError(code("error", result.error))
  }

  return <div className="whisli-page grid min-h-screen place-items-center p-4 text-sm">
    <div className="fixed top-3 right-4 flex items-center gap-2"><AppControls /></div>
    <form onSubmit={login} className="whisli-card relative grid w-full max-w-sm gap-4 p-6">
      <div>
        <h1 className="text-xl font-semibold">whisli bean</h1>
        <p className="whisli-muted mt-1">{t("login.description")}</p>
      </div>
      <Field label={t("login.password")}>
        <PasswordInput autoFocus autoComplete="current-password" value={password} onChange={event => setPassword(event.target.value)} />
      </Field>
      <label className="flex items-center gap-2"><input className="whisli-check" type="checkbox" checked={remember} onChange={event => setRemember(event.target.checked)} />{t("login.remember")}</label>
      <Button type="submit" disabled={busy || !password} className="justify-center">{t("login.submit")}</Button>
      {/* Below the card, so an error does not change its size */}
      <div className="absolute inset-x-0 top-full text-center"><Message className="text-base font-medium" message={error ? { ok: false, text: error } : null} /></div>
    </form>
    {/* Which device this is and the password hint, as quiet info at the bottom of the screen */}
    <footer className="whisli-muted fixed inset-x-0 bottom-0 grid gap-1 p-4 text-center text-xs">
      {device && <p>
        {t("login.device")}: <span className="text-[var(--ink)]">{device.name}</span>
        <span className="font-mono"> · {device.ip}{device.hostname && ` · ${device.hostname}.local`}</span>
      </p>}
      <p>{t("login.defaultPassword")} <span className="font-mono text-[var(--ink)]">{defaultPassword}</span></p>
      <p className="inline-flex items-center justify-center gap-1.5"><Lightbulb className="size-3.5 shrink-0" />{t("login.resetHint")}</p>
    </footer>
  </div>
}
