export type StatusKind = "info" | "success" | "error";

export function renderStatusMessage(container: HTMLElement, text: string, kind: StatusKind = "info"): void {
  container.innerHTML = `<div class="status-message" data-kind="${kind}" role="status"><span aria-hidden="true">${kind === "success" ? "✓" : kind === "error" ? "!" : "i"}</span><div>${text}</div></div>`;
}
