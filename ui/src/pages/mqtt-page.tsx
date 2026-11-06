import { useState } from "react"

import { Button } from "@/components/button"
import { AutoSaveInput, Field, Message, Muted, PageHeader, PasswordInput, Section, type Note } from "@/components/page"
import { useDevice } from "@/hooks/use-device"
import { useFetch } from "@/hooks/use-fetch"
import { useI18n, type TranslationKey } from "@/i18n"
import { post, type ApiParams } from "@/lib/api"
import { cn } from "@/lib/utils"
import type { MqttSettings } from "@/types"

// A made-up button; the shape must stay in step with buttonEvent() and publishTriggers() in src/mqtt.cpp.
const sample = { code: "NEC-4BBA-E0", name: "Play", gesture: "short" }

function exampleMessages(base: string, prefix: string, nodeId: string) {
  return {
    press: [
      [`${base}/button`, JSON.stringify(sample)],
      [`${base}/button/${sample.code}`, sample.gesture],
      [`${base}/event`, JSON.stringify({ event_type: sample.gesture, button: sample.name, code: sample.code })],
    ],
    trigger: [
      [`${prefix}/device_automation/${nodeId}/${sample.code}_${sample.gesture}/config`, JSON.stringify({
        automation_type: "trigger",
        type: "button_short_press",
        subtype: sample.name,
        topic: `${base}/button/${sample.code}`,
        payload: sample.gesture,
        device: { identifiers: [nodeId] },
      })],
    ],
  }
}

function Messages({ title, hint, messages }: { title: string; hint: string; messages: string[][] }) {
  return <div className="mt-4 max-w-3xl">
    <h3 className="font-medium">{title}</h3>
    <p className="whisli-muted mt-0.5 text-xs">{hint}</p>
    <div className="whisli-raised mt-2 grid gap-3 rounded-md p-3 font-mono text-xs">
      {messages.map(([topic, payload]) => <div key={topic} className="min-w-0">
        <p className="whisli-muted break-all">{topic}</p>
        <p className="mt-0.5 break-all">{payload}</p>
      </div>)}
    </div>
  </div>
}

/** Every field saves itself: text fields on leaving them, check boxes at once. */
export function MqttPage() {
  const { t, code } = useI18n()
  const { state } = useDevice()
  const { data: settings, reload } = useFetch<MqttSettings>("/api/v1/mqtt")
  const [password, setPassword] = useState("") // typed but not saved yet; the saved one never comes back
  const [message, setMessage] = useState<Note>(null)

  if (!settings) return <Muted>{t("app.connecting")}</Muted>
  const base = settings.baseTopic || settings.defaultBaseTopic
  const mqttState = state?.mqtt ?? settings.state
  const examples = exampleMessages(base, settings.discoveryPrefix, `whisli_bean_${settings.defaultBaseTopic.split("/").pop()}`)

  async function save(params: ApiParams) {
    const result = await post("/api/v1/mqtt", params)
    setMessage(result.ok ? { ok: true, text: t("common.saved") } : { ok: false, text: code("error", result.error) })
    await reload() // also undoes a refused change in the fields
  }

  async function savePassword() {
    if (!password) return
    await save({ password })
    setPassword("")
  }

  return <>
    <PageHeader eyebrow={t("nav.system")} title={t("nav.mqtt")} description={t("mqtt.description")} />
    <Section title={t("mqtt.connection")} description={t("mqtt.connectionHint")} className="mt-6">
      <p className="mt-3">
        {t("mqtt.state")}: <span className={cn(mqttState === "connected" ? "whisli-status-ok" : mqttState === "error" ? "whisli-status-error" : "whisli-muted")}>{t(`mqtt.state.${mqttState}` as TranslationKey)}</span>
        {mqttState === "error" && settings.error && <span className="whisli-status-error"> – {code("mqttError", settings.error)}</span>}
      </p>
      <div className="mt-4 grid max-w-3xl gap-4 sm:grid-cols-[1fr_8rem]">
        <label className="flex items-center gap-2 sm:col-span-2">
          <input className="whisli-check" type="checkbox" checked={settings.enabled} onChange={e => void save({ enabled: e.target.checked })} />{t("mqtt.enabled")}
        </label>
        <Field label={t("mqtt.host")}><AutoSaveInput value={settings.host} placeholder="homeassistant.local" onSave={host => void save({ host })} /></Field>
        <Field label={t("mqtt.port")}><AutoSaveInput type="number" min={1} max={65535} value={settings.port} onSave={port => void save({ port })} /></Field>
        <div className="grid gap-4 sm:col-span-2 sm:grid-cols-2">
          <Field label={t("mqtt.user")}><AutoSaveInput autoComplete="off" value={settings.user} onSave={user => void save({ user })} /></Field>
          <Field label={t("mqtt.password")} hint={settings.hasPassword &&
            <Button variant="ghost" className="-ml-2 h-7 px-2 text-xs" onClick={() => void save({ clearPassword: true })}>{t("mqtt.clearPassword")}</Button>}>
            <PasswordInput autoComplete="new-password" value={password}
              placeholder={t(settings.hasPassword ? "mqtt.passwordSet" : "mqtt.passwordNone")}
              onChange={e => setPassword(e.target.value)} onBlur={() => void savePassword()}
              onKeyDown={e => { if (e.key === "Enter") e.currentTarget.blur() }} />
          </Field>
          <Field label={t("mqtt.baseTopic")} hint={t("mqtt.baseTopicHint")}>
            <AutoSaveInput value={settings.baseTopic} placeholder={settings.defaultBaseTopic} onSave={baseTopic => void save({ baseTopic })} />
          </Field>
          <Field label={t("mqtt.discoveryPrefix")} hint={t("mqtt.discoveryPrefixHint")}>
            <AutoSaveInput value={settings.discoveryPrefix} onSave={discoveryPrefix => void save({ discoveryPrefix })} />
          </Field>
        </div>
        <label className="flex items-center gap-2 sm:col-span-2">
          <input className="whisli-check" type="checkbox" checked={settings.discovery} onChange={e => void save({ discovery: e.target.checked })} />{t("mqtt.discovery")}
        </label>
      </div>
      <Message message={message} />
    </Section>
    <Section title={t("mqtt.topics")} description={t("mqtt.topicsHint")}>
      <dl className="mt-4 grid max-w-3xl gap-x-6 gap-y-2 sm:grid-cols-[auto_1fr]">
        <dt className="font-mono text-xs">{base}/status</dt><dd className="whisli-muted">{t("mqtt.topicStatus")}</dd>
        <dt className="font-mono text-xs">{base}/state</dt><dd className="whisli-muted">{t("mqtt.topicState")}</dd>
        <dt className="font-mono text-xs">{base}/button</dt><dd className="whisli-muted">{t("mqtt.topicButton")}</dd>
        <dt className="font-mono text-xs">{base}/button/&lt;code&gt;</dt><dd className="whisli-muted">{t("mqtt.topicButtonCode")}</dd>
        <dt className="font-mono text-xs">{base}/event</dt><dd className="whisli-muted">{t("mqtt.topicEvent")}</dd>
      </dl>
    </Section>
    <Section title={t("mqtt.examples")} description={t("mqtt.examplesHint", { name: sample.name })}>
      <Messages title={t("mqtt.examplePress")} hint={t("mqtt.examplePressHint")} messages={examples.press} />
      <Messages title={t("mqtt.exampleTrigger")} hint={t("mqtt.exampleTriggerHint")} messages={examples.trigger} />
    </Section>
  </>
}
