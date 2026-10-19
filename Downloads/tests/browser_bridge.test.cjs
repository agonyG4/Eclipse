const assert = require("node:assert/strict");
const { test } = require("node:test");
const { DownloadBridge, snapshot } = require("../browser/background.js");

function item(extra = {}) {
    return { id: 7, filename: "/downloads/arquivo.zip", bytesReceived: 62,
             totalBytes: 100, state: "in_progress", paused: false,
             incognito: false, url: "https://private.invalid/token", ...extra };
}

function event() {
    const listeners = [];
    return { addListener(fn) { listeners.push(fn); },
             async fire(value) { await Promise.all(listeners.map(fn => fn(value))); } };
}

function fixture() {
    const items = new Map();
    const stored = {};
    const messages = [];
    const timers = new Map();
    const ports = [];
    let reconnected = false;
    let timerId = 0;
    const api = {
        storage: { local: { async get() { return { ...stored }; },
                             async set(value) { Object.assign(stored, value); } } },
        downloads: {
            onCreated: event(), onChanged: event(), onErased: event(),
            async search(query) {
                return [...items.values()].filter(x => query.id !== undefined
                    ? x.id === query.id : x.state === query.state);
            }
        },
        runtime: { connectNative() {
            const port = { onMessage: event(), onDisconnect: event(),
                disconnect() { void port.onDisconnect.fire(); },
                postMessage(message) {
                    messages.push(message);
                    const reconnect = reconnected;
                    reconnected = false;
                    queueMicrotask(() => port.onMessage.fire({ version: 1, ok: true, reconnected: reconnect }));
                }
            };
            ports.push(port);
            return port;
        } }
    };
    const options = {
        randomId: () => "profile-123", onError: message => { throw new Error(message); },
        setTimeout(fn, delay) { const id = ++timerId; timers.set(id, { fn, delay }); return id; },
        clearTimeout(id) { timers.delete(id); }
    };
    return { api, items, stored, messages, timers, options, ports,
        reconnect() { reconnected = true; },
        bridge: new DownloadBridge(api, "zen", options) };
}

test("maps browser states and unknown totals without sending browsing URLs", () => {
    const result = snapshot(item(), "zen:profile");
    assert.equal(result.state, "downloading");
    assert.equal(result.receivedBytes, 62);
    assert.equal(result.source, "zen:profile");
    assert.equal(result.id, "7");
    assert.equal(result.url, undefined);
    assert.equal(snapshot(item({ totalBytes: -1 }), "zen:p").totalBytes, null);
    assert.equal(snapshot(item({ paused: true }), "zen:p").state, "paused");
    assert.equal(snapshot(item({ state: "complete" }), "zen:p").state, "completed");
    assert.equal(snapshot(item({ state: "interrupted", error: "USER_CANCELED" }), "zen:p").state, "cancelled");
    assert.equal(snapshot(item({ state: "interrupted", error: "NETWORK_FAILED" }), "zen:p").state, "failed");
    assert.equal(snapshot(item({ incognito: true }), "zen:p"), null);
    assert.equal(snapshot(item({ filename: "" }), "zen:p"), null);
});

test("replays unchanged active and terminal records after API restart", async () => {
    const f = fixture();
    f.stored.trackedIds = [8];
    f.items.set(7, item());
    f.items.set(8, item({ id: 8, state: "complete" }));
    await f.bridge.start();
    const count = f.messages.filter(x => x.op === "update").length;
    f.reconnect();
    await f.bridge.scan();
    assert.equal(f.messages.filter(x => x.op === "update").length, count + 2);
    await f.ports[0].onDisconnect.fire();
    await f.bridge.scan();
    assert.equal(f.messages.filter(x => x.op === "update").length, count + 4);
    assert.equal(f.ports.length, 2);
});

test("ignores delayed replies from a disconnected native port", async () => {
    const f = fixture();
    f.items.set(7, item());
    await f.bridge.start();
    const oldPort = f.ports[0];
    const connect = f.api.runtime.connectNative;
    f.api.runtime.connectNative = () => {
        const port = connect();
        const post = port.postMessage;
        port.postMessage = message => {
            void oldPort.onMessage.fire({ ok: false, message: "stale reply" });
            post(message);
        };
        return port;
    };
    await oldPort.onDisconnect.fire();
    await f.bridge.scan();
    assert.equal(f.bridge.failed, false);
    assert.equal(f.ports.length, 2);
});

test("polls byte changes and publishes completion then erasure", async () => {
    const f = fixture();
    f.items.set(7, item());
    await f.bridge.start();
    let updates = f.messages.filter(x => x.op === "update");
    assert.equal(updates.at(-1).download.receivedBytes, 62);
    assert.ok([...f.timers.values()].some(x => x.delay === 1000));
    f.items.set(7, item({ bytesReceived: 90 }));
    await f.bridge.scan();
    updates = f.messages.filter(x => x.op === "update");
    assert.equal(updates.at(-1).download.receivedBytes, 90);
    f.items.set(7, item({ state: "complete", bytesReceived: 100 }));
    await f.api.downloads.onChanged.fire({ id: 7, state: { current: "complete" } });
    assert.equal(f.messages.filter(x => x.op === "update").at(-1).download.state, "completed");
    await f.api.downloads.onErased.fire(7);
    assert.equal(f.messages.at(-1).op, "remove");
    assert.equal(f.messages.at(-1).source, "zen:profile-123");
});

test("replays tracked records after reload without exporting old history", async () => {
    const f = fixture();
    f.stored.producerId = "existing-profile";
    f.stored.trackedIds = [7];
    f.items.set(7, item({ state: "complete" }));
    f.items.set(8, item({ id: 8, state: "complete", filename: "old-history.zip" }));
    await f.bridge.start();
    const updates = f.messages.filter(x => x.op === "update");
    assert.equal(updates.length, 1);
    assert.equal(updates[0].download.source, "zen:existing-profile");
    assert.equal(updates[0].download.id, "7");
    assert.equal(f.timers.size, 0);
});

test("skips private downloads and retries when a target name becomes available", async () => {
    const f = fixture();
    await f.bridge.start();
    f.items.set(7, item({ filename: "" }));
    await f.api.downloads.onCreated.fire(f.items.get(7));
    assert.equal(f.messages.filter(x => x.op === "update").length, 0);
    f.items.set(7, item());
    await f.api.downloads.onChanged.fire({ id: 7, filename: { current: "arquivo.zip" } });
    assert.equal(f.messages.filter(x => x.op === "update").length, 1);
    f.items.set(9, item({ id: 9, incognito: true }));
    await f.api.downloads.onCreated.fire(f.items.get(9));
    assert.equal(f.messages.filter(x => x.op === "update").length, 1);
    assert.ok(!f.stored.trackedIds.includes(9));
});
