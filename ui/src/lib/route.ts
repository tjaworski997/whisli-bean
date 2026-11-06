import { useEffect, useRef, useState } from "react"

// Hash routing: every page has its own #/… address, so links can open in a new tab.
const hashes = {
  buttons: "#/buttons",
  buttonNew: "#/buttons/new",
  webhooks: "#/webhooks",
  servers: "#/servers",
  receiver: "#/receiver",
  settings: "#/system",
  network: "#/system/network",
  mqtt: "#/system/mqtt",
  led: "#/system/led",
  diagnostics: "#/system/diagnostics",
  logs: "#/system/logs",
} as const

const editPrefix = "#/buttons/"

export type Page = keyof typeof hashes | "buttonEdit"
export type Route = { page: Page; code: string }

export function pageHash(page: keyof typeof hashes) {
  return hashes[page]
}

export function buttonHash(code: string) {
  return editPrefix + encodeURIComponent(code)
}

/** Goes to an address from inside a page. */
export function goTo(hash: string) {
  window.location.hash = hash
}

function parse(hash = window.location.hash): Route {
  const found = (Object.keys(hashes) as (keyof typeof hashes)[]).find(page => hashes[page] === hash)
  if (found) return { page: found, code: "" }
  if (hash.startsWith(editPrefix)) return { page: "buttonEdit", code: decodeURIComponent(hash.slice(editPrefix.length)) }
  return { page: "buttons", code: "" }
}

// A page with unsaved work can refuse to be left. It returns false to stay.
let leaveGuard: (() => boolean) | null = null
export function setLeaveGuard(guard: (() => boolean) | null) {
  leaveGuard = guard
}

export function useRoute() {
  const [route, setRoute] = useState<Route>(() => parse())
  const shownHash = useRef(window.location.hash)

  useEffect(() => {
    const sync = () => {
      if (window.location.hash === shownHash.current) return
      if (leaveGuard && !leaveGuard()) {
        window.history.pushState(null, "", shownHash.current) // stay: undo the address change
        return
      }
      shownHash.current = window.location.hash
      setRoute(parse())
    }
    window.addEventListener("hashchange", sync)
    if (!window.location.hash) {
      window.history.replaceState(null, "", hashes.buttons)
      shownHash.current = hashes.buttons
    }
    return () => window.removeEventListener("hashchange", sync)
  }, [])

  function navigate(next: keyof typeof hashes) {
    if (hashes[next] === shownHash.current || (leaveGuard && !leaveGuard())) return
    window.history.pushState(null, "", hashes[next])
    shownHash.current = hashes[next]
    setRoute({ page: next, code: "" })
  }

  return { ...route, navigate }
}
