const $ = (id) => document.getElementById(id);
let me = null;

// ---------- helpers ----------

async function api(path, options = {}) {
  const res = await fetch(path, {
    method: options.method || "GET",
    headers: options.body ? { "content-type": "application/json" } : {},
    body: options.body ? JSON.stringify(options.body) : undefined,
  });
  const data = await res.json().catch(() => ({}));
  if (res.status === 401 && path !== "/api/me") showLogin();
  if (!res.ok) throw new Error(data.error || "Something went wrong.");
  return data;
}

function store(key, value) {
  try {
    if (value === undefined) return localStorage.getItem(key);
    localStorage.setItem(key, value);
  } catch {
    return null;
  }
}

const localYmd = (d = new Date()) => d.toLocaleDateString("en-CA");
function addDays(ymd, n) {
  const d = new Date(ymd + "T12:00:00");
  d.setDate(d.getDate() + n);
  return localYmd(d);
}
function niceDate(ymd) {
  return new Date(ymd + "T12:00:00").toLocaleDateString(undefined, { weekday: "short", month: "short", day: "numeric" });
}
function daysBetween(a, b) {
  return Math.round((new Date(b + "T12:00:00") - new Date(a + "T12:00:00")) / 86400000);
}
function setMsg(el, text, isError = false) {
  el.textContent = text;
  el.classList.toggle("error", isError);
}

// ---------- login ----------

function showLogin() {
  $("app").hidden = true;
  $("login").hidden = false;
}

$("email-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const email = $("email").value;
  try {
    await api("/api/auth/request", { method: "POST", body: { email } });
    $("code-email").textContent = email;
    $("email-form").hidden = true;
    $("code-form").hidden = false;
    $("code").focus();
    setMsg($("login-msg"), "Code sent. Check your email.");
  } catch (err) {
    setMsg($("login-msg"), err.message, true);
  }
});

$("code-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  try {
    await api("/api/auth/verify", {
      method: "POST",
      body: {
        email: $("email").value,
        code: $("code").value,
        timeZone: Intl.DateTimeFormat().resolvedOptions().timeZone,
      },
    });
    setMsg($("login-msg"), "");
    await load();
  } catch (err) {
    setMsg($("login-msg"), err.message, true);
  }
});

$("code-back").addEventListener("click", () => {
  $("code-form").hidden = true;
  $("email-form").hidden = false;
});

$("logout-btn").addEventListener("click", async () => {
  await api("/api/auth/logout", { method: "POST" });
  me = null;
  showLogin();
});

// ---------- rendering ----------

function taskItem(t, today) {
  const li = document.createElement("li");
  if (t.done) li.classList.add("done");

  const box = document.createElement("input");
  box.type = "checkbox";
  box.checked = t.done;
  box.setAttribute("aria-label", "Done: " + t.title);
  box.addEventListener("change", () => update(api(`/api/tasks/${t.id}`, { method: "PATCH", body: { done: box.checked } })));

  const tag = document.createElement("span");
  tag.className = "tag " + t.kind;
  tag.textContent = t.kind;

  const title = document.createElement("div");
  title.className = "title";
  title.textContent = t.title;
  const meta = document.createElement("div");
  meta.className = "meta";
  const source = me.sources.find((s) => s.id === t.sourceId);
  const from = source ? source.name : t.origin === "photo" ? "photo" : t.origin === "manual" ? "added by you" : "";
  const left = daysBetween(today, t.due);
  const when = left === 0 ? "today" : left === 1 ? "tomorrow" : left < 0 ? `${-left} day(s) late` : niceDate(t.due);
  meta.textContent = [t.course, when, from].filter(Boolean).join(" · ");
  title.appendChild(meta);

  const del = document.createElement("button");
  del.className = "icon-btn";
  del.textContent = "✕";
  del.title = "Remove";
  del.setAttribute("aria-label", "Remove " + t.title);
  del.addEventListener("click", () => update(api(`/api/tasks/${t.id}`, { method: "DELETE" })));

  li.append(box, tag, title, del);
  return li;
}

function fill(listId, tasks, today, emptyText) {
  const ul = $(listId);
  ul.replaceChildren();
  if (!tasks.length) {
    const li = document.createElement("li");
    li.className = "empty";
    li.textContent = emptyText;
    ul.appendChild(li);
    return;
  }
  for (const t of tasks) ul.appendChild(taskItem(t, today));
}

function render() {
  const today = localYmd();
  const tomorrow = addDays(today, 1);
  const weekOut = addDays(today, 7);
  const byDate = [...me.tasks].sort((a, b) => a.due.localeCompare(b.due) || a.title.localeCompare(b.title));
  const isTest = (t) => t.kind === "test" || t.kind === "quiz";

  $("today-label").textContent = new Date().toLocaleDateString(undefined, { weekday: "long", month: "long", day: "numeric" });
  fill("list-today", byDate.filter((t) => t.due === today), today, "Nothing due today 🎉");
  fill("list-tomorrow", byDate.filter((t) => t.due === tomorrow), today, "Nothing due tomorrow.");
  fill("list-tests", byDate.filter((t) => isTest(t) && !t.done && t.due >= today && t.due <= weekOut), today, "No tests or quizzes this week.");
  fill("list-later", byDate.filter((t) => t.due > tomorrow && !(isTest(t) && t.due <= weekOut)), today, "Nothing else yet. Add a class website below.");
  fill("list-overdue", byDate.filter((t) => t.due < today && !t.done), today, "You're all caught up.");

  const ul = $("sources");
  ul.replaceChildren();
  for (const s of me.sources) {
    const li = document.createElement("li");
    const info = document.createElement("div");
    info.className = "info";
    const name = document.createElement("b");
    name.textContent = s.name;
    const meta = document.createElement("div");
    meta.className = "meta";
    const checked = s.lastChecked ? "checked " + new Date(s.lastChecked).toLocaleString() : "not checked yet";
    meta.textContent = `${s.kind === "calendar" ? "Calendar feed" : "Web page"} · ${s.itemCount ?? 0} items · ${checked}`;
    info.append(name, meta);
    if (s.error) {
      const err = document.createElement("div");
      err.className = "err";
      err.textContent = s.error;
      info.appendChild(err);
    }
    const check = document.createElement("button");
    check.className = "ghost small";
    check.textContent = "Check";
    check.addEventListener("click", () => update(api(`/api/sources/${s.id}/check`, { method: "POST" }), "Checked."));
    const del = document.createElement("button");
    del.className = "icon-btn";
    del.textContent = "✕";
    del.setAttribute("aria-label", "Remove " + s.name);
    del.addEventListener("click", () => {
      if (confirm(`Remove ${s.name} and its assignments?`)) update(api(`/api/sources/${s.id}`, { method: "DELETE" }));
    });
    li.append(info, check, del);
    ul.appendChild(li);
  }

  $("summary-time").value = me.settings.summaryTime;
  $("notify-btn").hidden = !("Notification" in window) || Notification.permission === "granted";
}

async function update(promise, okText = "") {
  try {
    const data = await promise;
    me = data;
    render();
    if (okText) setMsg($("app-msg"), okText);
    return data;
  } catch (err) {
    setMsg($("app-msg"), err.message, true);
  }
}

async function load() {
  try {
    me = await api("/api/me");
  } catch {
    return showLogin();
  }
  $("login").hidden = true;
  $("app").hidden = false;
  render();
  tick();
}

// ---------- adding things ----------

$("source-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  setMsg($("app-msg"), "Reading that page…");
  const data = await update(api("/api/sources", { method: "POST", body: { url: $("source-url").value, name: $("source-name").value } }));
  if (!data) return;
  const r = data.result;
  if (r.error) setMsg($("app-msg"), `Added, but: ${r.error}`, true);
  else setMsg($("app-msg"), `Added. Found ${r.total} item(s).${me.usesAI ? "" : " (AI reading is off, so some may be missed.)"}`);
  $("source-url").value = "";
  $("source-name").value = "";
});

$("check-all").addEventListener("click", () => {
  setMsg($("app-msg"), "Checking all your links…");
  update(api("/api/check-all", { method: "POST" }), "All links checked.");
});

$("task-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const ok = await update(
    api("/api/tasks", { method: "POST", body: { title: $("task-title").value, due: $("task-due").value, kind: $("task-kind").value } }),
    "Added.",
  );
  if (ok) $("task-title").value = "";
});

$("summary-time").addEventListener("change", (e) => {
  update(api("/api/settings", { method: "PUT", body: { summaryTime: e.target.value } }), "Saved.");
});

// Shrink photos before upload so they're quick to send and read.
function shrink(file, maxSide = 1600) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => {
      const scale = Math.min(1, maxSide / Math.max(img.width, img.height));
      const canvas = document.createElement("canvas");
      canvas.width = Math.round(img.width * scale);
      canvas.height = Math.round(img.height * scale);
      canvas.getContext("2d").drawImage(img, 0, 0, canvas.width, canvas.height);
      URL.revokeObjectURL(img.src);
      resolve(canvas.toDataURL("image/jpeg", 0.85));
    };
    img.onerror = () => reject(new Error("Couldn't open that image."));
    img.src = URL.createObjectURL(file);
  });
}

$("photo").addEventListener("change", async (e) => {
  const file = e.target.files[0];
  if (!file) return;
  setMsg($("photo-msg"), "Reading your photo…");
  try {
    const image = await shrink(file);
    const data = await update(api("/api/photo", { method: "POST", body: { image } }));
    if (data) setMsg($("photo-msg"), `Found ${data.result.total} item(s).`);
  } catch (err) {
    setMsg($("photo-msg"), err.message, true);
  }
  e.target.value = "";
});

// ---------- reminders ----------

$("notify-btn").addEventListener("click", async () => {
  const result = await Notification.requestPermission();
  if (result === "granted") {
    notify("Reminders are on", "We'll remind you about tests and what's due each morning while DueDay is open.");
  }
  render();
});

function notify(title, body) {
  if (!("Notification" in window) || Notification.permission !== "granted") return;
  new Notification(title, { body });
}

// Run each reminder only once by remembering it in this browser.
function once(key, fn) {
  if (store("dueday-" + key)) return;
  store("dueday-" + key, "1");
  fn();
}

function tick() {
  if (!me) return;
  const today = localYmd();
  const open = me.tasks.filter((t) => !t.done);

  const now = new Date();
  const hhmm = `${String(now.getHours()).padStart(2, "0")}:${String(now.getMinutes()).padStart(2, "0")}`;
  if (hhmm >= me.settings.summaryTime) {
    once(`summary-${today}`, () => {
      const due = open.filter((t) => t.due === today);
      const body = due.length ? due.map((t) => t.title).slice(0, 5).join(", ") : "Nothing due today.";
      notify(`Due today: ${due.length}`, body);
    });
  }

  for (const t of open) {
    if (t.kind !== "test" && t.kind !== "quiz") continue;
    const left = daysBetween(today, t.due);
    if (left === 3 || left === 1 || left === 0) {
      const when = left === 0 ? "today" : left === 1 ? "tomorrow" : "in 3 days";
      once(`test-${t.id}-${left}`, () => notify(`${t.kind === "test" ? "Test" : "Quiz"} ${when}`, t.title));
    }
  }

  for (const s of me.sources) {
    if (!s.changedAt) continue;
    const seenKey = "dueday-seen-" + s.id;
    if (store(seenKey) !== s.changedAt) {
      store(seenKey, s.changedAt);
      notify(`${s.name} was updated`, "Open DueDay to see what's new.");
    }
  }
}

setInterval(tick, 60 * 1000);
setInterval(async () => {
  if (!me) return;
  try {
    me = await api("/api/me");
    render();
    tick();
  } catch {
    /* offline: try again next time */
  }
}, 5 * 60 * 1000);

load();
