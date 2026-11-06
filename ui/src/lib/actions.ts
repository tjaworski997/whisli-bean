import { Power, Send, Webhook, type LucideIcon } from "lucide-react"

import type { ActionType, Button, GestureName } from "@/types"

export const gestureNames: GestureName[] = ["short", "long"]
export const actionTypes: ActionType[] = ["mqtt", "webhook", "wol"]
export const actionIcons: Record<ActionType, LucideIcon> = { mqtt: Send, webhook: Webhook, wol: Power }

/** How many gestures of all buttons use each webhook or server. */
export function countUses(buttons: Button[], type: "webhook" | "wol") {
  const counts: Record<number, number> = {}
  for (const button of buttons) {
    for (const chosen of Object.values(button.actions)) {
      const id = chosen?.[type]
      if (id) counts[id] = (counts[id] ?? 0) + 1
    }
  }
  return counts
}
