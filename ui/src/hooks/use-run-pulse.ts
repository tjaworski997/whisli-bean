import { useEffect, useRef, useState } from "react"

import type { ActionRun } from "@/types"

/**
 * Which button to animate: one that has just sent something. The device reports each real run of a
 * button's actions, so the animation follows sending, not frames of the remote. A button held with
 * "only once" animates once, one that repeats animates with every repeat, one with nothing to do never.
 */
export function useRunPulse(ready: boolean, run: ActionRun | null) {
  const [pulse, setPulse] = useState<{ code: string; id: number } | null>(null)
  const lastRun = useRef<number | null>(null)

  useEffect(() => {
    if (!ready) return // no live state yet, so nothing to compare with
    const count = run?.count ?? 0
    const isNew = lastRun.current !== null && count !== lastRun.current // what ran before this page opened is not news
    lastRun.current = count
    if (isNew && run) setPulse({ code: run.code, id: run.count })
  }, [ready, run?.count])

  return pulse
}
