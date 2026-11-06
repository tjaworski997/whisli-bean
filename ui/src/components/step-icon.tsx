import { Check, LoaderCircle, X, type LucideIcon } from "lucide-react"

import { useI18n, type TranslationKey } from "@/i18n"
import { cn } from "@/lib/utils"
import type { StepStatus } from "@/types"

const icons: Partial<Record<StepStatus, LucideIcon>> = { running: LoaderCircle, ok: Check, failed: X }

/** How one action of a button's last run went. */
export function StepIcon({ status }: { status: StepStatus }) {
  const { t } = useI18n()
  const Icon = icons[status]
  if (!Icon) return null
  return <Icon aria-label={t(`step.${status}` as TranslationKey)} className={cn("size-3.5 shrink-0",
    status === "running" && "animate-spin text-[var(--status-ok-text)]", status === "ok" && "text-[var(--status-ok-text)]", status === "failed" && "text-[var(--status-error)]")} />
}
