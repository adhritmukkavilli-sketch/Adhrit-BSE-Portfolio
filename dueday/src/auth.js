// Email login with a one-time 6-digit code. No passwords to store or leak.
import crypto from "node:crypto";

const CODE_TTL_MS = 10 * 60 * 1000;
const SESSION_TTL_MS = 30 * 24 * 60 * 60 * 1000;
const RESEND_WAIT_MS = 30 * 1000;
const MAX_ATTEMPTS = 5;

const sha256 = (s) => crypto.createHash("sha256").update(s).digest("hex");

export function normalizeEmail(email) {
  const e = String(email || "").trim().toLowerCase();
  return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(e) && e.length <= 254 ? e : null;
}

export async function requestCode(db, rawEmail, sendCode, now = Date.now()) {
  const email = normalizeEmail(rawEmail);
  if (!email) return { error: "Enter a valid email address." };

  const existing = db.data.codes[email];
  if (existing && now - existing.sentAt < RESEND_WAIT_MS) {
    return { error: "Wait a few seconds before asking for another code." };
  }

  const code = String(crypto.randomInt(0, 1_000_000)).padStart(6, "0");
  db.data.codes[email] = {
    hash: sha256(code),
    sentAt: now,
    expiresAt: now + CODE_TTL_MS,
    attempts: 0,
  };
  db.save();
  await sendCode(email, code);
  return { ok: true };
}

export function verifyCode(db, rawEmail, code, now = Date.now()) {
  const email = normalizeEmail(rawEmail);
  const entry = email && db.data.codes[email];
  if (!entry || entry.expiresAt < now) return { error: "Code expired. Ask for a new one." };

  entry.attempts += 1;
  if (entry.attempts > MAX_ATTEMPTS) {
    delete db.data.codes[email];
    db.save();
    return { error: "Too many tries. Ask for a new code." };
  }

  const given = Buffer.from(sha256(String(code || "").trim()));
  if (!crypto.timingSafeEqual(given, Buffer.from(entry.hash))) {
    db.save();
    return { error: "That code didn't match." };
  }

  delete db.data.codes[email];
  const token = crypto.randomBytes(32).toString("hex");
  db.data.sessions[sha256(token)] = { email, expiresAt: now + SESSION_TTL_MS };
  db.getUser(email);
  db.save();
  return { ok: true, token, email };
}

export function sessionEmail(db, token, now = Date.now()) {
  if (!token) return null;
  const s = db.data.sessions[sha256(token)];
  if (!s || s.expiresAt < now) return null;
  return s.email;
}

export function endSession(db, token) {
  if (token) {
    delete db.data.sessions[sha256(token)];
    db.save();
  }
}
