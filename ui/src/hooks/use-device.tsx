import { createContext, useContext, useEffect, useState, type ReactNode } from "react"

import { checkAuth } from "@/lib/auth"
import type { DeviceState } from "@/types"

const staleMs = 4000 // the device sends every second

type Device = {
  state: DeviceState | null
  /** WebSocket open and data arrives */
  connected: boolean
}

const DeviceContext = createContext<Device>({ state: null, connected: false })

function socketUrl(path: string) {
  return `${window.location.protocol === "https:" ? "wss" : "ws"}://${window.location.host}${path}`
}

/** Live state from ws://<device>/api/v1/ws; reconnects by itself. */
export function DeviceProvider({ children }: { children: ReactNode }) {
  const [state, setState] = useState<DeviceState | null>(null)
  const [receivedAt, setReceivedAt] = useState(0)
  const [now, setNow] = useState(Date.now())

  useEffect(() => {
    let socket: WebSocket | null = null
    let timer = 0
    let retry = 0
    let disposed = false

    function connect() {
      socket = new WebSocket(socketUrl("/api/v1/ws"))
      socket.onopen = () => { retry = 0 }
      socket.onmessage = event => {
        let message: DeviceState
        try { message = JSON.parse(event.data) } catch { return }
        setState(message)
        setReceivedAt(Date.now())
      }
      socket.onclose = () => {
        if (disposed) return
        if (retry === 0) void checkAuth() // a refused handshake usually means the session expired
        timer = window.setTimeout(connect, Math.min(1000 * 2 ** retry++, 8000))
      }
    }

    connect()
    const clock = window.setInterval(() => setNow(Date.now()), 1000)
    return () => {
      disposed = true
      window.clearTimeout(timer)
      window.clearInterval(clock)
      socket?.close()
    }
  }, [])

  const connected = receivedAt > 0 && now - receivedAt < staleMs
  return <DeviceContext.Provider value={{ state, connected }}>{children}</DeviceContext.Provider>
}

export function useDevice() {
  return useContext(DeviceContext)
}
