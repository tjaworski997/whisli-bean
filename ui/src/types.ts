// What the firmware sends. Keep in step with src/state.cpp, src/ir.cpp and src/web.cpp.

export type LearnState = "idle" | "waiting" | "learned" | "timedOut"

export interface Learn {
  state: LearnState
  distinct: number
  unreadable: number
  remainingMs?: number
  code?: string
  name?: string
}

export type GestureName = "short" | "long"

export interface Press {
  count: number
  code: string
  atMs: number
  gesture: GestureName | ""
  name?: string
}

export type StepStatus = "pending" | "running" | "ok" | "failed"

/** A button's last run: one step per action used by that gesture. */
export interface ActionRun {
  code: string
  gesture: GestureName
  count: number
  atMs: number
  steps: { type: ActionType; status: StepStatus; detail?: string }[]
}

export interface DeviceState {
  name: string
  hostname: string
  version: string
  uptime: number
  uptimeMs: number
  problems: string[]
  ir: { pin: number; level: number; lastFrameId: number; press: Press | null; learn: Learn }
  actions: { run: ActionRun | null }
  mqtt: MqttState
  rssi: number
  ip: string
  heap: number
  clock: number | null
}

export type ActionType = "mqtt" | "webhook" | "wol"

/** What one gesture does. webhook and wol: an id; 0 while ticked but not chosen yet (never saved). */
export interface GestureActions {
  /** Off keeps the settings but does nothing. On when absent. */
  enabled?: boolean
  /** Long press only: act once (the default), or repeat while the button is down. */
  once?: boolean
  mqtt?: boolean
  webhook?: number
  wol?: number
}

export interface Server {
  id: number
  name: string
  mac: string
  broadcast: string
}

export type HookMethod = "GET" | "POST" | "PUT" | "DELETE"
export type HookBody = "none" | "json" | "form" | "text"

export interface HookSummary {
  id: number
  name: string
  method: HookMethod
  url: string
}

export interface Webhook extends HookSummary {
  headers: string
  body: HookBody
  bodyTemplate: string
  insecure: boolean
}

export interface HookLimits { count: number; name: number; url: number; headers: number; body: number }
export interface HookTest { id: number; running: boolean; sent: boolean; status: number; error: string; response: string }

export interface Button {
  code: string
  name: string
  note?: string
  gestures: boolean
  actions: Partial<Record<GestureName, GestureActions>>
}

export type Protocol = "unknown" | "nec" | "nec-repeat" | "rc5" | "rc6" | "raw"

export interface Frame {
  id: number
  atMs: number
  firstLevel: number
  edges: number
  protocol: Protocol
  /** The device's judgement: from a remote, even if unreadable, rather than noise. */
  remote: boolean
  code?: string
  name?: string
  durationsUs: number[]
}

export interface FramesResponse {
  uptimeMs: number
  lastId: number
  frames: Frame[]
}

export type MqttState = "disabled" | "connecting" | "connected" | "error"

export interface MqttSettings {
  enabled: boolean
  host: string
  port: number
  user: string
  hasPassword: boolean
  baseTopic: string
  defaultBaseTopic: string
  discovery: boolean
  discoveryPrefix: string
  state: MqttState
  error: string
}

export interface WifiStatus {
  wifi: { ssid: string; connected: boolean; ip?: string; rssi?: number; channel?: number; hostname: string }
  ap: { ssid: string; active: boolean; ip?: string; clients?: number; defaultPassword: boolean }
}

export interface WifiScan {
  scanning: boolean
  networks: { ssid: string; rssi: number; secure: boolean }[]
}

export interface SystemInfo {
  version: string
  uptime: number
  resetReason: string
  chip: string
  chipRevision: number
  cpuMhz: number
  sdk: string
  heap: number
  heapTotal: number
  minHeap: number
  maxBlock: number
  flash: number
  firmwareSize: number
  firmwareSpace: number
  loopPerSecond: number
  loopAverageUs: number
  loopMaxUs: number
}

export interface Diagnostics {
  system: SystemInfo
  ir: { pin: number; level: number; edges: number; frames: number; framesWithCode: number }
  network: {
    ssid: string
    connected: boolean
    hostname: string
    mac: string
    disconnects: number
    ap: boolean
    bssid?: string
    channel?: number
    rssi?: number
    ip?: string
    gateway?: string
    subnet?: string
    dns?: string
    connectedFor?: number
  }
  mqtt: { state: MqttState; error: string; host: string; port: number; base: string; published: number; connects: number }
}

export type LogLevel = "info" | "warning" | "error"
export interface LogEntry { id: number; uptimeMs: number; level: LogLevel; category: string; code: string; detail: string }
export interface LogResponse { sessionId: number; capacity: number; hasMore: boolean; entries: LogEntry[] }
