import test from "node:test";
import assert from "node:assert/strict";
import { createApp } from "../src/app.js";
import { createDb } from "../src/db.js";

// Runs the real server with fake websites and a fake AI reader.
async function startServer() {
  const db = createDb(null);
  const codes = {};
  const pages = {
    "https://teacher.example/homework": "<h1>Homework</h1><p>Worksheet 4 due 10/24</p>",
    "https://canvas.example/feed.ics":
      "BEGIN:VCALENDAR\nBEGIN:VEVENT\nSUMMARY:Unit Test [Bio]\nDTSTART;VALUE=DATE:20261021\nEND:VEVENT\nEND:VCALENDAR",
  };
  const extractor = {
    usesAI: true,
    async fromText(text) {
      return text.includes("Worksheet") ? [{ title: "Worksheet 4", due: "2026-10-24", kind: "assignment", course: "" }] : [];
    },
    async fromImage() {
      return [{ title: "Chapter 6 quiz", due: "2026-10-23", kind: "quiz", course: "Math" }];
    },
  };
  const app = createApp({
    db,
    extractor,
    fetcher: async (url) => {
      const key = new URL(url.startsWith("http") ? url : "https://" + url).toString();
      if (!(key in pages)) throw new Error("The site answered with error 404.");
      return { url: key, body: pages[key] };
    },
    sendCode: async (email, code) => {
      codes[email] = code;
    },
  });
  const server = await new Promise((resolve) => {
    const s = app.listen(0, () => resolve(s));
  });
  const base = `http://127.0.0.1:${server.address().port}`;
  let cookie = "";
  const call = async (path, method = "GET", body) => {
    const res = await fetch(base + path, {
      method,
      headers: { "content-type": "application/json", cookie },
      body: body ? JSON.stringify(body) : undefined,
    });
    const set = res.headers.get("set-cookie");
    if (set) cookie = set.split(";")[0];
    return { status: res.status, data: await res.json() };
  };
  return { server, call, codes, pages };
}

test("full student flow", async (t) => {
  const { server, call, codes, pages } = await startServer();
  t.after(() => server.close());

  assert.equal((await call("/api/me")).status, 401);

  // Log in with an email code
  assert.equal((await call("/api/auth/request", "POST", { email: "Student@School.org" })).status, 200);
  assert.equal((await call("/api/auth/verify", "POST", { email: "student@school.org", code: "000000x" })).status, 400);
  const login = await call("/api/auth/verify", "POST", {
    email: "student@school.org",
    code: codes["student@school.org"],
    timeZone: "America/Chicago",
  });
  assert.equal(login.status, 200);
  const me = await call("/api/me");
  assert.equal(me.data.email, "student@school.org");
  assert.equal(me.data.settings.timeZone, "America/Chicago");

  // Add a teacher website and a calendar feed
  const page = await call("/api/sources", "POST", { url: "teacher.example/homework", name: "Science" });
  assert.equal(page.data.result.total, 1);
  const feed = await call("/api/sources", "POST", { url: "webcal://canvas.example/feed.ics" });
  assert.equal(feed.data.sources[1].kind, "calendar");
  assert.equal(feed.data.tasks.length, 2);
  assert.equal(feed.data.sources[0].hash, undefined, "internal fields stay private");

  // Duplicate and broken links
  assert.equal((await call("/api/sources", "POST", { url: "https://teacher.example/homework" })).status, 400);
  const broken = await call("/api/sources", "POST", { url: "https://nope.example/" });
  assert.match(broken.data.result.error, /404/);

  // Check off, then the teacher updates the page: checkmark stays, new item flagged
  const worksheet = feed.data.tasks.find((x) => x.title === "Worksheet 4");
  await call(`/api/tasks/${worksheet.id}`, "PATCH", { done: true });
  pages["https://teacher.example/homework"] += "<p>Also: Worksheet 5 due 10/28</p>";
  const sourceId = page.data.sources[0].id;
  const after = await call(`/api/sources/${sourceId}/check`, "POST");
  assert.ok(after.data.sources[0].changedAt);
  assert.equal(after.data.tasks.find((x) => x.title === "Worksheet 4").done, true);

  // Photo scan
  const photo = await call("/api/photo", "POST", { image: "data:image/jpeg;base64,AAAA" });
  assert.equal(photo.data.result.total, 1);
  assert.equal((await call("/api/photo", "POST", { image: "data:text/html;base64,AAAA" })).status, 400);

  // Manual task + validation
  assert.equal((await call("/api/tasks", "POST", { title: "Read ch 6", due: "2026-02-31" })).status, 400);
  const manual = await call("/api/tasks", "POST", { title: "Read ch 6", due: "2026-10-30" });
  assert.ok(manual.data.tasks.some((x) => x.origin === "manual"));

  // Remove a source and its tasks
  const removed = await call(`/api/sources/${sourceId}`, "DELETE");
  assert.ok(!removed.data.tasks.some((x) => x.sourceId === sourceId));

  // Log out
  await call("/api/auth/logout", "POST");
  assert.equal((await call("/api/me")).status, 401);
});

test("login codes expire after too many wrong tries", async (t) => {
  const { server, call, codes } = await startServer();
  t.after(() => server.close());
  await call("/api/auth/request", "POST", { email: "a@b.co" });
  for (let i = 0; i < 5; i++) await call("/api/auth/verify", "POST", { email: "a@b.co", code: "999999x" });
  const r = await call("/api/auth/verify", "POST", { email: "a@b.co", code: codes["a@b.co"] });
  assert.equal(r.status, 400);
});
