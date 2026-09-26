# Ask RuneSchema

Ask a plain-language question about RuneSchema authoring. This page searches the
**curated FAQ answers in the documentation** and links you to the loader page
that supports the answer.

<div class="rs-ask" data-rs-ask>
  <label class="rs-ask__label" for="rs-ask-input">Ask a RuneSchema question</label>
  <div class="rs-ask__row">
    <input id="rs-ask-input" class="rs-ask__input" type="search"
      placeholder="Example: Can /raw edit any DataTable?" autocomplete="off">
    <button id="rs-ask-button" class="md-button md-button--primary" type="button">Ask</button>
  </div>
  <div class="rs-ask__examples">
    Try: <button type="button" data-rs-question="Can /raw edit any DataTable?">raw DataTables</button>
    <button type="button" data-rs-question="Does placing a recipe unlock it?">recipe unlocks</button>
    <button type="button" data-rs-question="Can blueprints create a new Blueprint class?">Blueprint classes</button>
    <button type="button" data-rs-question="Do I need modId in every file?">mod identity</button>
    <button type="button" data-rs-question="Can I use nested folders?">nested folders</button>
    <button type="button" data-rs-question="Do clients need the same cooked assets?">multiplayer assets</button>
  </div>
  <div id="rs-ask-status" class="rs-ask__status" aria-live="polite"></div>
  <div id="rs-ask-results" class="rs-ask__results"></div>
</div>

## What this does

- Searches FAQ entries with stable IDs such as `FAQ-RAW-001`.
- Ranks matches by the question, loader name, and answer text.
- Returns a short answer and a direct link to the supporting loader section.
- Falls back to normal documentation search when no FAQ match is strong enough.

This is intentionally a **documentation answerer**, not a generative chatbot.
It does not invent RuneSchema behavior that is absent from the docs.

## Search tips

Use the loader name when you know it, such as `raw`, `assets`, `recipes`,
`spawns`, or `vendors`. Native terms such as `DataTable`, `PersistenceID`,
`DataHandle`, `Niagara`, and `GameplayEffect` also work well.
