// Keep external links visually distinct without changing internal navigation.
document$.subscribe(() => {
  document.querySelectorAll('.md-content a[href^="http"]').forEach((link) => {
    if (link.hostname !== window.location.hostname) {
      link.setAttribute('rel', 'noopener noreferrer');
    }
  });
});


const rsAskStopWords = new Set([
  "a","an","and","are","as","at","be","by","can","do","does","for","from","how",
  "i","if","in","is","it","my","of","on","or","the","this","to","use","what",
  "when","where","which","with"
]);

function rsAskNormalize(value) {
  return String(value || "")
    .toLowerCase()
    .replace(/[^a-z0-9_/$+.:-]+/g, " ")
    .replace(/\s+/g, " ")
    .trim();
}

function rsAskTokens(value) {
  return rsAskNormalize(value)
    .split(" ")
    .filter((token) => token.length > 1 && !rsAskStopWords.has(token));
}

function rsAskEscape(value) {
  const node = document.createElement("div");
  node.textContent = String(value || "");
  return node.innerHTML;
}

function rsAskSiteRoot() {
  const logo = document.querySelector("a.md-header__button.md-logo");
  return logo ? logo.href : new URL("./", window.location.href).href;
}

async function rsAskLoadIndex() {
  if (window.__runeSchemaSearchIndex) return window.__runeSchemaSearchIndex;
  const url = new URL("search/search_index.json", rsAskSiteRoot());
  const response = await fetch(url);
  if (!response.ok) throw new Error("search index unavailable");
  const payload = await response.json();
  window.__runeSchemaSearchIndex = payload.docs || [];
  return window.__runeSchemaSearchIndex;
}

function rsAskScore(query, record) {
  const q = rsAskNormalize(query);
  const tokens = rsAskTokens(query);
  const title = rsAskNormalize(record.title);
  const text = rsAskNormalize(record.text);
  const location = rsAskNormalize(record.location);
  let score = 0;

  if (title.includes(q) && q.length > 3) score += 80;
  if (text.includes(q) && q.length > 5) score += 45;

  for (const token of tokens) {
    if (title.includes(token)) score += 18;
    if (location.includes(token)) score += 10;
    if (text.includes(token)) score += 5;
  }

  const idMatch = String(record.title || "").match(/FAQ-[A-Z0-9]+-\d+/i);
  if (idMatch && q.includes(idMatch[0].toLowerCase())) score += 120;
  return score;
}

function rsAskShortAnswer(text) {
  const clean = String(text || "").replace(/\s+/g, " ").trim();
  if (clean.length <= 360) return clean;
  const clipped = clean.slice(0, 360);
  const sentence = clipped.lastIndexOf(". ");
  return (sentence > 180 ? clipped.slice(0, sentence + 1) : clipped + "…");
}

async function rsAskRun(root, question) {
  const status = root.querySelector("#rs-ask-status");
  const results = root.querySelector("#rs-ask-results");
  const query = String(question || "").trim();
  if (!query) {
    status.textContent = "Enter a question first.";
    results.innerHTML = "";
    return;
  }

  status.textContent = "Searching documented RuneSchema answers…";
  results.innerHTML = "";

  try {
    const docs = (await rsAskLoadIndex()).filter((record) => rsDocsMode() === "advanced" || !rsDocsIsAdvancedHref(record.location));
    const faq = docs.filter((record) => /^FAQ-[A-Z0-9]+-\d+\b/i.test(String(record.title || "")));
    const ranked = faq
      .map((record) => ({ record, score: rsAskScore(query, record) }))
      .filter((item) => item.score > 0)
      .sort((a, b) => b.score - a.score);

    const best = ranked.slice(0, 4);
    if (!best.length || best[0].score < 10) {
      const searchUrl = new URL(rsAskSiteRoot());
      searchUrl.searchParams.set("q", query);
      status.innerHTML = 'No strong FAQ match. <a href="' + searchUrl.href + '">Search the full documentation</a>.';
      return;
    }

    status.textContent = best.length === 1 ? "Best documented match:" : "Best documented matches:";
    results.innerHTML = best.map(({ record, score }, index) => {
      const id = (String(record.title).match(/FAQ-[A-Z0-9]+-\d+/i) || ["FAQ"])[0];
      const questionTitle = String(record.title).replace(/^FAQ-[A-Z0-9]+-\d+\s*[—:-]?\s*/i, "");
      const href = new URL(record.location, rsAskSiteRoot()).href;
      return '<article class="rs-ask-result' + (index === 0 ? ' rs-ask-result--best' : '') + '">' +
        '<div class="rs-ask-result__meta"><span class="rs-faq-id">' + rsAskEscape(id) + '</span>' +
        (index === 0 ? '<span>Best match</span>' : '<span>Related</span>') + '</div>' +
        '<h3>' + rsAskEscape(questionTitle || record.title) + '</h3>' +
        '<p>' + rsAskEscape(rsAskShortAnswer(record.text)) + '</p>' +
        '<a href="' + href + '">Open the supporting answer →</a>' +
        '</article>';
    }).join("");
  } catch (error) {
    status.textContent = "The documentation search index could not be loaded.";
  }
}

function rsAskInit() {
  document.querySelectorAll("[data-rs-ask]").forEach((root) => {
    if (root.dataset.rsReady === "1") return;
    root.dataset.rsReady = "1";
    const input = root.querySelector("#rs-ask-input");
    const button = root.querySelector("#rs-ask-button");
    const run = () => rsAskRun(root, input && input.value);
    if (button) button.addEventListener("click", run);
    if (input) input.addEventListener("keydown", (event) => {
      if (event.key === "Enter") { event.preventDefault(); run(); }
    });
    root.querySelectorAll("[data-rs-question]").forEach((example) => {
      example.addEventListener("click", () => {
        if (input) input.value = example.dataset.rsQuestion || "";
        run();
      });
    });
    const params = new URLSearchParams(window.location.search);
    const initial = params.get("q");
    if (initial && input) { input.value = initial; run(); }
  });
}

document$.subscribe(rsAskInit);

/* RuneSchema Simple / Advanced documentation modes */
const RS_DOC_MODE_KEY = "runeschema-doc-mode";
const RS_ADVANCED_SECTIONS = new Set(["Developers", "Release"]);
const RS_ADVANCED_PATHS = [
  "/DEVELOPER-GUIDE/",
  "/API-REFERENCE/",
  "/SCHEMAS/",
  "/raw/BASE-BUILDER-IMPORT/",
  "/DOCUMENTATION-INDEX/",
  "/RELEASES/",
  "/RELEASE-0.7.5.28/"
];

function rsDocsLabel(node) {
  return (node?.textContent || "").replace(/\s+/g, " ").trim();
}

function rsDocsMode() {
  return localStorage.getItem(RS_DOC_MODE_KEY) === "advanced" ? "advanced" : "simple";
}

function rsDocsIsAdvancedHref(href) {
  try {
    const url = new URL(href, window.location.href);
    return RS_ADVANCED_PATHS.some((part) => url.pathname.includes(part));
  } catch {
    return false;
  }
}

function rsDocsMarkAdvancedNav() {
  document.querySelectorAll(".md-tabs__item").forEach((item) => {
    const link = item.querySelector(".md-tabs__link");
    if (RS_ADVANCED_SECTIONS.has(rsDocsLabel(link))) item.dataset.rsAdvanced = "true";
  });

  document.querySelectorAll(".md-nav--primary").forEach((nav) => {
    const list = nav.querySelector(":scope > .md-nav__list");
    if (!list) return;
    list.querySelectorAll(":scope > .md-nav__item").forEach((item) => {
      const direct = item.querySelector(":scope > .md-nav__link");
      if (RS_ADVANCED_SECTIONS.has(rsDocsLabel(direct))) item.dataset.rsAdvanced = "true";
    });
  });
}

function rsDocsFilterSearch(mode) {
  document.querySelectorAll(".md-search-result__item").forEach((item) => {
    const link = item.querySelector("a[href]");
    item.hidden = mode === "simple" && !!link && rsDocsIsAdvancedHref(link.href);
  });
}

function rsDocsUpdateSwitch(mode) {
  document.querySelectorAll(".rs-mode-switch__button").forEach((button) => {
    button.setAttribute("aria-pressed", button.dataset.mode === mode ? "true" : "false");
  });
}

function rsDocsApplyMode(mode, persist = true) {
  const value = mode === "advanced" ? "advanced" : "simple";
  document.documentElement.dataset.rsDocMode = value;
  if (persist) localStorage.setItem(RS_DOC_MODE_KEY, value);

  rsDocsMarkAdvancedNav();
  document.querySelectorAll('[data-rs-advanced="true"]').forEach((node) => {
    node.hidden = value === "simple";
  });

  rsDocsUpdateSwitch(value);
  rsDocsFilterSearch(value);
}

function rsDocsEnsureSwitch() {
  if (document.querySelector(".rs-mode-switch")) return;
  const inner = document.querySelector(".md-header__inner");
  if (!inner) return;

  const group = document.createElement("div");
  group.className = "rs-mode-switch";
  group.setAttribute("role", "group");
  group.setAttribute("aria-label", "Documentation mode");
  group.innerHTML =
    '<button type="button" class="rs-mode-switch__button" data-mode="simple" title="Authoring navigation only">Simple</button>' +
    '<button type="button" class="rs-mode-switch__button" data-mode="advanced" title="Show developer and release navigation">Advanced</button>';

  group.querySelectorAll("button").forEach((button) => {
    button.addEventListener("click", () => rsDocsApplyMode(button.dataset.mode));
  });

  const anchor = inner.querySelector(".md-search") || inner.querySelector(".md-header__source");
  if (anchor) inner.insertBefore(group, anchor);
  else inner.appendChild(group);
}

function rsDocsEnsureSearchObserver() {
  const root = document.querySelector(".md-search");
  if (!root || root.dataset.rsModeObserved === "true") return;
  root.dataset.rsModeObserved = "true";
  new MutationObserver(() => rsDocsFilterSearch(rsDocsMode()))
    .observe(root, { childList: true, subtree: true });
}

document.documentElement.dataset.rsDocMode = rsDocsMode();

document$.subscribe(() => {
  rsDocsEnsureSwitch();
  rsDocsEnsureSearchObserver();
  rsDocsApplyMode(rsDocsMode(), false);
});
