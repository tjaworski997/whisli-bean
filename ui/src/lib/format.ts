export function duration(seconds: number) {
  const d = Math.floor(seconds / 86400)
  const h = Math.floor(seconds % 86400 / 3600)
  const m = Math.floor(seconds % 3600 / 60)
  return d ? `${d} d ${h} h` : h ? `${h} h ${m} min` : m ? `${m} min ${seconds % 60} s` : `${seconds} s`
}

export function kb(bytes: number) {
  return `${Math.round(bytes / 1024)} KB`
}

/** How long ago something happened, from the device's own clock. */
export function ago(uptimeMs: number, atMs: number) {
  return duration(Math.max(0, Math.floor((uptimeMs - atMs) / 1000)))
}

/** "Webhook", or "Webhook 2" when that name is taken. */
export function uniqueName(base: string, taken: string[]) {
  const names = new Set(taken.map(name => name.toLocaleLowerCase()))
  let name = base
  for (let number = 2; names.has(name.toLocaleLowerCase()); number += 1) name = `${base} ${number}`
  return name
}
