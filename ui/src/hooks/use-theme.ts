import { useEffect, useState } from "react"

export type Theme = "light" | "dark"
const storageKey = "whisli-bean.theme"

function initialTheme(): Theme {
  try {
    const saved = localStorage.getItem(storageKey)
    if (saved === "light" || saved === "dark") return saved
  } catch { /* no storage: follow the system */ }
  return window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light"
}

// Set before the first render, so a dark theme does not flash white while the panel starts.
document.documentElement.dataset.theme = initialTheme()

export function useTheme() {
  const [theme, setTheme] = useState<Theme>(initialTheme)

  useEffect(() => { document.documentElement.dataset.theme = theme }, [theme])

  function toggleTheme() {
    const next = theme === "light" ? "dark" : "light"
    setTheme(next)
    try { localStorage.setItem(storageKey, next) } catch { /* not remembered */ }
  }

  return { theme, toggleTheme }
}
