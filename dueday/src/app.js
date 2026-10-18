import express from "express";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { endSession, requestCode, sessionEmail, verifyCode } from "./auth.js";
import { checkSource, mergeTasks, newId } from "./sources.js";
import { normalizeUrl } from "./safeFetch.js";
import { guessKind, isValidYmd, todayIn } from "./dates.js";

const here = path.dirname(fileURLToPath(import.meta.url));
const COOKIE = "dueday_session";
const MAX_SOURCES = 25;
const IMAGE_TYPES = ["image/jpeg", "image/png", "image/webp", "image/gif"];

function readCookie(req, name) {
  for (const part of (req.headers.cookie || "").split(";")) {
    const [k, ...v] = part.trim().split("=");
    if (k === name) return decodeURIComponent(v.join("="));
  }
  return null;
}

function isValidTimeZone(tz) {
  try {
    new Intl.DateTimeFormat("en-US", { timeZone: tz });
    return true;
  } catch {
    return false;
  }
}

function publicUser(user, usesAI) {
  return {
    email: user.email,
    settings: user.settings,
    usesAI,
    sources: user.sources.map(({ hash, ...s }) => s),
    tasks: user.tasks,
  };
}

export function createApp({ db, fetcher, extractor, sendCode, secureCookies = false }) {
  const app = express();
  app.use(express.json({ limit: "8mb" }));
  app.use(express.static(path.join(here, "..", "public")));

  // ----- Login -----

  app.post("/api/auth/request", async (req, res) => {
    const result = await requestCode(db, req.body?.email, sendCode);
    res.status(result.error ? 400 : 200).json(result);
  });

  app.post("/api/auth/verify", (req, res) => {
    const result = verifyCode(db, req.body?.email, req.body?.code);
    if (result.error) return res.status(400).json(result);
    const tz = req.body?.timeZone;
    if (tz && isValidTimeZone(tz)) {
      db.getUser(result.email).settings.timeZone = tz;
      db.save();
    }
    res.cookie(COOKIE, result.token, {
      httpOnly: true,
      sameSite: "lax",
      secure: secureCookies,
      maxAge: 30 * 24 * 60 * 60 * 1000,
    });
    res.json({ ok: true });
  });

  app.post("/api/auth/logout", (req, res) => {
    endSession(db, readCookie(req, COOKIE));
    res.clearCookie(COOKIE);
    res.json({ ok: true });
  });

  // Everything below needs a logged-in student.
  app.use("/api", (req, res, next) => {
    const email = sessionEmail(db, readCookie(req, COOKIE));
    if (!email) return res.status(401).json({ error: "Please log in." });
    req.user = db.getUser(email);
    next();
  });

  app.get("/api/me", (req, res) => res.json(publicUser(req.user, extractor.usesAI)));

  app.put("/api/settings", (req, res) => {
    const { timeZone, summaryTime } = req.body || {};
    if (timeZone !== undefined) {
      if (!isValidTimeZone(timeZone)) return res.status(400).json({ error: "Unknown time zone." });
      req.user.settings.timeZone = timeZone;
    }
    if (summaryTime !== undefined) {
      if (!/^([01]\d|2[0-3]):[0-5]\d$/.test(summaryTime)) return res.status(400).json({ error: "Use HH:MM." });
      req.user.settings.summaryTime = summaryTime;
    }
    db.save();
    res.json(publicUser(req.user, extractor.usesAI));
  });

  // ----- Websites / calendar links -----

  app.post("/api/sources", async (req, res) => {
    const url = normalizeUrl(req.body?.url);
    if (!url) return res.status(400).json({ error: "That doesn't look like a website link." });
    if (req.user.sources.length >= MAX_SOURCES) return res.status(400).json({ error: "That's the most links for now." });
    if (req.user.sources.some((s) => s.url === url.toString())) {
      return res.status(400).json({ error: "You already added that link." });
    }
    const source = {
      id: newId(),
      url: url.toString(),
      name: String(req.body?.name || "").trim().slice(0, 60) || url.hostname,
      addedAt: new Date().toISOString(),
    };
    req.user.sources.push(source);
    const result = await checkSource(req.user, source, { fetcher, extractor });
    db.save();
    res.json({ result, ...publicUser(req.user, extractor.usesAI) });
  });

  app.delete("/api/sources/:id", (req, res) => {
    req.user.sources = req.user.sources.filter((s) => s.id !== req.params.id);
    req.user.tasks = req.user.tasks.filter((t) => t.sourceId !== req.params.id);
    db.save();
    res.json(publicUser(req.user, extractor.usesAI));
  });

  app.post("/api/sources/:id/check", async (req, res) => {
    const source = req.user.sources.find((s) => s.id === req.params.id);
    if (!source) return res.status(404).json({ error: "Not found." });
    const result = await checkSource(req.user, source, { fetcher, extractor, force: true });
    db.save();
    res.json({ result, ...publicUser(req.user, extractor.usesAI) });
  });

  app.post("/api/check-all", async (req, res) => {
    for (const source of req.user.sources) {
      await checkSource(req.user, source, { fetcher, extractor });
    }
    db.save();
    res.json(publicUser(req.user, extractor.usesAI));
  });

  // ----- Photo scan -----

  app.post("/api/photo", async (req, res) => {
    const m = String(req.body?.image || "").match(/^data:([a-z/+]+);base64,([A-Za-z0-9+/=]+)$/);
    if (!m || !IMAGE_TYPES.includes(m[1])) return res.status(400).json({ error: "Send a JPG or PNG photo." });
    try {
      const { timeZone } = req.user.settings;
      const items = await extractor.fromImage({ mediaType: m[1], base64: m[2] }, { today: todayIn(timeZone), timeZone });
      // Each photo becomes its own little source so re-scans don't wipe each other.
      const photoId = "photo-" + newId();
      const result = mergeTasks(req.user, photoId, items, "photo");
      db.save();
      res.json({ result, ...publicUser(req.user, extractor.usesAI) });
    } catch (err) {
      res.status(400).json({ error: err.message || "Couldn't read that photo." });
    }
  });

  // ----- Tasks -----

  app.post("/api/tasks", (req, res) => {
    const title = String(req.body?.title || "").trim().slice(0, 140);
    const due = req.body?.due;
    if (!title || !isValidYmd(due)) return res.status(400).json({ error: "Add a title and a due date." });
    const kind = ["assignment", "test", "quiz", "project"].includes(req.body?.kind) ? req.body.kind : guessKind(title);
    req.user.tasks.push({
      id: newId(),
      sourceId: null,
      origin: "manual",
      title,
      due,
      kind,
      course: String(req.body?.course || "").trim().slice(0, 60),
      done: false,
      createdAt: new Date().toISOString(),
    });
    db.save();
    res.json(publicUser(req.user, extractor.usesAI));
  });

  app.patch("/api/tasks/:id", (req, res) => {
    const task = req.user.tasks.find((t) => t.id === req.params.id);
    if (!task) return res.status(404).json({ error: "Not found." });
    if (typeof req.body?.done === "boolean") task.done = req.body.done;
    db.save();
    res.json(publicUser(req.user, extractor.usesAI));
  });

  app.delete("/api/tasks/:id", (req, res) => {
    req.user.tasks = req.user.tasks.filter((t) => t.id !== req.params.id);
    db.save();
    res.json(publicUser(req.user, extractor.usesAI));
  });

  return app;
}

// Re-check every student's links in the background.
export async function checkEveryone({ db, fetcher, extractor }) {
  for (const user of Object.values(db.data.users)) {
    for (const source of user.sources) {
      await checkSource(user, source, { fetcher, extractor });
    }
  }
  db.save();
}
