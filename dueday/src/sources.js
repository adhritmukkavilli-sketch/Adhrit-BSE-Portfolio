// Checking the student's saved links and keeping their task list in sync.
import crypto from "node:crypto";
import { looksLikeIcal, parseIcal } from "./ical.js";
import { htmlToText } from "./extract.js";
import { todayIn } from "./dates.js";

export const newId = () => crypto.randomBytes(8).toString("hex");
const taskKey = (t) => `${t.title.toLowerCase()}|${t.due}`;

// Replace one source's tasks with a fresh list, keeping "done" checkmarks.
export function mergeTasks(user, sourceId, items, origin) {
  const old = new Map(user.tasks.filter((t) => t.sourceId === sourceId).map((t) => [taskKey(t), t]));
  const kept = user.tasks.filter((t) => t.sourceId !== sourceId);
  const fresh = items.map((it) => {
    const prev = old.get(taskKey(it));
    return {
      id: prev?.id ?? newId(),
      sourceId,
      origin,
      title: it.title,
      due: it.due,
      kind: it.kind,
      course: it.course || "",
      done: prev?.done ?? false,
      createdAt: prev?.createdAt ?? new Date().toISOString(),
    };
  });
  const added = fresh.filter((t) => !old.has(taskKey(t)));
  user.tasks = [...kept, ...fresh];
  return { added: added.length, total: fresh.length };
}

export async function checkSource(user, source, { fetcher, extractor, force = false }) {
  const { timeZone } = user.settings;
  const today = todayIn(timeZone);
  source.lastChecked = new Date().toISOString();
  try {
    const { body } = await fetcher(source.url);

    if (looksLikeIcal(body)) {
      source.kind = "calendar";
      const result = mergeTasks(user, source.id, parseIcal(body, timeZone), "calendar");
      source.error = null;
      source.itemCount = result.total;
      if (result.added && source.hash) source.changedAt = source.lastChecked;
      source.hash = "ical";
      return result;
    }

    source.kind = "page";
    const text = htmlToText(body);
    const hash = crypto.createHash("sha256").update(text).digest("hex");
    if (hash === source.hash && !force) {
      source.error = null;
      return { added: 0, total: source.itemCount ?? 0, unchanged: true };
    }

    const items = await extractor.fromText(text, { today, timeZone, sourceName: source.name || source.url });
    const result = mergeTasks(user, source.id, items, "page");
    // Only call it an "update" if we had seen this page before.
    if (source.hash && hash !== source.hash) source.changedAt = source.lastChecked;
    source.hash = hash;
    source.error = null;
    source.itemCount = result.total;
    return result;
  } catch (err) {
    source.error = err.message || "Couldn't check this link.";
    return { added: 0, total: 0, error: source.error };
  }
}
