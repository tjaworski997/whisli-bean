import { RotateCcw } from "lucide-react"

import { Button } from "@/components/button"
import { useI18n } from "@/i18n"
import { post } from "@/lib/api"
import { buttonHash } from "@/lib/route"
import { cn } from "@/lib/utils"
import type { Learn } from "@/types"

/** Empty and blinking until the device catches a button. */
export function CodeField({ learn }: { learn?: Learn }) {
  const { t } = useI18n()
  if (learn?.state === "learned") return <div className="flex items-center gap-2">
    <span className={cn("whisli-input flex min-w-0 flex-1 items-center font-mono text-xs", learn.name ? "border-[var(--status-error)]" : "border-[var(--status-ok)]")}>{learn.code}</span>
    <Button variant="ghost" className="shrink-0" onClick={() => void post("/api/v1/ir/learn")}><RotateCcw className="size-4" />{t("edit.relearn")}</Button>
  </div>
  return <div role="status" className="whisli-input whisli-listening flex items-center">
    <span>{t(learn?.distinct ? "learn.pressAgain" : "learn.press")}</span>
  </div>
}

export function CodeHint({ learn }: { learn?: Learn }) {
  const { t } = useI18n()
  if (learn?.state === "learned" && learn.name) return <span className="whisli-status-error text-xs">
    {t("learn.exists", { name: learn.name })} · <a className="underline" href={buttonHash(learn.code ?? "")}>{t("learn.openExisting")}</a>
  </span>
  if (learn?.state === "learned") return null
  if (learn && learn.distinct >= 4) return <span className="whisli-status-error text-xs">{t("buttons.changing")}</span>
  if (learn?.unreadable) return <span className="whisli-status-error text-xs">{t("buttons.unreadable")}</span>
  return <span className="whisli-muted text-xs">{t(learn?.distinct ? "learn.pressAgainHint" : "learn.pressHint")}</span>
}
