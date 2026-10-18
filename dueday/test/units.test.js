import test from "node:test";
import assert from "node:assert/strict";
import { parseIcal, looksLikeIcal } from "../src/ical.js";
import { htmlToText, simpleExtract } from "../src/extract.js";
import { isPrivateAddress, normalizeUrl } from "../src/safeFetch.js";
import { guessKind, isValidYmd, addDays } from "../src/dates.js";
import { mergeTasks } from "../src/sources.js";

const ICS = [
  "BEGIN:VCALENDAR",
  "BEGIN:VEVENT",
  "SUMMARY:Essay 2 draft [ENG 101]",
  "DTSTART;VALUE=DATE:20261020",
  "END:VEVENT",
  "BEGIN:VEVENT",
  "SUMMARY:Unit 3 Test [Chemistry]",
  "DTSTART:20261023T035900Z",
  "END:VEVENT",
  "BEGIN:VEVENT",
  "SUMMARY:A very long title that the calendar",
  "  folds onto two lines",
  "DTSTART;TZID=America/Chicago:20261025T090000",
  "END:VEVENT",
  "END:VCALENDAR",
].join("\r\n");

test("reads calendar feeds", () => {
  assert.ok(looksLikeIcal(ICS));
  const items = parseIcal(ICS, "America/New_York");
  assert.deepEqual(items[0], { title: "Essay 2 draft", course: "ENG 101", due: "2026-10-20", kind: "assignment" });
  // 03:59 UTC on the 23rd is still the evening of the 22nd in New York.
  assert.equal(items[1].due, "2026-10-22");
  assert.equal(items[1].kind, "test");
  assert.equal(items[2].title, "A very long title that the calendar folds onto two lines");
  assert.equal(items[2].due, "2026-10-25");
});

test("turns HTML into readable text", () => {
  const text = htmlToText("<html><head><title>x</title></head><body><script>bad()</script><h1>Homework</h1><p>Lab &amp; worksheet<br>due 10/24</p></body></html>");
  assert.equal(text, "Homework\nLab & worksheet\ndue 10/24");
});

test("spots dates without AI", () => {
  const text = "Welcome to class!\nWorksheet 4 due 10/24\nChapter 5 quiz - Oct 27\nUnit test on 2026-11-02\nHave a nice weekend";
  const items = simpleExtract(text, "2026-10-18");
  assert.deepEqual(
    items.map((i) => [i.due, i.kind]),
    [
      ["2026-10-24", "assignment"],
      ["2026-10-27", "quiz"],
      ["2026-11-02", "test"],
    ],
  );
});

test("picks the nearest year for dates without one", () => {
  const items = simpleExtract("Project due Jan 5", "2026-12-20");
  assert.equal(items[0].due, "2027-01-05");
});

test("blocks private network addresses", () => {
  for (const ip of ["127.0.0.1", "10.1.2.3", "172.20.0.1", "192.168.1.1", "169.254.169.254", "::1", "fd00::1", "::ffff:127.0.0.1"]) {
    assert.ok(isPrivateAddress(ip), ip);
  }
  for (const ip of ["8.8.8.8", "142.250.72.14", "2607:f8b0::1"]) assert.ok(!isPrivateAddress(ip), ip);
});

test("cleans up pasted links", () => {
  assert.equal(normalizeUrl("mrsmith.weebly.com").toString(), "https://mrsmith.weebly.com/");
  assert.equal(normalizeUrl("webcal://canvas.school.edu/feeds/x.ics").protocol, "https:");
  assert.equal(normalizeUrl("file:///etc/passwd"), null);
  assert.equal(normalizeUrl("javascript:alert(1)"), null);
});

test("date helpers", () => {
  assert.ok(isValidYmd("2026-02-28"));
  assert.ok(!isValidYmd("2026-02-30"));
  assert.equal(addDays("2026-12-31", 1), "2027-01-01");
  assert.equal(guessKind("Midterm exam"), "test");
  assert.equal(guessKind("Pop quiz"), "quiz");
});

test("re-checking a page keeps checkmarks", () => {
  const user = { tasks: [] };
  mergeTasks(user, "s1", [{ title: "Worksheet", due: "2026-10-20", kind: "assignment" }], "page");
  user.tasks[0].done = true;
  const r = mergeTasks(
    user,
    "s1",
    [
      { title: "Worksheet", due: "2026-10-20", kind: "assignment" },
      { title: "Quiz 3", due: "2026-10-22", kind: "quiz" },
    ],
    "page",
  );
  assert.equal(r.added, 1);
  assert.equal(user.tasks.find((t) => t.title === "Worksheet").done, true);
});
