export interface FirmwareFile {
  path: string;
  offset: string;
  size: number;
  sha256: string;
}

export interface FirmwareManifest {
  schemaVersion: number;
  product: string;
  gitCommit: string;
  flashBaudRate: number;
  preservedRegions: Array<{ name: string; offset: string; size: string }>;
  files: FirmwareFile[];
}

export interface FirmwareRelease {
  productName: string;
  version: string;
  releasedAt: string;
  targetChip: "ESP32-C3";
  status: "stable" | "testing";
  manifest: string;
  releaseNotes: Record<"de" | "en", string>;
}

interface FirmwareCatalog { releases: FirmwareRelease[]; }

function isDate(value: string): boolean { return /^\d{2}\.\d{2}\.\d{4}$/.test(value); }
function isOffset(value: string): boolean { return /^0x[0-9a-f]+$/i.test(value); }

export async function loadStableFirmware(): Promise<{ release: FirmwareRelease; manifest: FirmwareManifest }> {
  const catalog = await fetch("firmware/catalog.json", { cache: "no-store" }).then(async (response) => {
    if (!response.ok) throw new Error(`Katalog konnte nicht geladen werden (${response.status}).`);
    return response.json() as Promise<FirmwareCatalog>;
  });
  const release = catalog.releases.find((candidate) => candidate.status === "stable");
  if (!release || release.targetChip !== "ESP32-C3" || !isDate(release.releasedAt) || !release.manifest) throw new Error("Der Firmwarekatalog ist ungültig.");
  const manifest = await fetch(`firmware/${release.manifest}`, { cache: "no-store" }).then(async (response) => {
    if (!response.ok) throw new Error(`Manifest konnte nicht geladen werden (${response.status}).`);
    return response.json() as Promise<FirmwareManifest>;
  });
  if (manifest.schemaVersion !== 1 || !Array.isArray(manifest.files) || manifest.files.length === 0 || manifest.files.some((file) => !file.path || !isOffset(file.offset) || file.size < 1 || !/^[a-f0-9]{64}$/i.test(file.sha256))) {
    throw new Error("Das Firmwaremanifest ist ungültig.");
  }
  return { release, manifest };
}

export async function fetchAndVerifyFirmwareFile(file: FirmwareFile): Promise<Uint8Array> {
  const response = await fetch(`firmware/${file.path}`, { cache: "no-store" });
  if (!response.ok) throw new Error(`Firmwaredatei fehlt: ${file.path}`);
  const data = new Uint8Array(await response.arrayBuffer());
  if (data.byteLength !== file.size) throw new Error(`Ungültige Dateigröße: ${file.path}`);
  const hash = [...new Uint8Array(await crypto.subtle.digest("SHA-256", data))].map((part) => part.toString(16).padStart(2, "0")).join("");
  if (hash !== file.sha256.toLowerCase()) throw new Error(`Prüfsumme stimmt nicht: ${file.path}`);
  return data;
}
