import { useEffect, useState } from "react"

import { Field, Message, Section, type Note } from "@/components/page"
import { useFetch } from "@/hooks/use-fetch"
import { useI18n, type TranslationKey } from "@/i18n"
import { post, type ApiParams } from "@/lib/api"
import { cn } from "@/lib/utils"

type LedMode = "received" | "sent" | "status" | "off"
interface LedSettings { mode: LedMode; brightness: number; alerts: boolean }

const modes: LedMode[] = ["received", "sent", "status", "off"]

/** The board's LED. Every change is saved and shown on the LED at once. */
export function LedSection() {
  const { t, code } = useI18n()
  const { data } = useFetch<LedSettings>("/api/v1/led")
  const [led, setLed] = useState<LedSettings | null>(null)
  const [message, setMessage] = useState<Note>(null)
  useEffect(() => { if (data) setLed(data) }, [data])

  async function save(params: ApiParams) {
    const result = await post("/api/v1/led", params)
    if (result.ok) {
      setLed(result.data as unknown as LedSettings)
      setMessage(null)
    } else {
      setMessage({ ok: false, text: code("error", result.error) })
    }
  }

  return <Section title={t("led.mode")} className="mt-6">
    {led && <div className="mt-4 grid max-w-3xl gap-5">
      <div className="grid gap-2 sm:grid-cols-2">
        {modes.map(mode => <button key={mode} type="button" aria-pressed={led.mode === mode} onClick={() => void save({ mode })}
          className={cn("whisli-card p-3 text-left", led.mode === mode ? "border-[var(--status-ok)]" : "hover:bg-[var(--raised)]")}>
          <p className={cn("font-medium", led.mode === mode && "whisli-status-ok")}>{t(`led.mode.${mode}` as TranslationKey)}</p>
          <p className="whisli-muted mt-0.5 text-xs">{t(`led.mode.${mode}Hint` as TranslationKey)}</p>
        </button>)}
      </div>
      <Field label={`${t("led.brightness")} · ${led.brightness} %`} className="max-w-sm">
        <input type="range" min={1} max={100} value={led.brightness} className="w-full accent-[var(--status-ok)]"
          onChange={event => setLed({ ...led, brightness: Number(event.target.value) })}
          onPointerUp={() => void save({ brightness: led.brightness })} onKeyUp={() => void save({ brightness: led.brightness })} />
      </Field>
      {(led.mode === "received" || led.mode === "sent") && <label className="flex items-start gap-2">
        <input className="whisli-check mt-0.5" type="checkbox" checked={led.alerts} onChange={event => void save({ alerts: event.target.checked })} />
        <span>{t("led.alerts")}<span className="whisli-muted block text-xs">{t("led.alertsHint")}</span></span>
      </label>}
    </div>}
    <Message message={message} />
  </Section>
}
