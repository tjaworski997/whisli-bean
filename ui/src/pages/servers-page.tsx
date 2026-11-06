import { ChevronDown, ChevronRight, Plus, Power, Trash2 } from "lucide-react"
import { useEffect, useState } from "react"

import { Button } from "@/components/button"
import { MessageBox } from "@/components/message-box"
import { Field, Message, Muted, PageHeader, type Note } from "@/components/page"
import { useI18n } from "@/i18n"
import { countUses } from "@/lib/actions"
import { getJson, post } from "@/lib/api"
import { uniqueName } from "@/lib/format"
import type { Button as SavedButton, Server } from "@/types"

type ServersData = { servers: Server[]; limit: number }

/** Computers that buttons can wake over the network. Defined once, used by any number of buttons. */
export function ServersPage() {
  const { t, code } = useI18n()
  const [data, setData] = useState<ServersData | null>(null)
  const [usage, setUsage] = useState<Record<number, number>>({})
  const [message, setMessage] = useState<Note>(null)
  const [open, setOpen] = useState(0)
  const [removing, setRemoving] = useState<Server | null>(null)
  const [busy, setBusy] = useState(false)

  async function refresh() {
    const [servers, buttons] = await Promise.all([
      getJson<ServersData>("/api/v1/servers"),
      getJson<{ buttons: SavedButton[] }>("/api/v1/buttons"),
    ]).catch(() => [null, null] as const)
    if (!servers || !buttons) return setMessage({ ok: false, text: code("error", "offline") })
    setData(servers)
    setUsage(countUses(buttons.buttons, "wol"))
  }
  useEffect(() => { void refresh() }, [])

  async function add() {
    const name = uniqueName(t("servers.defaultName"), (data?.servers ?? []).map(server => server.name))
    setBusy(true)
    const result = await post("/api/v1/servers", { name })
    setBusy(false)
    if (!result.ok) return setMessage({ ok: false, text: code("error", result.error) })
    setMessage(null)
    await refresh()
    setOpen(Number(result.data.id) || 0)
  }

  async function remove() {
    if (!removing) return
    setBusy(true)
    const result = await post("/api/v1/servers/remove", { id: removing.id })
    setBusy(false)
    setRemoving(null)
    setMessage(result.ok ? null : { ok: false, text: code("error", result.error) })
    await refresh()
  }

  const full = Boolean(data && data.servers.length >= data.limit)
  return <>
    <PageHeader title={t("nav.servers")} description={t("servers.description")} />
    <div className="mt-6"><Button className="whisli-button-primary" disabled={busy || !data || full} onClick={() => void add()}><Plus className="size-4" />{t("servers.add")}</Button></div>
    <Message message={message} />
    {!data ? <Muted className="mt-4">{t("app.connecting")}</Muted>
      : !data.servers.length ? <Muted className="mt-4">{t("servers.none")}</Muted>
      : <div className="whisli-card mt-4 divide-y divide-[var(--line)]">
        {data.servers.map(server => <ServerRow key={server.id} server={server} uses={usage[server.id] ?? 0} expanded={open === server.id}
          onToggle={() => setOpen(open === server.id ? 0 : server.id)} onChanged={() => void refresh()} onRemove={() => setRemoving(server)} />)}
      </div>}
    <MessageBox open={Boolean(removing)} danger busy={busy} title={t("servers.removeTitle")}
      message={removing && usage[removing.id] ? t("servers.removeUsed", { name: removing.name, count: usage[removing.id] }) : t("servers.removeMessage", { name: removing?.name ?? "" })}
      confirmLabel={t("buttons.delete")} cancelLabel={t("common.cancel")} onClose={() => setRemoving(null)} onConfirm={() => void remove()} />
  </>
}

function ServerRow({ server, uses, expanded, onToggle, onChanged, onRemove }: { server: Server; uses: number; expanded: boolean; onToggle: () => void; onChanged: () => void; onRemove: () => void }) {
  const { t, code } = useI18n()
  const [draft, setDraft] = useState(server)
  const [message, setMessage] = useState<Note>(null)
  const [waking, setWaking] = useState(false)
  useEffect(() => setDraft(server), [server])

  async function save(next: Server) {
    if (JSON.stringify(next) === JSON.stringify(server)) return true
    if (!next.name.trim()) { setMessage({ ok: false, text: code("error", "invalidName") }); return false }
    const result = await post("/api/v1/servers/update", { ...next })
    setMessage(result.ok ? { ok: true, text: t("common.saved") } : { ok: false, text: code("error", result.error) })
    if (result.ok) onChanged()
    return result.ok
  }

  async function wake() {
    if (!await save(draft)) return
    setWaking(true)
    const result = await post("/api/v1/servers/wake", { id: server.id })
    setWaking(false)
    if (!result.ok) return setMessage({ ok: false, text: code("error", result.error) })
    const sent = Boolean(result.data.sent)
    setMessage(sent ? { ok: true, text: t("servers.sent", { address: String(result.data.detail) }) } : { ok: false, text: String(result.data.detail) })
  }

  const commit = () => void save(draft)
  return <div>
    <div className="flex items-center gap-3 px-4 py-3">
      <button type="button" className="whisli-link shrink-0" aria-expanded={expanded} aria-label={server.name} onClick={onToggle}>
        {expanded ? <ChevronDown className="size-4" /> : <ChevronRight className="size-4" />}
      </button>
      <Power className="whisli-muted hidden size-5 shrink-0 sm:block" aria-hidden />
      <button type="button" className="min-w-0 flex-1 text-left" onClick={onToggle}>
        <span className="block truncate font-medium">{server.name}</span>
        <span className="whisli-muted block truncate font-mono text-xs">{server.mac || t("servers.noMac")}{server.broadcast && ` → ${server.broadcast}`}</span>
      </button>
      <span className="whisli-muted hidden shrink-0 text-xs sm:inline">{uses ? t("hooks.usedBy", { count: uses }) : t("hooks.unused")}</span>
      <Button disabled={waking || !draft.mac} onClick={() => void wake()}><Power className="size-3.5" /><span className="hidden sm:inline">{t("servers.wake")}</span></Button>
      <Button variant="ghost" className="w-9 justify-center px-0" aria-label={`${t("buttons.delete")}: ${server.name}`} title={t("buttons.delete")} onClick={onRemove}><Trash2 className="size-4" /></Button>
    </div>
    {expanded && <div className="whisli-line grid gap-4 border-t px-4 py-4 sm:grid-cols-3">
      <Field label={t("hooks.name")}><input className="whisli-input" maxLength={32} value={draft.name} onChange={event => setDraft({ ...draft, name: event.target.value })} onBlur={commit} /></Field>
      <Field label={t("servers.mac")}><input className="whisli-input font-mono text-xs" maxLength={17} placeholder="AA:BB:CC:DD:EE:FF" spellCheck={false} value={draft.mac} onChange={event => setDraft({ ...draft, mac: event.target.value })} onBlur={commit} /></Field>
      <Field label={t("servers.broadcast")} hint={t("servers.broadcastHint")}><input className="whisli-input font-mono text-xs" maxLength={15} placeholder="192.168.1.255" spellCheck={false} value={draft.broadcast} onChange={event => setDraft({ ...draft, broadcast: event.target.value })} onBlur={commit} /></Field>
    </div>}
    {message && <div className="px-4 pb-3"><Message message={message} className="mt-0" /></div>}
  </div>
}
