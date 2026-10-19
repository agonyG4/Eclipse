/* Shared Zen/Brave adapter. No website scripts or remote requests. */
"use strict";

function snapshot(item, source) {
    if (item.incognito || !item.filename || item.filename.length > 4096)
        return null;
    let state;
    if (item.state === "complete")
        state = "completed";
    else if (item.state === "interrupted")
        state = item.error === "USER_CANCELED" ? "cancelled" : "failed";
    else if (item.state === "in_progress")
        state = item.paused ? "paused" : "downloading";
    else
        return null;
    const bytes = value => Number.isSafeInteger(value) && value >= 0;
    return { source, id: String(item.id), filename: item.filename, state,
             receivedBytes: bytes(item.bytesReceived) ? item.bytesReceived : 0,
             totalBytes: bytes(item.totalBytes) ? item.totalBytes : null };
}

class DownloadBridge {
    constructor(api, browserName, options = {}) {
        this.api = api;
        this.browserName = browserName;
        this.setTimeout = options.setTimeout || globalThis.setTimeout.bind(globalThis);
        this.clearTimeout = options.clearTimeout || globalThis.clearTimeout.bind(globalThis);
        this.randomId = options.randomId || (() => crypto.randomUUID());
        this.onError = options.onError || (message => console.warn("Astrea Downloads:", message));
        this.known = new Set();
        this.removed = new Set();
        this.sent = new Map();
        this.port = null;
        this.waiter = null;
        this.timer = null;
        this.running = null;
        this.activeCount = 0;
        this.failed = false;
    }

    async initialize() {
        const saved = await this.api.storage.local.get(["producerId", "trackedIds"]);
        const producerId = typeof saved.producerId === "string"
            && /^[a-zA-Z0-9-]{1,80}$/.test(saved.producerId) ? saved.producerId : this.randomId();
        this.source = `${this.browserName}:${producerId}`;
        this.known = new Set((Array.isArray(saved.trackedIds) ? saved.trackedIds : [])
            .filter(Number.isSafeInteger).filter(id => id >= 0).slice(-256));
        await this.api.storage.local.set({ producerId });
        this.savedIds = JSON.stringify([...this.known]);
    }

    start() {
        if (this.ready)
            return this.scan();
        this.ready = this.initialize();
        this.api.downloads.onCreated.addListener(async item => {
            if (item.incognito)
                return;
            await this.ready;
            this.known.add(item.id);
            return this.scan();
        });
        this.api.downloads.onChanged.addListener(async change => {
            await this.ready;
            if (this.known.has(change.id))
                return this.scan();
        });
        this.api.downloads.onErased.addListener(async id => {
            await this.ready;
            if (this.known.delete(id)) {
                this.removed.add(id);
                this.sent.delete(id);
                return this.scan();
            }
        });
        return this.scan();
    }

    schedule(delay) {
        if (this.timer !== null)
            this.clearTimeout(this.timer);
        this.timer = this.setTimeout(() => {
            this.timer = null;
            return this.scan();
        }, delay);
    }

    connect() {
        if (this.port)
            return;
        const port = this.api.runtime.connectNative("org.astrea.downloads");
        this.port = port;
        port.onMessage.addListener(response => {
            if (this.port === port && this.waiter) {
                const waiter = this.waiter;
                this.waiter = null;
                waiter.resolve(response);
            }
        });
        port.onDisconnect.addListener(() => {
            // Reading lastError in the callback acknowledges Chromium's diagnostic.
            const lastError = this.api.runtime.lastError;
            if (this.port !== port)
                return;
            this.port = null;
            this.sent.clear();
            if (this.waiter) {
                const waiter = this.waiter;
                this.waiter = null;
                waiter.reject(new Error(lastError?.message || "Native host disconnected"));
            }
            this.schedule(2000);
        });
    }

    async send(request) {
        this.connect();
        let timeout;
        try {
            const response = await new Promise((resolve, reject) => {
                this.waiter = { resolve, reject };
                timeout = this.setTimeout(() => {
                    this.waiter = null;
                    const port = this.port;
                    this.port = null;
                    port?.disconnect();
                    this.sent.clear();
                    reject(new Error("Native host response timed out"));
                }, 5000);
                this.port.postMessage(request);
            });
            if (!response.ok && !(request.op === "remove" && response.error === "not_found"))
                throw new Error(response.message || "Native host rejected download update");
            if (response.reconnected) {
                this.sent.clear();
                this.dirty = true;
            }
            return response;
        } finally {
            this.waiter = null;
            if (timeout !== undefined)
                this.clearTimeout(timeout);
        }
    }

    scan() {
        if (this.running) {
            this.dirty = true;
            return this.running;
        }
        if (this.timer !== null) {
            this.clearTimeout(this.timer);
            this.timer = null;
        }
        this.running = (async () => {
            await this.ready;
            do {
                this.dirty = false;
                await this.synchronize();
            } while (this.dirty);
            this.failed = false;
        })().catch(error => {
            this.failed = true;
            this.onError(error.message);
        }).finally(() => {
            this.running = null;
            if (this.failed || this.activeCount)
                this.schedule(this.failed ? 2000 : 1000);
        });
        return this.running;
    }

    async synchronize() {
        // A real round trip detects API restarts even when byte counts are unchanged.
        await this.send({ version: 1, op: "ping" });
        const active = (await this.api.downloads.search({ state: "in_progress" }))
            .filter(item => !item.incognito);
        const current = new Map(active.map(item => [item.id, item]));
        this.activeCount = active.length;
        for (const item of active)
            this.known.add(item.id);
        // Prefer discarding old terminal records when the tracking window fills.
        for (const id of [...this.known]) {
            if (this.known.size <= 256)
                break;
            if (!current.has(id)) {
                this.known.delete(id);
                this.removed.add(id);
            }
        }
        while (this.known.size > 256) {
            const id = this.known.values().next().value;
            this.known.delete(id);
            this.removed.add(id);
        }
        for (const id of [...this.removed]) {
            await this.send({ version: 1, op: "remove", source: this.source, id: String(id) });
            this.removed.delete(id);
            this.sent.delete(id);
        }
        for (const id of [...this.known]) {
            const item = current.get(id) || (await this.api.downloads.search({ id }))[0];
            if (!this.known.has(id))
                continue; // An erasure arrived while the browser query was pending.
            if (!item || item.incognito) {
                this.known.delete(id);
                this.removed.add(id);
                this.dirty = true;
                continue;
            }
            const update = snapshot(item, this.source);
            if (!update)
                continue;
            const serialized = JSON.stringify(update);
            if (this.sent.get(id) === serialized)
                continue;
            await this.send({ version: 1, op: "update", download: update });
            this.sent.set(id, serialized);
        }
        const ids = JSON.stringify([...this.known]);
        if (ids !== this.savedIds) {
            await this.api.storage.local.set({ trackedIds: [...this.known] });
            this.savedIds = ids;
        }
    }
}

if (typeof module !== "undefined" && module.exports)
    module.exports = { DownloadBridge, snapshot };
else {
    const api = globalThis.browser || globalThis.chrome;
    globalThis.astreaDownloadBridge = new DownloadBridge(api, globalThis.ASTREA_BROWSER);
    void globalThis.astreaDownloadBridge.start();
}
