// Minimal iCalendar (.ics) reader for the calendar feeds that Canvas, Schoology,
// Google Classroom (via Google Calendar) and others publish.
import { dateIn, guessKind } from "./dates.js";

export function looksLikeIcal(body) {
  return /^\s*BEGIN:VCALENDAR/i.test(body);
}

function unfold(text) {
  // Long lines are continued on the next line starting with a space or tab.
  return text.replace(/\r\n/g, "\n").replace(/\n[ \t]/g, "");
}

function unescapeText(v) {
  return v.replace(/\\n/gi, " ").replace(/\\([,;\\])/g, "$1").trim();
}

function parseLine(line) {
  const colon = line.indexOf(":");
  if (colon < 0) return null;
  const name = line.slice(0, colon).split(";")[0];
  return { name: name.toUpperCase(), value: line.slice(colon + 1) };
}

// Returns "YYYY-MM-DD" in the student's time zone.
export function icalDate(value, timeZone) {
  const m = value.match(/^(\d{4})(\d{2})(\d{2})(?:T(\d{2})(\d{2})(\d{2})(Z)?)?$/);
  if (!m) return null;
  const [, y, mo, d, h, mi, s, z] = m;
  if (z) {
    return dateIn(new Date(Date.UTC(+y, +mo - 1, +d, +h, +mi, +s)), timeZone);
  }
  // All-day dates and local/TZID times: the written date is the due date.
  return `${y}-${mo}-${d}`;
}

export function parseIcal(text, timeZone) {
  const items = [];
  let cur = null;
  for (const raw of unfold(text).split("\n")) {
    const line = raw.trimEnd();
    if (/^BEGIN:(VEVENT|VTODO)$/i.test(line)) {
      cur = {};
      continue;
    }
    if (/^END:(VEVENT|VTODO)$/i.test(line)) {
      if (cur && cur.title && cur.due) items.push(cur);
      cur = null;
      continue;
    }
    if (!cur) continue;
    const p = parseLine(line);
    if (!p) continue;
    if (p.name === "SUMMARY") cur.title = unescapeText(p.value);
    // A VTODO's DUE wins over DTSTART; for events DTSTART is the deadline.
    if (p.name === "DUE" || (p.name === "DTSTART" && !cur.due)) {
      cur.due = icalDate(p.value.trim(), timeZone);
    }
  }

  return items.map(({ title, due }) => {
    // Canvas puts the course in brackets: "Essay 2 [ENG 101]"
    const m = title.match(/^(.*?)\s*\[([^\]]+)\]\s*$/);
    const clean = m ? m[1] : title;
    return { title: clean, course: m ? m[2] : "", due, kind: guessKind(clean) };
  });
}
