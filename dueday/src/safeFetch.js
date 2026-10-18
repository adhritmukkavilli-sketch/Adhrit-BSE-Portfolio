// Fetch a student-supplied URL without letting it reach our own network
// (localhost, cloud metadata, private IPs), with size and time limits.
import dns from "node:dns/promises";
import net from "node:net";

export function isPrivateAddress(ip) {
  if (net.isIPv6(ip)) {
    const lower = ip.toLowerCase();
    const mapped = lower.match(/^::ffff:(\d+\.\d+\.\d+\.\d+)$/);
    if (mapped) return isPrivateAddress(mapped[1]);
    return (
      lower === "::" ||
      lower === "::1" ||
      /^f[cd]/.test(lower) || // fc00::/7 unique local
      /^fe[89ab]/.test(lower) // fe80::/10 link local
    );
  }
  const [a, b] = ip.split(".").map(Number);
  return (
    a === 0 ||
    a === 10 ||
    a === 127 ||
    (a === 100 && b >= 64 && b <= 127) ||
    (a === 169 && b === 254) ||
    (a === 172 && b >= 16 && b <= 31) ||
    (a === 192 && b === 168) ||
    a >= 224
  );
}

export function normalizeUrl(raw) {
  let s = String(raw || "").trim();
  if (/^webcal:\/\//i.test(s)) s = "https://" + s.slice(9);
  if (!/^[a-z]+:\/\//i.test(s)) s = "https://" + s;
  let url;
  try {
    url = new URL(s);
  } catch {
    return null;
  }
  return url.protocol === "http:" || url.protocol === "https:" ? url : null;
}

async function assertPublicHost(hostname) {
  const host = hostname.replace(/^\[|\]$/g, "");
  let addrs;
  try {
    addrs = net.isIP(host) ? [{ address: host }] : await dns.lookup(host, { all: true });
  } catch {
    throw new Error("Couldn't find that website. Check the link.");
  }
  if (!addrs.length || addrs.some((a) => isPrivateAddress(a.address))) {
    throw new Error("That address isn't allowed.");
  }
}

export async function safeFetch(rawUrl, { maxBytes = 3_000_000, timeoutMs = 15_000, maxRedirects = 4 } = {}) {
  let url = normalizeUrl(rawUrl);
  if (!url) throw new Error("That doesn't look like a website link.");

  for (let hop = 0; hop <= maxRedirects; hop++) {
    await assertPublicHost(url.hostname);
    let res;
    try {
      res = await fetch(url, {
        redirect: "manual",
        signal: AbortSignal.timeout(timeoutMs),
        headers: { "user-agent": "DueDayBot/0.1 (student assignment reminder)" },
      });
    } catch {
      throw new Error("Couldn't reach that website. Try again later.");
    }
    if (res.status >= 300 && res.status < 400 && res.headers.get("location")) {
      url = new URL(res.headers.get("location"), url);
      if (url.protocol !== "http:" && url.protocol !== "https:") throw new Error("Bad redirect.");
      continue;
    }
    if (!res.ok) throw new Error(`The site answered with error ${res.status}.`);

    const reader = res.body.getReader();
    const chunks = [];
    let size = 0;
    for (;;) {
      const { done, value } = await reader.read();
      if (done) break;
      size += value.length;
      if (size > maxBytes) {
        await reader.cancel();
        throw new Error("That page is too big to read.");
      }
      chunks.push(value);
    }
    return { url: url.toString(), body: Buffer.concat(chunks).toString("utf8") };
  }
  throw new Error("Too many redirects.");
}
