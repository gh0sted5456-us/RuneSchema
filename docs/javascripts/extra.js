// Keep external links visually distinct without changing internal navigation.
document$.subscribe(() => {
  document.querySelectorAll('.md-content a[href^="http"]').forEach((link) => {
    if (link.hostname !== window.location.hostname) {
      link.setAttribute('rel', 'noopener noreferrer');
    }
  });
});
