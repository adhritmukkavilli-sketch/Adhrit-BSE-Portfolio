# DueDay

Every assignment from every class website in one list, with reminders.

Students log in with their email, paste in their teachers' websites (and/or the
calendar links from Canvas, Schoology or Google Classroom), and DueDay shows:

- **Due today** and **due tomorrow**
- **Tests & quizzes in the next 7 days**
- Everything else coming up, plus anything overdue

It re-checks every website every few hours and lets the student know when a
teacher's page changes. Students can also **snap a photo** of a syllabus,
worksheet or whiteboard to pull the due dates out of it, or add things by hand.

## Run it

```bash
cd dueday
npm install
npm start          # http://localhost:3000
```

The login code is printed in the terminal for now (no email service hooked up yet).

### Turn on AI reading (recommended)

Teacher websites all look different, so DueDay uses Claude to read them and to
read photos. Set an Anthropic API key before starting:

```bash
export ANTHROPIC_API_KEY=sk-ant-...
npm start
```

Without a key it still works with calendar links and manual tasks, and uses a
simple date-spotter on web pages (it finds lines like "Worksheet due 10/24"),
but photo scanning is off.

### Settings (environment variables)

| Variable | Default | What it does |
|---|---|---|
| `PORT` | `3000` | Web server port |
| `DUEDAY_DB` | `data/dueday.json` | Where accounts and tasks are saved |
| `CHECK_EVERY_HOURS` | `3` | How often every link is re-checked |
| `ANTHROPIC_API_KEY` | – | Turns on AI reading of pages and photos |

## Tests

```bash
npm test
```

## How it's built

| File | Job |
|---|---|
| `src/server.js` | Starts everything and runs the background re-checks |
| `src/app.js` | All the web routes (login, links, photos, tasks) |
| `src/auth.js` | Email + 6-digit-code login, sessions |
| `src/sources.js` | Checks a link and updates that link's tasks (keeps checkmarks) |
| `src/ical.js` | Reads Canvas / Schoology / Google calendar feeds |
| `src/extract.js` | Reads web pages and photos (Claude, or the simple fallback) |
| `src/safeFetch.js` | Downloads pasted links safely (blocks private network addresses, size/time limits) |
| `src/db.js` | Saves data to a JSON file |
| `public/` | The app students see (works on phones) |

## Before real students use it

- **Send login codes by email.** Hook `sendCode` in `src/server.js` up to an
  email service (Resend, SendGrid, Amazon SES).
- **Real database.** Move from the JSON file to SQLite or Postgres.
- **Reminders when the app is closed.** Reminders currently pop up while DueDay
  is open in a browser tab. Closed-app reminders need web push (a service worker)
  or a daily email/text.
- **Privacy.** It's an app for students, so write a clear privacy policy and
  check the rules for users under 13 (COPPA) before launching.
- **Logged-in pages.** Pages that need a school login can't be read by the
  watcher; students can use the calendar-feed link or a photo instead.
