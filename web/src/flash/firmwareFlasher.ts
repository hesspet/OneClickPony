import { ESPLoader, Transport, type IEspLoaderTerminal } from "esptool-js";
import type { FirmwareManifest } from "./firmwareCatalog";
import { fetchAndVerifyFirmwareFile } from "./firmwareCatalog";

export interface FlashProgress { percent: number; detail: string; }

function bytesToBinaryString(data: Uint8Array): string {
  const chunkSize = 8192;
  let result = "";
  for (let index = 0; index < data.length; index += chunkSize) result += String.fromCharCode(...data.subarray(index, index + chunkSize));
  return result;
}

function toAddress(offset: string): number { return Number.parseInt(offset, 16); }

export class FirmwareFlasher {
  private transport: Transport | undefined;

  async detect(port: SerialPort, onDetail: (detail: string) => void): Promise<string> {
    this.transport = new Transport(port, false);
    const terminal: IEspLoaderTerminal = { clean: () => undefined, write: onDetail, writeLine: onDetail };
    const loader = new ESPLoader({ transport: this.transport, baudrate: 115200, romBaudrate: 115200, terminal });
    const chip = await loader.main();
    if (!/ESP32-C3/i.test(chip)) throw new Error(`Falsche Chipfamilie erkannt: ${chip}`);
    return chip;
  }

  async flash(manifest: FirmwareManifest, onProgress: (progress: FlashProgress) => void): Promise<void> {
    if (!this.transport) throw new Error("Kein Gerät verbunden.");
    const files = await Promise.all(manifest.files.map(async (file, index) => {
      onProgress({ percent: Math.round((index / manifest.files.length) * 25), detail: `Prüfe ${file.path}` });
      return { address: toAddress(file.offset), data: bytesToBinaryString(await fetchAndVerifyFirmwareFile(file)) };
    }));
    const loader = new ESPLoader({ transport: this.transport, baudrate: manifest.flashBaudRate, romBaudrate: 115200 });
    await loader.main("no_reset");
    await loader.writeFlash({
      fileArray: files,
      flashSize: "keep",
      flashMode: "keep",
      flashFreq: "keep",
      eraseAll: false,
      compress: true,
      reportProgress: (_fileIndex, written, total) => onProgress({ percent: 25 + Math.round((written / total) * 75), detail: "Firmware wird geschrieben" }),
    });
    await loader.after("hard_reset");
  }

  async close(): Promise<void> {
    if (this.transport) await this.transport.disconnect();
    this.transport = undefined;
  }
}
