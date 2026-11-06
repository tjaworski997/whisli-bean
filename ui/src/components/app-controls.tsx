import { Moon, Sun } from "lucide-react"

import { Button } from "@/components/button"
import { useTheme } from "@/hooks/use-theme"
import { localeLabels, useI18n, type Locale } from "@/i18n"

/** Language and theme: in the panel's header and on the login screen. */
export function AppControls() {
  const { t, locale, setLocale } = useI18n()
  const { theme, toggleTheme } = useTheme()
  return <>
    <select className="whisli-select h-8 text-xs" aria-label={t("app.language")} value={locale} onChange={event => setLocale(event.target.value as Locale)}>
      {Object.entries(localeLabels).map(([value, label]) => <option key={value} value={value}>{label}</option>)}
    </select>
    <Button variant="ghost" className="w-9 justify-center px-0" aria-label={t("app.theme")} title={t("app.theme")} onClick={toggleTheme}>
      {theme === "light" ? <Moon className="size-4" /> : <Sun className="size-4" />}
    </Button>
  </>
}
