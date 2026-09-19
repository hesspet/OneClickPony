interface SerialPort {
  open(options: { baudRate: number }): Promise<void>;
  close(): Promise<void>;
  readable: ReadableStream<Uint8Array> | null;
  writable: WritableStream<Uint8Array> | null;
  getInfo?(): { usbVendorId?: number; usbProductId?: number };
}

interface BrowserSerial {
  requestPort(options?: { filters?: Array<{ usbVendorId?: number; usbProductId?: number }> }): Promise<SerialPort>;
  getPorts(): Promise<SerialPort[]>;
  addEventListener(type: "connect" | "disconnect", listener: EventListener): void;
  removeEventListener(type: "connect" | "disconnect", listener: EventListener): void;
}

interface SerialNavigator extends Navigator { serial?: BrowserSerial; }
