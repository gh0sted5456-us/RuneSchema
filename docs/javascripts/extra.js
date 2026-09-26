const RS_DOC_MODE_KEY = "runeschema-doc-mode";
const RS_ADVANCED_SECTIONS = new Set(["Developer", "Release"]);
const RS_ADVANCED_PATHS = [
  "/API-REFERENCE/",
  "/SCHEMAS/",
  "/STARTUP-PERFORMANCE/",
  "/MANUAL-SAVE-RECOVERY/",
  "/NATIVE-HOOK-PARITY/",
  "/BASELINE-SUBSYSTEM-AUDIT/",
  "/HELpy-REDESIGN-AUDIT/",
  "/raw/BASE-BUILDER-IMPORT/",
  "/DOCUMENTATION-INDEX/",
  "/RELEASES/"
];

function rsLabel(node) {
  return (node?.textContent || "").replace(/\s+/g, " ").trim();
}

function rsGetMode() {
  const stored = localStorage.getItem(RS_DOC_MODE_KEY);
  return stored === "advanced" ? "advanced" : "simple";
}

function rsIsAdvancedHref(href) {
  try {
    const url = new URL(href, window.location.href);
    return RS_ADVANCED_PATHS.some((part) => url.pathname.includes(part));
  } catch {
    return false;
  }
}

function rsMarkAdvancedNavigation() {
  document.querySelectorAll(".md-tabs__item").forEach((item) => {
    const link = item.querySelector(".md-tabs__link");
    if (RS_ADVANCED_SECTIONS.has(rsLabel(link))) {
      item.dataset.rsAdvanced = "true";
    }
  });

  document.querySelectorAll(".md-nav--primary").forEach((nav) => {
    const list = nav.querySelector(":scope > .md-nav__list");
    if (!list) return;

    list.querySelectorAll(":scope > .md-nav__item").forEach((item) => {
      const direct = item.querySelector(":scope > .md-nav__link");
      if (RS_ADVANCED_SECTIONS.has(rsLabel(direct))) {
        item.dataset.rsAdvanced = "true";
      }
    });
  });
}

function rsFilterSearchResults(mode) {
  document.querySelectorAll(".md-search-result__item").forEach((item) => {
    const link = item.querySelector("a[href]");
    item.hidden = mode === "simple" && !!link && rsIsAdvancedHref(link.href);
  });
}

function rsUpdateSwitch(mode) {
  document.querySelectorAll(".rs-mode-switch__button").forEach((button) => {
    const active = button.dataset.mode === mode;
    button.setAttribute("aria-pressed", active ? "true" : "false");
  });
}

function rsApplyMode(mode, persist = true) {
  const normalized = mode === "advanced" ? "advanced" : "simple";
  document.documentElement.dataset.rsDocMode = normalized;

  if (persist) {
    localStorage.setItem(RS_DOC_MODE_KEY, normalized);
  }

  rsMarkAdvancedNavigation();

  document.querySelectorAll('[data-rs-advanced="true"]').forEach((node) => {
    node.hidden = normalized === "simple";
  });

  rsUpdateSwitch(normalized);
  rsFilterSearchResults(normalized);
}

function rsEnsureModeSwitch() {
  if (document.querySelector(".rs-mode-switch")) return;

  const inner = document.querySelector(".md-header__inner");
  if (!inner) return;

  const group = document.createElement("div");
  group.className = "rs-mode-switch";
  group.setAttribute("role", "group");
  group.setAttribute("aria-label", "Documentation mode");
  group.innerHTML = `
    <button type="button" class="rs-mode-switch__button" data-mode="simple"
      title="Show authoring documentation only">Simple</button>
    <button type="button" class="rs-mode-switch__button" data-mode="advanced"
      title="Show developer, internals, and release documentation">Advanced</button>
  `;

  group.querySelectorAll("button").forEach((button) => {
    button.addEventListener("click", () => rsApplyMode(button.dataset.mode));
  });

  const search = inner.querySelector(".md-search");
  const source = inner.querySelector(".md-header__source");
  const anchor = search || source;

  if (anchor) inner.insertBefore(group, anchor);
  else inner.appendChild(group);
}

function rsEnsureSearchObserver() {
  const root = document.querySelector(".md-search");
  if (!root || root.dataset.rsObserved === "true") return;

  root.dataset.rsObserved = "true";
  const observer = new MutationObserver(() => rsFilterSearchResults(rsGetMode()));
  observer.observe(root, { childList: true, subtree: true });
}

function rsDecorateExternalLinks() {
  document.querySelectorAll('.md-content a[href^="http"]').forEach((link) => {
    if (link.hostname !== window.location.hostname) {
      link.setAttribute("rel", "noopener noreferrer");
    }
  });
}

document.documentElement.dataset.rsDocMode = rsGetMode();

document$.subscribe(() => {
  rsEnsureModeSwitch();
  rsEnsureSearchObserver();
  rsDecorateExternalLinks();
  rsApplyMode(rsGetMode(), false);
});
