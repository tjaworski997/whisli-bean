import { ChevronDown, ChevronRight, Play, Plus, Trash2, Webhook as WebhookIcon } from "lucide-react"
import { useEffect, useRef, useState } from "react"

import { Button } from "@/components/button"
import { MessageBox } from "@/components/message-box"
import { Field, Message, Muted, PageHeader, type Note } from "@/components/page"
import { useI18n } from "@/i18n"
import { countUses } from "@/lib/actions"
import { getJson, post } from "@/lib/api"
import { uniqueName } from "@/lib/format"
import { cn } from "@/lib/utils"
import type { Button as SavedButton, HookBody, HookLimits, HookMethod, HookSummary, HookTest, Webhook } from "@/types"

type HooksData = { hooks: HookSummary[]; limits: HookLimits }

const methods: HookMethod[] = ["POST", "GET", "PUT", "DELETE"]
const bodyKinds: HookBody[] = ["json", "form", "text", "none"]
const variables = ["button", "code", "gesture", "controller", "ip", "time", "uptime"] as const
const testPollMs = 400
const sleep = (ms: number) => new Promise(resolve => window.setTimeout(resolve, ms))

/** Example values for the preview; the device fills the real ones when it sends. */
function sampleValues(): Record<(typeof variables)[number], string> {
  return { button: "Play", code: "NEC-4BBA-E0", gesture: "short", controller: "Whisli Bean", ip: window.location.hostname, time: new Date().toISOString().replace(/\.\d+Z$/, "Z"), uptime: "3600" }
}

/** The same substitution as the device: values encoded for where they land. */
function expand(text: string, body: HookBody) {
  const values = sampleValues() as Record<string, string>
  return text.replace(/\{(\w+)\}/g, (whole, name: string) => {
    const value = values[name]
    if (value === undefined) return whole
    if (body === "form") return encodeURIComponent(value)
    if (body === "json") return JSON.stringify(value).slice(1, -1)
    return value
  })
}

/** Named HTTP requests that button actions call. Defined once, used by any number of buttons. */
export function WebhooksPage() {
  const { t, code } = useI18n()
  const [data, setData] = useState<HooksData | null>(null)
  const [usage, setUsage] = useState<Record<number, number>>({})
  const [message, setMessage] = useState<Note>(null)
  const [open, setOpen] = useState(0)
  const [removing, setRemoving] = useState<HookSummary | null>(null)
  const [busy, setBusy] = useState(false)

  async function refresh() {
    const [hooks, buttons] = await Promise.all([
      getJson<HooksData>("/api/v1/hooks"),
      getJson<{ buttons: SavedButton[] }>("/api/v1/buttons"),
    ]).catch(() => [null, null] as const)
    if (!hooks || !buttons) return setMessage({ ok: false, text: code("error", "offline") })
    setData(hooks)
    setUsage(countUses(buttons.buttons, "webhook"))
  }
  useEffect(() => { void refresh() }, [])

  // A record with a default name, opened right away, as in Whisli.
  async function add() {
    const name = uniqueName(t("hooks.defaultName"), (data?.hooks ?? []).map(hook => hook.name))
    setBusy(true)
    const result = await post("/api/v1/hooks", { name })
    setBusy(false)
    if (!result.ok) return setMessage({ ok: false, text: code("error", result.error) })
    setMessage(null)
    await refresh()
    setOpen(Number(result.data.id) || 0)
  }

  async function remove() {
    if (!removing) return
    setBusy(true)
    const result = await post("/api/v1/hooks/remove", { id: removing.id })
    setBusy(false)
    setRemoving(null)
    setMessage(result.ok ? null : { ok: false, text: code("error", result.error) })
    await refresh()
  }

  const full = Boolean(data && data.hooks.length >= data.limits.count)
  return <>
    <PageHeader title={t("nav.webhooks")} description={t("hooks.description")} />
    <div className="mt-6"><Button className="whisli-button-primary" disabled={busy || !data || full} onClick={() => void add()}><Plus className="size-4" />{t("hooks.add")}</Button></div>
    <Message message={message} />
    {!data ? <Muted className="mt-4">{t("app.connecting")}</Muted>
      : !data.hooks.length ? <Muted className="mt-4">{t("hooks.none")}</Muted>
      : <div className="whisli-card mt-4 divide-y divide-[var(--line)]">
        {data.hooks.map(hook => <HookRow key={hook.id} hook={hook} limits={data.limits} uses={usage[hook.id] ?? 0} expanded={open === hook.id}
          onToggle={() => setOpen(open === hook.id ? 0 : hook.id)} onChanged={() => void refresh()} onRemove={() => setRemoving(hook)} />)}
      </div>}
    <MessageBox open={Boolean(removing)} danger busy={busy} title={t("hooks.removeTitle")}
      message={removing && usage[removing.id] ? t("hooks.removeUsed", { name: removing.name, count: usage[removing.id] }) : t("hooks.removeMessage", { name: removing?.name ?? "" })}
      confirmLabel={t("buttons.delete")} cancelLabel={t("common.cancel")} onClose={() => setRemoving(null)} onConfirm={() => void remove()} />
  </>
}

function HookRow({ hook, limits, uses, expanded, onToggle, onChanged, onRemove }: {
  hook: HookSummary
  limits: HookLimits
  uses: number
  expanded: boolean
  onToggle: () => void
  onChanged: () => void
  onRemove: () => void
}) {
  const { t, code } = useI18n()
  const [testing, setTesting] = useState(false)
  const [result, setResult] = useState<HookTest | null>(null)
  const [error, setError] = useState("")

  async function test() {
    setTesting(true)
    setResult(null)
    setError("")
    const started = await post("/api/v1/hooks/test", { id: hook.id })
    if (!started.ok) {
      setTesting(false)
      setError(code("error", started.error))
      return
    }
    for (;;) {
      await sleep(testPollMs)
      const status = await getJson<HookTest>("/api/v1/hooks/test").catch(() => null)
      if (status && !status.running) { setResult(status); break }
    }
    setTesting(false)
  }

  const good = result?.sent && result.status < 400
  return <div>
    <div className="flex items-center gap-3 px-4 py-3">
      <button type="button" className="whisli-link shrink-0" aria-expanded={expanded} aria-label={hook.name} onClick={onToggle}>
        {expanded ? <ChevronDown className="size-4" /> : <ChevronRight className="size-4" />}
      </button>
      <WebhookIcon className="whisli-muted hidden size-5 shrink-0 sm:block" aria-hidden />
      <button type="button" className="min-w-0 flex-1 text-left" onClick={onToggle}>
        <span className="block truncate font-medium">{hook.name}</span>
        <span className="whisli-muted block truncate font-mono text-xs">
          <span className="whisli-line mr-2 rounded border px-1 py-px text-[10px]">{hook.method}</span>{hook.url || t("hooks.noUrl")}
        </span>
      </button>
      <span className="whisli-muted hidden shrink-0 text-xs sm:inline">{uses ? t("hooks.usedBy", { count: uses }) : t("hooks.unused")}</span>
      <Button disabled={testing || !hook.url} onClick={() => void test()}><Play className="size-3.5" /><span className="hidden sm:inline">{t(testing ? "hooks.testing" : "actions.test")}</span></Button>
      <Button variant="ghost" className="w-9 justify-center px-0" aria-label={`${t("buttons.delete")}: ${hook.name}`} title={t("buttons.delete")} onClick={onRemove}><Trash2 className="size-4" /></Button>
    </div>
    {error && <p role="alert" className="whisli-status-error px-4 pb-3 text-xs">{error}</p>}
    {result && <div className="whisli-raised mx-4 mb-3 rounded-lg px-3 py-2 text-xs">
      <p className={cn("break-words font-medium", good ? "whisli-status-ok" : "whisli-status-error")}>{result.sent ? t("hooks.testStatus", { status: result.status }) : result.error}</p>
      {result.response && <pre className="whisli-muted mt-1 max-h-32 overflow-auto whitespace-pre-wrap break-all font-mono text-[11px]">{result.response}</pre>}
    </div>}
    {expanded && <HookForm id={hook.id} limits={limits} onSaved={onChanged} />}
  </div>
}

/** Fetched when opened, because a list of full requests would not fit in the device's memory. Saves on leaving a field. */
function HookForm({ id, limits, onSaved }: { id: number; limits: HookLimits; onSaved: () => void }) {
  const { t, code } = useI18n()
  const [draft, setDraft] = useState<Webhook | null>(null)
  const [saved, setSaved] = useState<Webhook | null>(null)
  const [message, setMessage] = useState<Note>(null)
  const bodyRef = useRef<HTMLTextAreaElement>(null)

  useEffect(() => {
    void getJson<Webhook>(`/api/v1/hooks/detail?id=${id}`).then(hook => { setDraft(hook); setSaved(hook) }).catch(() => setMessage({ ok: false, text: code("error", "offline") }))
  }, [id])

  if (!draft) return message ? <div className="px-4 pb-4"><Message message={message} /></div> : <Muted className="px-4 pb-4">{t("app.connecting")}</Muted>

  async function save(next: Webhook) {
    if (JSON.stringify(next) === JSON.stringify(saved)) return
    if (!next.name.trim()) return setMessage({ ok: false, text: code("error", "invalidName") })
    const result = await post("/api/v1/hooks/update", { ...next, insecure: next.insecure ? 1 : 0 })
    setMessage(result.ok ? { ok: true, text: t("common.saved") } : { ok: false, text: code("error", result.error) })
    if (!result.ok) return
    setSaved(next)
    onSaved()
  }

  const set = (fields: Partial<Webhook>) => setDraft({ ...draft, ...fields })
  const setAndSave = (fields: Partial<Webhook>) => { const next = { ...draft, ...fields }; setDraft(next); void save(next) }
  const commit = () => void save(draft)

  // Puts {name} where the cursor is in the body.
  function insertVariable(name: string) {
    const field = bodyRef.current
    const token = `{${name}}`
    const current = draft!.bodyTemplate
    if (!field) return setAndSave({ bodyTemplate: current + token })
    const start = field.selectionStart, end = field.selectionEnd
    setAndSave({ bodyTemplate: current.slice(0, start) + token + current.slice(end) })
    requestAnimationFrame(() => { field.focus(); field.setSelectionRange(start + token.length, start + token.length) })
  }

  const hasBody = draft.method !== "GET" && draft.body !== "none"
  return <div className="whisli-line grid gap-4 border-t px-4 py-4">
    <Field label={t("hooks.name")} className="max-w-md">
      <input className="whisli-input" maxLength={limits.name} value={draft.name} onChange={event => set({ name: event.target.value })} onBlur={commit} />
    </Field>
    <div className="grid gap-4 sm:grid-cols-[8rem_minmax(0,1fr)]">
      <Field label={t("actions.method")}>
        <select className="whisli-select" value={draft.method} onChange={event => setAndSave({ method: event.target.value as HookMethod })}>
          {methods.map(method => <option key={method}>{method}</option>)}
        </select>
      </Field>
      <Field label={t("actions.url")} hint={t("hooks.urlHint")}>
        <input className="whisli-input font-mono text-xs" maxLength={limits.url} placeholder="https://…/api/webhook/{gesture}" spellCheck={false} value={draft.url} onChange={event => set({ url: event.target.value })} onBlur={commit} />
      </Field>
    </div>
    <Field label={t("hooks.headers")} hint={t("hooks.headersHint")}>
      <textarea className="whisli-input h-auto min-h-16 resize-y py-2 font-mono text-xs leading-5" rows={2} maxLength={limits.headers} placeholder="Authorization: Bearer …" spellCheck={false} value={draft.headers}
        onChange={event => set({ headers: event.target.value })} onBlur={commit} />
    </Field>
    <div className="grid gap-1.5">
      <span className="whisli-muted text-xs">{t("actions.body")}</span>
      <div className="flex flex-wrap gap-1.5" role="group" aria-label={t("actions.body")}>
        {bodyKinds.map(kind => <button key={kind} type="button" aria-pressed={draft.body === kind} disabled={draft.method === "GET"}
          className={cn("whisli-line rounded-full border px-3 py-1 text-xs disabled:opacity-40", draft.body === kind ? "border-[var(--ink)] bg-[var(--ink)] text-[var(--page)]" : "hover:border-[var(--ink)]")}
          onClick={() => setAndSave({ body: kind })}>{t(`hooks.body.${kind}`)}</button>)}
      </div>
      {draft.method === "GET" && <span className="whisli-muted text-xs">{t("hooks.bodyGet")}</span>}
    </div>
    {hasBody && <div className="grid gap-4 lg:grid-cols-2">
      <div className="grid content-start gap-1.5">
        <span className="whisli-muted text-xs">{t("hooks.template")}</span>
        <textarea ref={bodyRef} className="whisli-input h-auto min-h-40 resize-y py-2 font-mono text-xs leading-5" rows={8} maxLength={limits.body} spellCheck={false} placeholder={'{"button":"{button}","gesture":"{gesture}"}'} value={draft.bodyTemplate}
          onChange={event => set({ bodyTemplate: event.target.value })} onBlur={commit} />
        <div className="flex flex-wrap gap-1">
          {variables.map(name => <button key={name} type="button" title={t("hooks.insert")} className="whisli-line whisli-muted rounded border px-1.5 py-0.5 font-mono text-[11px] hover:border-[var(--ink)] hover:text-[var(--ink)]"
            onMouseDown={event => event.preventDefault()} onClick={() => insertVariable(name)}>{`{${name}}`}</button>)}
        </div>
      </div>
      <div className="grid content-start gap-1.5">
        <span className="whisli-muted text-xs">{t("hooks.preview")}</span>
        <pre className="whisli-raised min-h-40 overflow-auto whitespace-pre-wrap break-all rounded-md p-3 font-mono text-xs leading-5">{expand(draft.bodyTemplate, draft.body)}</pre>
      </div>
    </div>}
    {draft.url.startsWith("https://") && <label className="flex items-center gap-2">
      <input className="whisli-check" type="checkbox" checked={draft.insecure} onChange={event => setAndSave({ insecure: event.target.checked })} />{t("actions.insecure")}
    </label>}
    <Message message={message} className="mt-0" />
  </div>
}
