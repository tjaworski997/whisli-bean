import { useEffect, useRef, useState } from "react"

import { Button } from "@/components/button"
import { InfoList, Muted, PageHeader, Section } from "@/components/page"
import { useDevice } from "@/hooks/use-device"
import { useI18n } from "@/i18n"
import { getJson } from "@/lib/api"
import { ago } from "@/lib/format"
import { cn } from "@/lib/utils"
import type { Frame, FramesResponse } from "@/types"

const keptFrames = 20
const heldWithinMs = 300 // holding a button repeats the frame this often or faster
const noiseToWarn = 3 // every boot leaves a stray pulse on the pin, so a lone one says nothing

type Row = Frame & { repeats: number }

const isRemote = (f: Frame) => f.remote

/** Adds new frames to the list: the frames of one held button become one row. */
function merge(rows: Row[], frames: Frame[]) {
  let next = rows
  for (const frame of frames) {
    const top = next.find(isRemote)
    const held = top?.code && top.code === frame.code && frame.atMs - top.atMs < heldWithinMs
    if (top && (held || (frame.protocol === "nec-repeat" && top.protocol.startsWith("nec")))) {
      next = next.map(row => row === top ? { ...row, repeats: row.repeats + 1, atMs: frame.atMs } : row)
    } else {
      next = [{ ...frame, repeats: 0 }, ...next]
    }
  }
  // Noise must not push remote frames out of the list.
  return [...next.filter(isRemote).slice(0, keptFrames), ...next.filter(f => !isRemote(f)).slice(0, keptFrames)].sort((a, b) => b.id - a.id)
}

export function ReceiverPage() {
  const { t } = useI18n()
  const { state } = useDevice()
  const [rows, setRows] = useState<Row[]>([])
  const [noise, setNoise] = useState(0)
  const [remote, setRemote] = useState(0)
  const [showNoise, setShowNoise] = useState(false)
  const since = useRef(0)
  const lastFrameId = state?.ir.lastFrameId

  // The live state says when a frame arrived; its timings come from a separate, larger answer.
  useEffect(() => {
    if (lastFrameId === undefined) return
    if (lastFrameId < since.current) { // the device restarted
      since.current = 0
      setRows([]); setNoise(0); setRemote(0)
    }
    if (lastFrameId === since.current && since.current !== 0) return
    let active = true
    void getJson<FramesResponse>(`/api/v1/ir?since=${since.current}`).then(result => {
      if (!active) return
      since.current = result.lastId
      if (!result.frames.length) return
      setRows(list => merge(list, result.frames))
      setRemote(n => n + result.frames.filter(isRemote).length)
      setNoise(n => n + result.frames.filter(f => !isRemote(f)).length)
    }).catch(() => { /* the next change tries again */ })
    return () => { active = false }
  }, [lastFrameId])

  if (!state) return <Muted>{t("app.connecting")}</Muted>

  const shown = showNoise ? rows : rows.filter(isRemote)
  const real = rows.find(isRemote)
  const verdict = real
    ? { title: t("receiver.works"), hint: real.code ? t("receiver.lastCode", { code: real.code }) : t("receiver.unreadableFrame"), className: "whisli-status-ok" }
    : !state.ir.level
      ? { title: t("receiver.stuckLow", { pin: state.ir.pin }), hint: t("receiver.stuckLowHint"), className: "whisli-status-error" }
      : noise >= noiseToWarn
        ? { title: t("receiver.noFrame"), hint: t("receiver.noFrameHint"), className: "" }
        : { title: t("receiver.waiting"), hint: t("receiver.waitingHint"), className: "" }

  return <>
    <PageHeader title={t("nav.receiver")} description={t("receiver.description")} />
    <div className="mt-6 grid max-w-3xl gap-4 md:grid-cols-2">
      <div className="whisli-card p-5">
        <p className={cn("text-lg font-semibold", verdict.className)}>{verdict.title}</p>
        <p className="whisli-muted mt-1">{verdict.hint}</p>
      </div>
      <div className="whisli-card p-5">
        <InfoList rows={[
          [`${t("receiver.level")} · GPIO${state.ir.pin}`, t(state.ir.level ? "receiver.levelHigh" : "receiver.levelLow")],
          [t("receiver.remoteFrames"), remote],
          [t("receiver.noiseFrames"), noise],
        ]} />
      </div>
    </div>
    <Section title={t("receiver.frames")} actions={<>
      <label className="flex items-center gap-2"><input className="whisli-check" type="checkbox" checked={showNoise} onChange={event => setShowNoise(event.target.checked)} />{t("receiver.showNoise")}</label>
      <Button variant="ghost" onClick={() => { setRows([]); setNoise(0); setRemote(0) }}>{t("receiver.clear")}</Button>
    </>}>
      {!shown.length ? <Muted className="mt-3">{t("receiver.none")}</Muted> : <ul className="mt-2 max-w-3xl divide-y divide-[var(--line)]">
        {shown.map(frame => <FrameRow key={frame.id} frame={frame} uptimeMs={state.uptimeMs} />)}
      </ul>}
    </Section>
  </>
}

function FrameRow({ frame, uptimeMs }: { frame: Row; uptimeMs: number }) {
  const { t } = useI18n()
  const title = frame.code ? frame.name ?? frame.code
    : frame.protocol === "nec-repeat" ? t("receiver.repeat")
    : t(isRemote(frame) ? "receiver.unreadable" : "receiver.noise")
  const protocol = frame.protocol === "raw" ? t("receiver.fingerprint") : frame.protocol.startsWith("nec") ? "NEC" : frame.protocol === "unknown" ? "" : frame.protocol.toUpperCase()
  return <li className="py-3">
    <div className="flex flex-wrap items-baseline gap-x-3 gap-y-1">
      <span className="font-medium">{title}{frame.repeats > 0 && <span className="whisli-muted"> ×{frame.repeats + 1}</span>}</span>
      <span className="whisli-muted text-xs">{[frame.code && frame.code !== title ? frame.code : "", t("receiver.edges", { count: frame.edges }), protocol].filter(Boolean).join(" · ")}</span>
      <span className="whisli-muted ml-auto text-xs tabular-nums">{ago(uptimeMs, frame.atMs)}</span>
    </div>
    {frame.durationsUs.length > 0 && <>
      <Wave frame={frame} />
      <details className="whisli-muted mt-1 text-xs">
        <summary className="cursor-pointer">{t("receiver.times")}</summary>
        <code className="mt-1 block break-words font-mono">{frame.durationsUs.join(" ")}</code>
      </details>
    </>}
  </li>
}

/** The frame as a picture: a bar where the receiver reports the carrier. */
function Wave({ frame }: { frame: Frame }) {
  const total = frame.durationsUs.reduce((sum, value) => sum + value, 0) || 1
  let x = 0
  let active = frame.firstLevel === 0
  const bars = frame.durationsUs.map((value, index) => {
    const bar = active ? <rect key={index} x={x} width={value} height={10} /> : null
    x += value
    active = !active
    return bar
  })
  return <svg aria-hidden className="mt-2 block h-5 w-full fill-[var(--status-ok)]" viewBox={`0 0 ${total} 10`} preserveAspectRatio="none">{bars}</svg>
}
