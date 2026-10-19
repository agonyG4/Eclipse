// Prototype for a customized native-Wayland Zen runtime. No WebExtension required.
export function downloadSnapshot(download, source, id) {
  if (download.source.isPrivate) return null;
  const state = download.succeeded ? "completed" : download.error ? "failed" :
    download.canceled ? (download.hasPartialData ? "paused" : "cancelled") : "downloading";
  const size = download.target.size;
  return { source, id, filename:download.target.path.split("/").pop(), state,
    receivedBytes:download.succeeded && Number.isSafeInteger(size) && size >= 0
      ? size : download.currentBytes,
    totalBytes:download.hasProgress ? download.totalBytes : null };
}

export class ProgressRecords {
  constructor(source, limit = 128) {
    this.source = source;
    this.limit = limit;
    this.records = new Map();
    this.pending = new Map();
    this.ids = new WeakMap();
    this.nextId = 0;
  }
  queue(id, request) {
    this.pending.set(id, request);
    // Current snapshots are recoverable through replay. Tombstones are best effort
    // under sustained overload; this is a bounded, session-scoped prototype.
    while (this.pending.size > this.limit * 2) this.pending.delete(this.pending.keys().next().value);
  }
  add(download, initial = false) {
    if (download.source.isPrivate || (initial && download.stopped)) return;
    if (!this.ids.has(download)) this.ids.set(download, String(++this.nextId));
    const id = this.ids.get(download);
    if (!this.records.has(id) && this.records.size >= this.limit) {
      const entries = [...this.records.entries()];
      const terminal = entries.find(([, value]) => value.snapshot.state === "completed") ||
        entries.find(([, value]) => ["failed","cancelled"].includes(value.snapshot.state));
      if (!terminal) return;
      this.remove(terminal[1].download);
    }
    const snapshot = downloadSnapshot(download, this.source, id);
    this.records.set(id, {download, snapshot});
    this.queue(id, {version:1, op:"update", download:snapshot});
  }
  change(download) { this.add(download); }
  remove(download) {
    const id = this.ids.get(download);
    if (!this.records.delete(id)) return;
    this.queue(id, {version:1, op:"remove", source:this.source, id});
  }
  replay() {
    for (const [id, {snapshot}] of this.records) {
      this.queue(id, {version:1, op:"update", download:snapshot});
    }
  }
  acknowledge(request) {
    const id = request.op === "update" ? request.download.id : request.id;
    if (this.pending.get(id) === request) this.pending.delete(id);
  }
}

// One request at a time, with a reply and a deadline. Neither socket reads nor
// writes block the browser's main thread. Only UTF-8 JSON Lines cross the socket.
function request(socketPath, message, timers, onCancel) {
  return new Promise((resolve, reject) => {
    let transport, output, pump, timeout;
    let done = false;
    const finish = (error, value) => {
      if (done) return;
      done = true;
      timers.clearTimeout(timeout);
      onCancel.delete(cancel);
      try { pump?.cancel(Components.results.NS_BINDING_ABORTED); } catch {}
      try { output?.close(); } catch {}
      try { transport?.close(Components.results.NS_OK); } catch {}
      error ? reject(error) : resolve(value);
    };
    const cancel = () => finish(new Error("publisher stopped"));
    onCancel.add(cancel);
    timeout = timers.setTimeout(() => finish(new Error("API request timed out")), 2000);
    try {
      const file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      file.initWithPath(socketPath);
      const service = Cc["@mozilla.org/network/socket-transport-service;1"].getService(Ci.nsISocketTransportService);
      transport = service.createUnixDomainTransport(file);
      output = transport.openOutputStream(0,0,0).QueryInterface(Ci.nsIAsyncOutputStream);
      const encoded = new TextEncoder().encode(JSON.stringify(message) + "\n");
      if (encoded.length > 65536) throw new Error("API request exceeds 64 KiB");
      const bytes = String.fromCharCode(...encoded);
      let offset = 0;
      const writer = {
        onOutputStreamReady(stream) {
          if (done) return;
          try {
            while (offset < bytes.length) {
              const count = stream.write(bytes.slice(offset), bytes.length - offset);
              if (!count) throw new Error("API output closed");
              offset += count;
            }
          } catch (error) {
            if (error.result === Components.results.NS_BASE_STREAM_WOULD_BLOCK) {
              stream.asyncWait(writer,0,0,Services.tm.currentThread);
            } else finish(error);
          }
        },
      };
      pump = Cc["@mozilla.org/network/input-stream-pump;1"].createInstance(Ci.nsIInputStreamPump);
      pump.init(transport.openInputStream(0,0,0),0,0,true);
      const decoder = new TextDecoder();
      let response = "", received = 0;
      pump.asyncRead({
        onStartRequest() {},
        onDataAvailable(_request, input, _offset, count) {
          if (done) return;
          try {
            received += count;
            if (received > 65536) throw new Error("API reply exceeds 64 KiB");
            const binary = Cc["@mozilla.org/binaryinputstream;1"].createInstance(Ci.nsIBinaryInputStream);
            binary.setInputStream(input);
            response += decoder.decode(Uint8Array.from(binary.readByteArray(count)),{stream:true});
            const newline = response.indexOf("\n");
            if (newline !== -1) {
              const value = JSON.parse(response.slice(0,newline));
              if (value.version !== 1 || typeof value.ok !== "boolean") throw new Error("Invalid API reply");
              finish(null,value);
            }
          } catch (error) { finish(error); }
        },
        onStopRequest() { if (!done) finish(new Error("API disconnected")); },
      });
      output.asyncWait(writer,0,0,Services.tm.currentThread);
    } catch (error) { finish(error); }
  });
}

let instance;
export async function start() {
  if (instance) return;
  const socketPath = Services.env.get("ASTREA_DOWNLOADS_SOCKET");
  const source = Services.env.get("ASTREA_DOWNLOADS_SOURCE");
  if (!socketPath.startsWith("/") || !source) throw new Error("Missing prototype socket/source");
  const { Downloads } = ChromeUtils.importESModule("resource://gre/modules/Downloads.sys.mjs");
  const timers = ChromeUtils.importESModule("resource://gre/modules/Timer.sys.mjs");
  const records = new ProgressRecords(source);
  const cancels = new Set();
  let initial = true, busy = false, stopped = false, replayAt = 0, lastError = "";
  const view = {
    onDownloadAdded:download => records.add(download,initial),
    onDownloadChanged:download => records.change(download),
    onDownloadRemoved:download => records.remove(download),
  };
  const list = await Downloads.getList(Downloads.PUBLIC);
  await list.addView(view);
  initial = false;
  const flush = async () => {
    if (busy || stopped) return;
    busy = true;
    try {
      const ping = await request(socketPath,{version:1,op:"ping"},timers,cancels);
      if (!ping.ok) throw new Error("API ping rejected");
      if (Date.now() >= replayAt) { records.replay(); replayAt = Date.now() + 5000; }
      for (const message of [...records.pending.values()]) {
        if (stopped) break;
        const reply = await request(socketPath,message,timers,cancels);
        if (!reply.ok) throw new Error("API rejected " + (reply.error || "request"));
        records.acknowledge(message);
      }
      lastError = "";
    } catch (error) {
      replayAt = 0;
      const reason = String(error);
      if (!stopped && reason !== lastError) {
        console.warn("Astrea Zen Downloads:",reason);
        lastError = reason;
      }
    } finally { busy = false; }
  };
  const interval = timers.setInterval(flush,1000);
  const shutdown = () => {
    stopped = true;
    timers.clearInterval(interval);
    list.removeView(view);
    for (const cancel of [...cancels]) cancel();
    Services.obs.removeObserver(shutdown,"quit-application-granted");
  };
  instance = {shutdown};
  Services.obs.addObserver(shutdown,"quit-application-granted");
  dump("Astrea Zen Downloads: publisher ready\n");
  void flush();
}
