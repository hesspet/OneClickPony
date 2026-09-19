export type SerialCapability = { kind: "ready" } | { kind: "insecure" } | { kind: "unsupported" };

export function getSerialCapability(): SerialCapability {
  if (!window.isSecureContext) return { kind: "insecure" };
  const browserNavigator = navigator as SerialNavigator;
  if (!browserNavigator.serial || typeof browserNavigator.serial.requestPort !== "function" || typeof browserNavigator.serial.getPorts !== "function") {
    return { kind: "unsupported" };
  }
  return { kind: "ready" };
}

export async function getPreviouslyApprovedPorts(): Promise<SerialPort[]> {
  const browserNavigator = navigator as SerialNavigator;
  return getSerialCapability().kind === "ready" ? browserNavigator.serial!.getPorts() : [];
}
