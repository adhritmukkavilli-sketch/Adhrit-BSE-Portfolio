import { createApp, checkEveryone } from "./app.js";
import { createDb } from "./db.js";
import { createExtractor } from "./extract.js";
import { safeFetch } from "./safeFetch.js";

const PORT = Number(process.env.PORT) || 3000;
const CHECK_EVERY_MS = (Number(process.env.CHECK_EVERY_HOURS) || 3) * 60 * 60 * 1000;

const db = createDb(process.env.DUEDAY_DB || "data/dueday.json");
const extractor = createExtractor();

// For now the login code is printed here. Hook up an email service
// (Resend, SendGrid, Amazon SES...) before real students use it.
async function sendCode(email, code) {
  console.log(`\n  Login code for ${email}: ${code}\n`);
}

const app = createApp({
  db,
  fetcher: safeFetch,
  extractor,
  sendCode,
  secureCookies: process.env.NODE_ENV === "production",
});

app.listen(PORT, () => {
  console.log(`DueDay running at http://localhost:${PORT}`);
  console.log(extractor.usesAI ? "AI reading: on" : "AI reading: off (set ANTHROPIC_API_KEY to turn it on)");
});

setInterval(() => {
  checkEveryone({ db, fetcher: safeFetch, extractor }).catch((err) => console.error("Background check failed:", err));
}, CHECK_EVERY_MS);
