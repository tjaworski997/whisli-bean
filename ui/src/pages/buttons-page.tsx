import { Pencil, Plus, Trash2 } from "lucide-react"
import { useState } from "react"

import { Button } from "@/components/button"
import { MessageBox } from "@/components/message-box"
import { Message, Muted, PageHeader, type Note } from "@/components/page"
import { StepIcon } from "@/components/step-icon"
import { useDevice } from "@/hooks/use-device"
import { useFetch } from "@/hooks/use-fetch"
import { useRunPulse } from "@/hooks/use-run-pulse"
import { useI18n, type TranslationKey } from "@/i18n"
import { actionIcons, actionTypes, gestureNames } from "@/lib/actions"
import { post } from "@/lib/api"
import { ago } from "@/lib/format"
import { buttonHash, pageHash } from "@/lib/route"
import { cn } from "@/lib/utils"
import type { ActionRun, ActionType, Button as SavedButton, GestureActions, GestureName, HookSummary, Press, Server } from "@/types"

type Names = { hooks: HookSummary[]; servers: Server[] }

function usedTypes(chosen?: GestureActions) {
  if (chosen?.enabled === false) return []
  return actionTypes.filter(type => type === "mqtt" ? chosen?.mqtt : chosen?.[type])
}

function chipText(type: ActionType, chosen: GestureActions, names: Names, t: (key: TranslationKey) => string) {
  if (type === "mqtt") return "MQTT"
  const list: { id: number; name: string }[] = type === "webhook" ? names.hooks : names.servers
  return list.find(item => item.id === chosen[type])?.name ?? t("actions.missing")
}

export function ButtonsPage() {
  const { t, code } = useI18n()
  const { state } = useDevice()
  const { data, reload } = useFetch<{ buttons: SavedButton[]; runs: ActionRun[] }>("/api/v1/buttons", 5000)
  const { data: hookData } = useFetch<{ hooks: HookSummary[] }>("/api/v1/hooks")
  const { data: serverData } = useFetch<{ servers: Server[] }>("/api/v1/servers")
  const names: Names = { hooks: hookData?.hooks ?? [], servers: serverData?.servers ?? [] }
  const [deleting, setDeleting] = useState<SavedButton | null>(null)
  const [message, setMessage] = useState<Note>(null)
  const press = state?.ir.press ?? null
  const liveRun = state?.actions.run ?? null
  const pulse = useRunPulse(Boolean(state), liveRun)

  async function remove() {
    if (!deleting) return
    const result = await post("/api/v1/buttons/delete", { code: deleting.code })
    setDeleting(null)
    setMessage(result.ok ? null : { ok: false, text: code("error", result.error) })
    void reload()
  }

  if (!state) return <Muted>{t("app.connecting")}</Muted>

  // The live state carries the newest run; the list has the older ones of other buttons.
  const runOf = (buttonCode: string) => {
    const saved = data?.runs.find(run => run.code === buttonCode)
    return liveRun?.code === buttonCode && liveRun.count >= (saved?.count ?? 0) ? liveRun : saved
  }

  return <>
    <PageHeader title={t("nav.buttons")} description={t("buttons.description")} />
    <div className="mt-6 flex flex-wrap items-center gap-3">
      <a href={pageHash("buttonNew")} className="whisli-button whisli-button-primary"><Plus className="size-4" />{t("buttons.add")}</a>
      <LastPress press={press} uptimeMs={state.uptimeMs} />
    </div>
    <Message message={message} />

    {!data ? <Muted className="mt-4">{t("app.connecting")}</Muted>
      : !data.buttons.length ? <Muted className="mt-4">{t("buttons.none")}</Muted>
      : <>
        <div className="whisli-card mt-4 overflow-hidden">
          <table className="w-full table-fixed border-collapse text-left">
            <thead className="whisli-muted whisli-line border-b text-xs">
              <tr>
                <th className="w-[38%] px-4 py-2.5 font-medium md:w-56">{t("buttons.colName")}</th>
                <th className="hidden w-36 px-4 py-2.5 font-medium sm:table-cell">{t("buttons.colCode")}</th>
                <th className="hidden w-16 px-2 py-2.5 text-center font-medium sm:table-cell">{t("buttons.colCount")}</th>
                <th className="hidden px-4 py-2.5 font-medium md:table-cell">{t("buttons.colActions")}</th>
                <th className="w-24 px-2 py-2.5"><span className="sr-only">{t("buttons.edit")}</span></th>
              </tr>
            </thead>
            <tbody className="[&>tr:not(.md\:hidden)]:border-t [&>tr:not(.md\:hidden)]:border-[var(--line)]">
              {data.buttons.map(button => <Row key={button.code} button={button} names={names} run={runOf(button.code)} uptimeMs={state.uptimeMs}
                pulseId={pulse?.code === button.code ? pulse.id : 0} onDelete={() => setDeleting(button)} />)}
            </tbody>
          </table>
        </div>
        <p className="whisli-muted mt-3 text-xs">{t("buttons.legend")}</p>
      </>}

    <MessageBox open={Boolean(deleting)} danger title={t("buttons.deleteTitle")} message={t("buttons.deleteMessage", { name: deleting?.name ?? "" })}
      confirmLabel={t("buttons.delete")} cancelLabel={t("common.cancel")} onClose={() => setDeleting(null)} onConfirm={() => void remove()} />
  </>
}

function LastPress({ press, uptimeMs }: { press: Press | null; uptimeMs: number }) {
  const { t } = useI18n()
  return <p className="whisli-muted ml-auto flex min-w-0 flex-wrap items-baseline gap-x-3 text-xs">
    {!press ? <span>{t("buttons.pressHint")}</span> : <span className="min-w-0 truncate">
      {t("buttons.lastShort")}: <span key={press.count} className="font-medium text-[var(--ink)]">{press.name ?? t("buttons.unknown")}</span>
      {" · "}<span className="font-mono">{t("buttons.ago", { code: press.code, time: ago(uptimeMs, press.atMs) })}</span>
    </span>}
  </p>
}

function Row({ button, names, run, uptimeMs, pulseId, onDelete }: { button: SavedButton; names: Names; run?: ActionRun; uptimeMs: number; pulseId: number; onDelete: () => void }) {
  const { t } = useI18n()
  const gestures: GestureName[] = button.gestures ? gestureNames : ["short"]
  const count = gestures.reduce((sum, gesture) => sum + usedTypes(button.actions[gesture]).length, 0)
  const chips = <ActionChips button={button} names={names} gestures={gestures} run={run} live={pulseId !== 0} uptimeMs={uptimeMs} />

  return <><tr key={pulseId} className={cn(pulseId !== 0 && "whisli-row-pressed")}>
    <td className="px-4 py-3 align-top">
      <a href={buttonHash(button.code)} className="block truncate font-medium hover:underline"><span className={cn(pulseId !== 0 && "whisli-pop")}>{button.name}</span></a>
      {button.note && <span title={button.note} className="whisli-muted line-clamp-2 text-xs">{button.note}</span>}
      <span className="whisli-muted block truncate font-mono text-xs sm:hidden">{button.code}</span>
    </td>
    <td className="whisli-muted hidden truncate px-4 py-3 align-top font-mono text-xs leading-5 sm:table-cell">{button.code}</td>
    <td className="hidden px-2 py-3 text-center align-top tabular-nums sm:table-cell">{count || <span className="whisli-muted">—</span>}</td>
    <td className="hidden min-w-0 px-4 py-2.5 align-top md:table-cell">{chips}</td>
    <td className="px-2 py-2 align-top">
      <div className="flex justify-end">
        <a href={buttonHash(button.code)} className="whisli-button whisli-button-ghost w-9 justify-center px-0" aria-label={`${t("buttons.edit")}: ${button.name}`} title={t("buttons.edit")}><Pencil className="size-4" /></a>
        <Button variant="ghost" className="w-9 justify-center px-0" aria-label={`${t("buttons.delete")}: ${button.name}`} title={t("buttons.delete")} onClick={onDelete}><Trash2 className="size-4" /></Button>
      </div>
    </td>
  </tr>
  {/* Phones have no room for the column: the actions get their own line */}
  {count > 0 && <tr className="md:hidden"><td colSpan={5} className="px-4 pb-3">{chips}</td></tr>}
  </>
}

/** What each used gesture does, with the status from the button's last run. */
function ActionChips({ button, names, gestures, run, live, uptimeMs }: { button: SavedButton; names: Names; gestures: GestureName[]; run?: ActionRun; live: boolean; uptimeMs: number }) {
  const { t } = useI18n()
  const used = gestures.filter(gesture => usedTypes(button.actions[gesture]).length)
  if (!used.length) return <span className="whisli-muted text-xs leading-6">{t("actions.none")}</span>
  return <div className="grid gap-1.5">
    {used.map(gesture => <div key={gesture} className="flex min-w-0 flex-wrap items-center gap-1.5">
      {button.gestures && <span className="whisli-muted w-full text-xs sm:w-auto">{t(`actions.gestureShort.${gesture}` as TranslationKey)}:</span>}
      {usedTypes(button.actions[gesture]).map(type => <ActionChip key={type} type={type} live={live} text={chipText(type, button.actions[gesture]!, names, t)}
        step={run?.gesture === gesture ? run.steps.find(step => step.type === type) : undefined} />)}
    </div>)}
    {run && <span className="whisli-muted text-xs">{t("buttons.lastRun", { time: ago(uptimeMs, run.atMs) })}</span>}
  </div>
}

function ActionChip({ type, text, live, step }: { type: ActionType; text: string; live: boolean; step?: ActionRun["steps"][number] }) {
  const { t } = useI18n()
  const Icon = actionIcons[type]
  const title = [t(`actions.type.${type}` as TranslationKey), text, step && t(`step.${step.status}` as TranslationKey), step?.detail].filter(Boolean).join(" · ")
  return <span title={title} key={step?.status} className={cn("whisli-step", step?.status === "failed" && "whisli-step-failed", live && step?.status === "running" && "whisli-step-running", live && step?.status === "ok" && "whisli-step-ok")}>
    <Icon className="size-3.5 shrink-0" />
    <span className="truncate">{text}</span>
    {step && <StepIcon status={step.status} />}
  </span>
}
