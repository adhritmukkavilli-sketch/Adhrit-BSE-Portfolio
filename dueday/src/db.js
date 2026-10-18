// Tiny JSON-file store. Good enough for a prototype with a handful of users;
// swap for a real database (SQLite/Postgres) before launching for real.
import fs from "node:fs";
import path from "node:path";

export function createDb(file) {
  let data = { users: {}, codes: {}, sessions: {} };
  if (file && fs.existsSync(file)) {
    data = { ...data, ...JSON.parse(fs.readFileSync(file, "utf8")) };
  }

  function save() {
    if (!file) return;
    fs.mkdirSync(path.dirname(file), { recursive: true });
    fs.writeFileSync(file + ".tmp", JSON.stringify(data, null, 2));
    fs.renameSync(file + ".tmp", file);
  }

  function getUser(email) {
    if (!data.users[email]) {
      data.users[email] = {
        email,
        settings: { timeZone: "America/New_York", summaryTime: "07:00" },
        sources: [],
        tasks: [],
      };
    }
    return data.users[email];
  }

  return { data, save, getUser };
}
