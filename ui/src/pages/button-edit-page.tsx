import { ArrowLeft, Trash2 } from "lucide-react"
import { useEffect, useRef, useState } from "react"

import { Button } from "@/components/button"
import { CodeField, CodeHint } from "@/components/code-field"
import { GesturePanel } from "@/components/gesture-panel"
import { MessageBox } from "@/components/message-box"
import { Field, Muted, PageHeader, Section } from "@/components/page"
import { useDevice } from "@/hooks/use-device"
import { useFetch } from "@/hooks/use-fetch"
import { useRunPulse } from "@/hooks/use-run-pulse"
import { useI18n, type TranslationKey } from "@/i18n"
import { gestureNames } from "@/lib/actions"
import { post } from "@/lib/api"
import { goTo, pageHash, setLeaveGuard } from "@/lib/route"
import { cn } from "@/lib/utils"
import type { Button as SavedButton, GestureActions, GestureName, HookSummary, Server } from "@/types"

type Choices = Record<GestureName, GestureActions>

function BackLink() {
  const { t } = useI18n()
  return <a href={pageHash("buttons")} className="whisli-link mb-3 inline-flex items-center gap-1.5"><ArrowLeft className="size-4" />{t("nav.buttons")}</a>
}

/** One screen for both adding and editing; an empty code means adding. */
export function ButtonEditPage({ code }: { code: string }) {
  return code ? <ExistingButton code={code} /> : <Editor />
}

function ExistingButton({ code }: { code: string }) {
  const { t } = useI18n()
  const { data, failed } = useFetch<{ buttons: SavedButton[] }>("/api/v1/buttons")
  if (!data) return <Muted>{t(failed ? "error.offline" : "app.connecting")}</Muted>
  const button = data.buttons.find(item => item.code === code)
  if (!button) return <><BackLink /><Muted>{t("error.notFound")}</Muted></>
  return <Editor key={button.code} button={button} />
}

// A new gesture sends over MQTT unless unticked: that is what most buttons are for.
const startChoices = (saved?: GestureActions): GestureActions => saved ?? { mqtt: true }
const saveDelayMs = 700

function Editor({ button }: { button?: SavedButton }) {
  const { t, code } = useI18n()
  const { state } = useDevice()
  const { data: hookData } = useFetch<{ hooks: HookSummary[] }>("/api/v1/hooks")
  const { data: serverData } = useFetch<{ servers: Server[] }>("/api/v1/servers")
  const hooks = hookData?.hooks ?? []
  const servers = serverData?.servers ?? []
  // A new button exists from its first save on, and this screen then simply goes on editing it.
  const [createdCode, setCreatedCode] = useState("")
  const savedCode = button?.code ?? createdCode
  const adding = !savedCode
  const learn = state?.ir.learn
  const liveRun = state?.actions.run ?? null
  const pulse = useRunPulse(Boolean(state), liveRun)
  // A code that is already saved cannot be used for a new button.
  const newCode = adding && learn?.state === "learned" && !learn.name ? learn.code ?? "" : ""
  const buttonCode = savedCode || newCode
  const [name, setName] = useState(button?.name ?? "")
  const [note, setNote] = useState(button?.note ?? "")
  const [shortOn, setShortOn] = useState(button?.actions.short?.enabled ?? true)
  const [longOn, setLongOn] = useState(button?.actions.long ? button.actions.long.enabled ?? button.gestures : false)
  const [choices, setChoices] = useState<Choices>(() => ({
    short: startChoices(button?.actions.short),
    long: startChoices(button?.actions.long),
  }))
  const [dirty, setDirty] = useState(false)
  const [saving, setSaving] = useState(false)
  const [saved, setSaved] = useState(false)
  const [deleting, setDeleting] = useState(false)
  const [error, setError] = useState("")

  // What stops a save right now, as a text key; null when it can go.
  const on: Record<GestureName, boolean> = { short: shortOn, long: longOn }
  const active = gestureNames.filter(gesture => on[gesture])
  const blocker: TranslationKey | null = !buttonCode ? "edit.needCode" : !name.trim() ? "edit.needName"
    : active.some(gesture => choices[gesture].webhook === 0) ? "edit.pickHook"
    : active.some(gesture => choices[gesture].wol === 0) ? "edit.pickServer" : null

  // Timers and the leave-page check need the current values, not those of an earlier render.
  const latest = useRef({ dirty, blocker, code: buttonCode, fields: {} as Record<string, string | number> })
  latest.current = {
    dirty,
    blocker,
    code: buttonCode,
    fields: { code: buttonCode, name, note, gestures: longOn ? 1 : 0, actions: JSON.stringify({ short: { ...choices.short, enabled: shortOn }, long: { ...choices.long, enabled: longOn } }) },
  }
  const version = useRef(0)
  const saveRunning = useRef(false)

  function touch() {
    version.current += 1
    setDirty(true)
    setSaved(false)
    setError("")
  }

  function change(gesture: GestureName, next: GestureActions) {
    setChoices(current => ({ ...current, [gesture]: next }))
    touch()
  }

  // One save at a time; changes made during a save go out right after it.
  async function save() {
    if (saveRunning.current || latest.current.blocker) return
    saveRunning.current = true
    setSaving(true)
    const sentVersion = version.current
    const sentCode = latest.current.code
    const result = await post("/api/v1/buttons", latest.current.fields)
    saveRunning.current = false
    setSaving(false)
    if (!result.ok) return setError(code("error", result.error))
    setCreatedCode(current => button ? current : current || sentCode)
    if (version.current !== sentVersion) return void save()
    setDirty(false)
    setSaved(true)
  }

  // A pause after the last change, so typing a name does not write to flash for every letter.
  useEffect(() => {
    if (!dirty || blocker) return
    const timer = window.setTimeout(() => void save(), saveDelayMs)
    return () => window.clearTimeout(timer)
  }, [dirty, blocker, name, note, shortOn, longOn, choices, buttonCode])

  // Leaving right after a change still saves it; only what cannot be saved needs a question.
  useEffect(() => {
    const cannotSave = () => latest.current.dirty && Boolean(latest.current.blocker)
    setLeaveGuard(() => !cannotSave() || window.confirm(t("edit.unsavedLeave")))
    const onUnload = (event: BeforeUnloadEvent) => { if (cannotSave()) event.preventDefault() }
    window.addEventListener("beforeunload", onUnload)
    return () => {
      setLeaveGuard(null)
      window.removeEventListener("beforeunload", onUnload)
      if (latest.current.dirty && !latest.current.blocker) void post("/api/v1/buttons", latest.current.fields)
    }
  }, [])

  // Adding: the device listens as long as this screen waits for a button, and stops when it closes.
  useEffect(() => {
    if (!adding) return
    void post("/api/v1/ir/learn")
    return () => { void post("/api/v1/ir/learn/cancel") }
  }, [adding])
  // Also after a timeout, a device restart or another tab ending the session: a blinking field must mean listening.
  useEffect(() => {
    if (adding && (learn?.state === "timedOut" || learn?.state === "idle")) void post("/api/v1/ir/learn")
  }, [adding, learn?.state])

  async function remove() {
    const result = await post("/api/v1/buttons/delete", { code: savedCode })
    setDeleting(false)
    if (!result.ok) return setError(code("error", result.error))
    latest.current.dirty = false
    setDirty(false)
    goTo(pageHash("buttons"))
  }

  const panel = (gesture: GestureName) => <GesturePanel gesture={gesture} value={choices[gesture]} hooks={hooks} servers={servers}
    buttonCode={savedCode} dirty={dirty} run={liveRun?.code === savedCode && liveRun.gesture === gesture ? liveRun : undefined}
    pulseId={pulse?.code === savedCode && liveRun?.gesture === gesture ? pulse.id : 0} onChange={next => change(gesture, next)} />

  const status = error ? <span role="alert" className="whisli-status-error text-xs">{error}</span>
    : blocker && (dirty || adding) ? <span className={cn("text-xs", blocker === "edit.pickHook" || blocker === "edit.pickServer" ? "whisli-status-error" : "whisli-muted")}>{t(blocker)}</span>
    : saving || dirty ? <span role="status" className="whisli-muted text-xs">{t("common.saving")}</span>
    : saved ? <span role="status" className="whisli-status-ok text-xs">{t("common.saved")}</span>
    : null

  return <div className="max-w-3xl">
    <BackLink />
    <PageHeader title={name.trim() || t("buttons.add")} actions={status} />

    <div className="mt-6 grid gap-4 sm:grid-cols-2">
      <div className="grid content-start gap-1.5">
        <span className="whisli-muted text-xs">{t("edit.code")}</span>
        {savedCode ? <span className="whisli-input flex items-center font-mono text-xs">{savedCode}</span> : <CodeField learn={learn} />}
        {adding && <CodeHint learn={learn} />}
      </div>
      <Field label={t("buttons.name")}>
        <input className="whisli-input" autoFocus={!button} maxLength={32} placeholder={t("buttons.namePlaceholder")} autoComplete="off" value={name} onChange={event => { setName(event.target.value); touch() }} />
      </Field>
      <Field label={t("edit.note")} className="sm:col-span-2">
        <textarea className="whisli-input h-auto min-h-16 resize-y py-2 leading-5" rows={2} maxLength={200} placeholder={t("edit.notePlaceholder")} value={note} onChange={event => { setNote(event.target.value); touch() }} />
      </Field>
    </div>

    <section className="whisli-line mt-8 grid gap-6 border-t pt-6">
      <GestureSwitch title={t("edit.short")} text={t(longOn ? "edit.shortHintDelayed" : "edit.shortHint")} checked={shortOn} onChange={checked => { setShortOn(checked); touch() }}>{panel("short")}</GestureSwitch>
      <GestureSwitch title={t("edit.long")} text={t("edit.longHint")} checked={longOn} onChange={checked => { setLongOn(checked); touch() }}>
        {panel("long")}
        <label className="mt-3 grid cursor-pointer grid-cols-[auto_1fr] gap-x-2">
          <input className="whisli-check mt-0.5" type="checkbox" checked={choices.long.once ?? true} onChange={event => change("long", { ...choices.long, once: event.target.checked })} />
          <span className="font-medium">{t("edit.once")}</span>
          <span className="whisli-muted col-start-2 mt-1 text-xs">{t(choices.long.once ?? true ? "edit.onceHint" : "edit.repeatHint")}</span>
        </label>
      </GestureSwitch>
    </section>

    {savedCode && <Section title={t("edit.delete")} description={t("edit.deleteHint")}>
      <Button variant="danger" className="mt-3" onClick={() => setDeleting(true)}><Trash2 className="size-4" />{t("edit.deleteButton")}</Button>
    </Section>}

    <MessageBox open={deleting} danger title={t("buttons.deleteTitle")} message={t("buttons.deleteMessage", { name })}
      confirmLabel={t("buttons.delete")} cancelLabel={t("common.cancel")} onClose={() => setDeleting(false)} onConfirm={() => void remove()} />
  </div>
}

/** A gesture: a checkbox with a description. Unticking only hides its settings, they stay as they are. */
function GestureSwitch({ title, text, checked, onChange, children }: { title: string; text: string; checked: boolean; onChange: (checked: boolean) => void; children: React.ReactNode }) {
  return <div>
    <label className="grid cursor-pointer grid-cols-[auto_1fr] gap-x-2">
      <input className="whisli-check mt-0.5" type="checkbox" checked={checked} onChange={event => onChange(event.target.checked)} />
      <span className="font-medium">{title}</span>
      <span className="whisli-muted col-start-2 mt-1 text-xs">{text}</span>
    </label>
    {checked && <div className="mt-2 sm:ml-6">{children}</div>}
  </div>
}
