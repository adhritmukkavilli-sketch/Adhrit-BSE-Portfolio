// Date helpers. Due dates are stored as plain "YYYY-MM-DD" in the student's time zone.

export function todayIn(timeZone, now = new Date()) {
  return dateIn(now, timeZone);
}

export function dateIn(date, timeZone) {
  return new Intl.DateTimeFormat("en-CA", {
    timeZone,
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
  }).format(date);
}

export function addDays(ymd, days) {
  const d = new Date(ymd + "T12:00:00Z");
  d.setUTCDate(d.getUTCDate() + days);
  return d.toISOString().slice(0, 10);
}

export function isValidYmd(s) {
  if (!/^\d{4}-\d{2}-\d{2}$/.test(s || "")) return false;
  const d = new Date(s + "T12:00:00Z");
  return !Number.isNaN(d.getTime()) && d.toISOString().slice(0, 10) === s;
}

export function guessKind(title) {
  const t = title.toLowerCase();
  if (/\b(test|exam|midterm|final)s?\b/.test(t)) return "test";
  if (/\bquiz(zes)?\b/.test(t)) return "quiz";
  if (/\bproject\b/.test(t)) return "project";
  return "assignment";
}
