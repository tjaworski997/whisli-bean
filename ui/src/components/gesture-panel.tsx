import { FlaskConical } from "lucide-react"
import { useState, type ReactNode } from "react"

import { Button } from "@/components/button"
import { IconTile } from "@/components/page"
import { StepIcon } from "@/components/step-icon"
import { useDevice } from "@/hooks/use-device"
import { useI18n, type TranslationKey } from "@/i18n"
import { actionIcons } from "@/lib/actions"
import { post } from "@/lib/api"
import { pageHash } from "@/lib/route"
import { cn } from "@/lib/utils"
import type { ActionRun, ActionType, GestureActions, GestureName, HookSummary, Server } from "@/types"

/** The three things a gesture can do. */
export function GesturePanel({ gesture, value, hooks, servers, buttonCode, dirty, run, pulseId, onChange }: {
  gesture: GestureName
  value: GestureActions
  hooks: HookSummary[]
  servers: Server[]
  buttonCode?: string
  dirty: boolean
  run?: ActionRun
  pulseId: number
  onChange: (next: GestureActions) => void
}) {
  const { t } = useI18n()
  const stepOf = (type: ActionType) => run?.steps.find(step => step.type === type)
  const used = Boolean(value.mqtt) || value.webhook !== undefined || value.wol !== undefined
  // Ticking with a single option picks it; 0 means "still to choose", which saving refuses.
  const first = (list: { id: number }[]) => list.length === 1 ? list[0].id : 0

  return <>
    <div className="whisli-card divide-y divide-[var(--line)]">
      <Row type="mqtt" step={stepOf("mqtt")} pulseId={pulseId} checked={Boolean(value.mqtt)} onCheck={checked => onChange({ ...value, mqtt: checked })} />
      <Row type="webhook" step={stepOf("webhook")} pulseId={pulseId} checked={value.webhook !== undefined} onCheck={checked => onChange({ ...value, webhook: checked ? first(hooks) : undefined })}>
        <Picker value={value.webhook} options={hooks} none={t("edit.noHooks")} manage="webhooks" onChange={webhook => onChange({ ...value, webhook })} />
      </Row>
      <Row type="wol" step={stepOf("wol")} pulseId={pulseId} checked={value.wol !== undefined} onCheck={checked => onChange({ ...value, wol: checked ? first(servers) : undefined })}>
        <Picker value={value.wol} options={servers} none={t("edit.noServers")} manage="servers" onChange={wol => onChange({ ...value, wol })} />
      </Row>
    </div>
    {buttonCode && <div className="mt-3 flex flex-wrap items-center gap-2"><TestButton code={buttonCode} gesture={gesture} dirty={dirty} used={used} /></div>}
  </>
}

function Row({ type, step, pulseId, checked, onCheck, children }: { type: ActionType; step?: ActionRun["steps"][number]; pulseId: number; checked: boolean; onCheck: (checked: boolean) => void; children?: ReactNode }) {
  const { t } = useI18n()
  return <div className="relative flex flex-wrap items-center gap-x-3 gap-y-2 p-3">
    {/* A separate layer restarts the animation without rebuilding the fields, which would lose typing and focus */}
    {step && pulseId !== 0 && <span key={pulseId} aria-hidden className="whisli-row-pressed pointer-events-none absolute inset-0" />}
    <label className="flex min-w-0 flex-1 basis-56 cursor-pointer items-center gap-3">
      <input className="whisli-check shrink-0" type="checkbox" checked={checked} onChange={event => onCheck(event.target.checked)} />
      <IconTile icon={actionIcons[type]} className={cn(!checked && "opacity-50")} />
      <span className="min-w-0">
        <span className="block font-medium">{t(`actions.type.${type}` as TranslationKey)}</span>
        <span className="whisli-muted block text-xs">{t(`actions.typeHint.${type}` as TranslationKey)}</span>
      </span>
    </label>
    {checked && children}
    {step && <span title={t(`step.${step.status}` as TranslationKey)} className={cn("flex shrink-0 items-center gap-1.5 text-xs", step.status === "failed" ? "whisli-status-error" : "whisli-muted")}>
      <StepIcon status={step.status} />{step.detail && <span className="max-w-48 truncate">{step.detail}</span>}
    </span>}
  </div>
}

function Picker({ value, options, none, manage, onChange }: { value?: number; options: { id: number; name: string }[]; none: string; manage: "webhooks" | "servers"; onChange: (id: number) => void }) {
  const { t } = useI18n()
  if (!options.length) return <a className="whisli-status-error basis-56 text-xs underline" href={pageHash(manage)}>{none}</a>
  const missing = Boolean(value) && !options.some(option => option.id === value)
  return <select className={cn("whisli-select min-w-0 flex-1 basis-56", (!value || missing) && "border-[var(--status-error)]")} value={missing ? 0 : value ?? 0} onChange={event => onChange(Number(event.target.value))}>
    <option value={0}>{t("actions.choose")}</option>
    {options.map(option => <option key={option.id} value={option.id}>{option.name}</option>)}
  </select>
}

/** Runs the saved version of a gesture and shows how it went. */
function TestButton({ code: buttonCode, gesture, dirty, used }: { code: string; gesture: GestureName; dirty: boolean; used: boolean }) {
  const { t, code } = useI18n()
  const { state } = useDevice()
  const [startedAt, setStartedAt] = useState<number | null>(null)
  const [error, setError] = useState("")
  const run = state?.actions.run
  const finished = run && startedAt !== null && run.count > startedAt && run.steps.every(step => step.status === "ok" || step.status === "failed")
  const failed = finished ? run.steps.find(step => step.status === "failed") : undefined

  async function test() {
    setError("")
    setStartedAt(run?.count ?? 0)
    const result = await post("/api/v1/buttons/test", { code: buttonCode, gesture })
    if (!result.ok) { setStartedAt(null); setError(code("error", result.error)) }
  }

  return <>
    <Button variant="ghost" disabled={dirty || !used} onClick={() => void test()}><FlaskConical className="size-4" />{t("actions.test")}</Button>
    {!dirty && error && <span className="whisli-status-error text-xs">{error}</span>}
    {!dirty && startedAt !== null && !finished && <span className="whisli-muted text-xs">{t("actions.testRunning")}</span>}
    {!dirty && finished && <span className={cn("min-w-0 break-words text-xs", failed ? "whisli-status-error" : "whisli-status-ok")}>
      {t(failed ? "actions.testFailed" : "actions.testOk")}{failed?.detail && ` · ${failed.detail}`}
    </span>}
  </>
}
