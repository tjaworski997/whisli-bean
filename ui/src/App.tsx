import { CircleX, LogOut, Power, Radio, RadioReceiver, Settings, Webhook, type LucideIcon } from "lucide-react"
import { Fragment, useEffect, type ReactNode } from "react"

import { AppControls } from "@/components/app-controls"
import { Button } from "@/components/button"
import { DeviceProvider, useDevice } from "@/hooks/use-device"
import { useI18n, type TranslationKey } from "@/i18n"
import { post } from "@/lib/api"
import { checkAuth, signedOut, useAuth } from "@/lib/auth"
import { pageHash, useRoute, type Page } from "@/lib/route"
import { cn } from "@/lib/utils"
import { ButtonEditPage } from "@/pages/button-edit-page"
import { ButtonsPage } from "@/pages/buttons-page"
import { DiagnosticsPage } from "@/pages/diagnostics-page"
import { LedPage } from "@/pages/led-page"
import { LogsPage } from "@/pages/logs-page"
import { LoginPage } from "@/pages/login-page"
import { MqttPage } from "@/pages/mqtt-page"
import { NetworkPage } from "@/pages/network-page"
import { ReceiverPage } from "@/pages/receiver-page"
import { ServersPage } from "@/pages/servers-page"
import { SettingsPage } from "@/pages/settings-page"
import { WebhooksPage } from "@/pages/webhooks-page"

// A section's sub-pages show under it while one of them is open.
type NavPage = Exclude<Page, "buttonEdit">
type NavigationItem = { page: NavPage; label: TranslationKey; icon: LucideIcon; alsoFor?: Page[]; children?: { page: NavPage; label: TranslationKey }[] }
const navigation: NavigationItem[] = [
  { page: "buttons", label: "nav.buttons", icon: Radio, alsoFor: ["buttonNew", "buttonEdit"] },
  { page: "webhooks", label: "nav.webhooks", icon: Webhook },
  { page: "servers", label: "nav.servers", icon: Power },
  { page: "receiver", label: "nav.receiver", icon: RadioReceiver },
  { page: "settings", label: "nav.system", icon: Settings, children: [
    { page: "settings", label: "nav.settings" },
    { page: "network", label: "nav.network" },
    { page: "mqtt", label: "nav.mqtt" },
    { page: "led", label: "nav.led" },
    { page: "diagnostics", label: "nav.diagnostics" },
    { page: "logs", label: "nav.logs" },
  ] },
]

const pages: Record<Page, (code: string) => ReactNode> = {
  buttons: () => <ButtonsPage />,
  buttonNew: () => <ButtonEditPage key="new" code="" />,
  buttonEdit: code => <ButtonEditPage key={code} code={code} />,
  webhooks: () => <WebhooksPage />,
  servers: () => <ServersPage />,
  receiver: () => <ReceiverPage />,
  led: () => <LedPage />,
  settings: () => <SettingsPage />,
  network: () => <NetworkPage />,
  mqtt: () => <MqttPage />,
  diagnostics: () => <DiagnosticsPage />,
  logs: () => <LogsPage />,
}

export default function App() {
  const auth = useAuth()
  useEffect(() => { void checkAuth() }, [])
  if (auth === "unknown") return <div className="whisli-page min-h-screen" />
  if (auth === "signedOut") return <LoginPage />
  return <DeviceProvider><Panel /></DeviceProvider>
}

async function logout() {
  await post("/api/v1/auth/logout")
  signedOut()
}

function Panel() {
  const { t } = useI18n()
  const { page, code, navigate } = useRoute()
  const { state, connected } = useDevice()

  useEffect(() => { document.title = state ? `${state.name} · whisli bean` : "whisli bean" }, [state?.name])

  return <div className="whisli-page min-h-screen text-sm">
    <header className="whisli-line flex h-14 items-center gap-2 border-b px-4 md:px-6">
      <b className="text-base">whisli bean</b>
      {state && <span className="whisli-muted ml-2 hidden truncate sm:inline">{state.name}</span>}
      {state && !connected && <span role="status" className="whisli-status-error ml-2 inline-flex min-w-0 items-center gap-1.5 text-xs">
        <CircleX className="size-4 shrink-0" /><span className="truncate">{t("app.disconnected")}</span>
      </span>}
      <div className="ml-auto flex items-center gap-2"><AppControls /></div>
      <Button variant="ghost" className="w-9 justify-center px-0" aria-label={t("app.logout")} title={t("app.logout")} onClick={() => void logout()}>
        <LogOut className="size-4" />
      </Button>
    </header>

    <div className="md:grid md:min-h-[calc(100vh-56px)] md:grid-cols-[220px_1fr]">
      {/* Phone: a horizontal, scrollable bar. Desktop: a sidebar with the firmware version at the bottom. */}
      <aside className="whisli-line border-b md:flex md:flex-col md:border-r md:border-b-0 md:px-4 md:py-5">
        <nav className="flex gap-1 overflow-x-auto px-2 py-2 [scrollbar-width:none] md:block md:space-y-1 md:overflow-visible md:p-0">
          {navigation.map(({ page: target, label, icon: Icon, alsoFor, children }) => {
            const inSection = page === target || Boolean(alsoFor?.includes(page)) || Boolean(children?.some(child => child.page === page))
            return <Fragment key={target}>
              <NavLink page={target} current={!children && inSection} onNavigate={navigate}
                className={cn("flex shrink-0 items-center gap-2 rounded-md px-3 py-2 md:w-full md:gap-3", inSection && !children ? "whisli-nav-active" : inSection ? "font-medium" : "whisli-nav")}>
                <Icon className="size-4" />{t(label)}
              </NavLink>
              {inSection && children?.map(child => <NavLink key={child.page} page={child.page} current={page === child.page} onNavigate={navigate}
                className={cn("flex shrink-0 items-center rounded-md px-3 py-1.5 md:ml-7 md:w-[calc(100%-1.75rem)]", page === child.page ? "whisli-nav-active" : "whisli-nav")}>
                {t(child.label)}
              </NavLink>)}
            </Fragment>
          })}
        </nav>
        <footer className="whisli-line whisli-muted mt-auto hidden border-t pt-4 text-xs md:block">
          <p>{t("app.firmware")}</p>
          <p className="mt-1 font-mono">{state?.version ?? __VERSION__}</p>
        </footer>
      </aside>
      <main className="min-w-0 px-4 py-6 md:px-8 md:py-7 lg:px-10">{pages[page](code)}</main>
    </div>
  </div>
}

/** A real link: middle click and Ctrl/Cmd+click open a new tab; a plain click stays in the app. */
function NavLink({ page, current, className, onNavigate, children }: { page: NavPage; current: boolean; className: string; onNavigate: (page: NavPage) => void; children: ReactNode }) {
  return <a href={pageHash(page)} aria-current={current ? "page" : undefined} className={className}
    onClick={event => {
      if (event.button !== 0 || event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return
      event.preventDefault()
      onNavigate(page)
    }}>{children}</a>
}
