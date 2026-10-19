// Exercise anchor/search behavior without network or a browser dependency.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname, '../../docs/javascripts/extra.js'), 'utf8');
const start = source.indexOf('let rsCodeAnchorController;');
const end = source.indexOf('rsOnPage(rsCodesInit);', start);
assert(start >= 0 && end > start);
const cards = [
  { textContent: 'RS-CACHE-003 Content differs', hidden: false },
  { textContent: 'inventory_not_confirmed Consumption not confirmed', hidden: false },
];
let scrolled = 0;
const input = { value: '', dataset: {}, isConnected: true, events: {}, addEventListener(kind, fn) { this.events[kind] = fn; } };
const count = { textContent: '' };
const anchor = { closest: () => cards[1], scrollIntoView: () => ++scrolled };
const document = {
  getElementById: id => id === 'rs-code-search' ? input : id === 'rs-code-count' ? count : id === 'bridge-inventory-not-confirmed' ? anchor : null,
  querySelectorAll: () => cards,
};
const window = new EventTarget();
const location = { hash: '' };
const context = { document, window, location, AbortController };
vm.runInNewContext(source.slice(start, end) + '\nrsCodesInit();', context);
assert.match(count.textContent, /2 of 2/);
input.value = 'RS-CACHE-003';
input.events.input();
assert.equal(cards[1].hidden, true);
location.hash = '#bridge-inventory-not-confirmed';
window.dispatchEvent(new Event('hashchange'));
assert.equal(input.value, '');
assert.equal(cards[1].hidden, false);
assert.equal(scrolled, 1);
location.hash = '#%E0%A4%A'; // Malformed external fragment is harmless.
window.dispatchEvent(new Event('hashchange'));
assert.equal(scrolled, 1);
input.value = 'does not exist';
input.events.input();
assert.match(count.textContent, /No matching code/);
input.events.keydown({ key: 'Escape' });
assert.equal(cards[0].hidden, false);
input.isConnected = false;
location.hash = '#bridge-inventory-not-confirmed';
window.dispatchEvent(new Event('hashchange'));
assert.equal(scrolled, 1); // An old instant-navigation document is never scrolled.
console.log('Diagnostic search, direct anchor reveal and malformed-fragment checks passed.');
