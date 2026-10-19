import assert from "node:assert/strict";
import { existsSync } from "node:fs";
import test from "node:test";
const url = new URL("../zen/ZenDownloadPublisher.sys.mjs", import.meta.url);
assert.ok(existsSync(url), "Zen prototype publisher is missing");
const { downloadSnapshot, ProgressRecords } = await import(url);
const item = (extra = {}) => ({source:{isPrivate:false},target:{path:"/tmp/arquivo.zip",size:100},
  currentBytes:25,totalBytes:100,hasProgress:true,stopped:false,...extra});
test("private downloads are excluded", () => {
  assert.equal(downloadSnapshot(item({source:{isPrivate:true}}), "zen:test", "1"), null);
});
test("known and unknown sizes and final bytes", () => {
  assert.equal(downloadSnapshot(item(), "zen:test", "1").filename, "arquivo.zip");
  assert.equal(downloadSnapshot(item({hasProgress:false}), "zen:test", "1").totalBytes, null);
  const final = downloadSnapshot(item({succeeded:true}), "zen:test", "1");
  assert.equal(final.receivedBytes,100); assert.equal(final.state,"completed");
  assert.equal(downloadSnapshot(item({error:{}}),"zen:test","1").state,"failed");
  assert.equal(downloadSnapshot(item({canceled:true,hasPartialData:true}),"zen:test","1").state,"paused");
  assert.equal(downloadSnapshot(item({canceled:true}),"zen:test","1").state,"cancelled");
});
test("history filtering, coalescing, stable IDs, removal and replay", () => {
  const records = new ProgressRecords("zen:test");
  records.add(item({stopped:true,succeeded:true}),true); assert.equal(records.records.size,0);
  const d=item();records.add(d,true); const first=[...records.pending.values()][0];
  records.acknowledge(first);assert.equal(records.pending.size,0);
  d.currentBytes=50;records.change(d);d.currentBytes=75;records.change(d);
  assert.equal(records.pending.size,1);const latest=[...records.pending.values()][0];
  assert.equal(latest.download.receivedBytes,75);assert.equal(latest.download.id,first.download.id);
  records.acknowledge(first);assert.equal(records.pending.size,1);
  records.acknowledge(latest);records.replay();assert.equal(records.pending.size,1);
  records.remove(d);assert.equal(records.records.size,0);assert.equal([...records.pending.values()][0].op,"remove");
});
test("retention and pending work stay bounded", () => {
  const records = new ProgressRecords("zen:test",2);
  for(let i=0;i<30;i++)records.add(item({succeeded:true,stopped:true}));
  assert.equal(records.records.size,2);assert.ok(records.pending.size<=4);
  const active = new ProgressRecords("zen:test",2);
  active.add(item());active.add(item());active.add(item());assert.equal(active.records.size,2);
});

test("completed records are evicted before paused transfers", () => {
  const records = new ProgressRecords("zen:test",2);
  const paused=item({stopped:true,canceled:true,hasPartialData:true});
  const completed=item({stopped:true,succeeded:true});
  records.add(paused);records.add(completed);records.add(item());
  assert.ok(records.records.has(records.ids.get(paused)));
  assert.ok(!records.records.has(records.ids.get(completed)));
});
