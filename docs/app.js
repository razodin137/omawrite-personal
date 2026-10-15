(() => {
  const editor = document.getElementById('editor');
  const wordCountEl = document.getElementById('word-count');
  const charCountEl = document.getElementById('char-count');
  const statusLabel = document.getElementById('status-label');
  const themeToggleBtn = document.getElementById('theme-toggle');
  const fontPickerBtn = document.getElementById('font-picker-btn');
  const fontModal = document.getElementById('font-modal');
  const shortcutsBtn = document.getElementById('shortcuts-btn');
  const shortcutsModal = document.getElementById('shortcuts-modal');
  const closeModals = document.querySelectorAll('.close-modal');
  const saveBtn = document.getElementById('save-btn');
  const openBtn = document.getElementById('open-btn');
  const fileInput = document.getElementById('file-input');
  const searchPanel = document.getElementById('search-panel');
  const searchInput = document.getElementById('search-input');
  const replaceRow = document.getElementById('replace-row');
  const replaceInput = document.getElementById('replace-input');
  const searchCount = document.getElementById('search-count');
  const searchCloseBtn = document.getElementById('search-close');
  const replaceCurrentBtn = document.getElementById('replace-current');
  const replaceAllBtn = document.getElementById('replace-all');

  let currentFileName = "";
  let lastSavedText = "";
  let isModified = false;
  let searchMatches = [];
  let searchMatchIndex = -1;

  // Format date helper: YYYY-MM-DD
  function formatDate(d = new Date()) {
    const year = d.getFullYear();
    const month = String(d.getMonth() + 1).padStart(2, '0');
    const day = String(d.getDate()).padStart(2, '0');
    return `${year}-${month}-${day}`;
  }

  // Format timestamp helper: YYYY-MM-DD HH:MM
  function formatTimestamp(d = new Date()) {
    const ymd = formatDate(d);
    const hours = String(d.getHours()).padStart(2, '0');
    const mins = String(d.getMinutes()).padStart(2, '0');
    return `${ymd} ${hours}:${mins}`;
  }

  // Extract first title/heading
  function extractTitle(text) {
    const lines = text.split('\n');
    for (const rawLine of lines) {
      const line = rawLine.trim();
      if (!line) continue;
      if (line.startsWith('#')) {
        return line.replace(/^#+\s*/, '').trim();
      }
      return line;
    }
    return "untitled";
  }

  // Convert string to slug
  function slugify(text) {
    return text
      .toLowerCase()
      .replace(/[^\w\s-]/g, '')
      .trim()
      .replace(/[\s_-]+/g, '-')
      .replace(/^-+|-+$/g, '');
  }

  // Check if text starts with YAML front matter
  function hasFrontMatter(text) {
    return /^---\r?\n[\s\S]*?\r?\n---\r?\n/.test(text);
  }

  // Update or insert YAML front matter on save
  function updateFrontMatterOnSave(content) {
    const now = new Date();
    const today = formatDate(now);
    const timestamp = formatTimestamp(now);

    if (!hasFrontMatter(content)) {
      // First save! Prepend front matter
      const title = extractTitle(content);
      const frontMatter = [
        "---",
        `date: ${today}`,
        "updated: []",
        `title: ${JSON.stringify(title)}`,
        "draft: true",
        "categories: []",
        "tags: []",
        "---",
        "",
        ""
      ].join("\n");
      return frontMatter + content;
    }

    // Has front matter already: update `updated:` log
    const match = content.match(/^---\r?\n([\s\S]*?)\r?\n---\r?\n/);
    if (!match) return content;

    const fullMatch = match[0];
    let fmBody = match[1];
    const rest = content.slice(fullMatch.length);

    if (/^updated:\s*\[\]/m.test(fmBody)) {
      // Replace empty updated: [] with list
      fmBody = fmBody.replace(/^updated:\s*\[\]/m, `updated:\n  - ${timestamp}`);
    } else if (/^updated:/m.test(fmBody)) {
      // Append to updated: list
      fmBody = fmBody.replace(/^updated:\s*(?:\r?\n(\s+-\s+[^\r\n]+)*)/m, (full) => {
        return `${full}\n  - ${timestamp}`;
      });
    } else {
      // Insert after date line or at top
      if (/^date:[^\r\n]*/m.test(fmBody)) {
        fmBody = fmBody.replace(/^date:[^\r\n]*/m, (d) => `${d}\nupdated:\n  - ${timestamp}`);
      } else {
        fmBody = `updated:\n  - ${timestamp}\n` + fmBody;
      }
    }

    return `---\n${fmBody}\n---\n${rest}`;
  }

  // Compute filename
  function computeFilename(content) {
    const today = formatDate();
    const title = extractTitle(content.replace(/^---\r?\n[\s\S]*?\r?\n---\r?\n/, ''));
    const slug = slugify(title) || 'draft';
    if (slug.startsWith(today)) {
      return `${slug}.md`;
    }
    return `${today}-${slug}.md`;
  }

  // Word and character count
  function updateCounts() {
    const text = editor.value;
    const chars = text.length;
    const words = text.trim() ? text.trim().split(/\s+/).length : 0;

    wordCountEl.textContent = `${words} ${words === 1 ? 'Word' : 'Words'}`;
    charCountEl.textContent = `${chars} ${chars === 1 ? 'Character' : 'Characters'}`;

    if (text !== lastSavedText && lastSavedText !== "") {
      isModified = true;
      document.title = `* ${currentFileName || 'WriteCam'} - WriteCam`;
    }
  }

  // Set status message
  function setStatus(msg, timeout = 4000) {
    statusLabel.textContent = msg;
    if (timeout > 0) {
      setTimeout(() => {
        if (statusLabel.textContent === msg) {
          statusLabel.textContent = "";
        }
      }, timeout);
    }
  }

  // Save / Export
  function saveDocument() {
    let content = editor.value;
    if (!content.trim()) {
      setStatus("Document is empty.");
      return;
    }

    const updatedContent = updateFrontMatterOnSave(content);
    editor.value = updatedContent;
    lastSavedText = updatedContent;
    isModified = false;

    if (!currentFileName) {
      currentFileName = computeFilename(updatedContent);
    }

    // Trigger download of the file
    const blob = new Blob([updatedContent], { type: 'text/markdown;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = currentFileName;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);

    document.title = `${currentFileName} - WriteCam`;
    setStatus(`Saved as ${currentFileName}`);
    updateCounts();
  }

  // Open file
  function openFile(e) {
    const file = e.target.files && e.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (event) => {
      editor.value = event.target.result;
      currentFileName = file.name;
      lastSavedText = editor.value;
      isModified = false;
      document.title = `${currentFileName} - WriteCam`;
      setStatus(`Opened ${file.name}`);
      updateCounts();
    };
    reader.readAsText(file);
    fileInput.value = "";
  }

  // Formatting helpers
  function surroundSelection(before, after) {
    const start = editor.selectionStart;
    const end = editor.selectionEnd;
    const sel = editor.value.slice(start, end);
    const replacement = `${before}${sel}${after}`;
    editor.setRangeText(replacement, start, end, 'select');
    updateCounts();
  }

  // Theme management
  function initTheme() {
    const saved = localStorage.getItem('writecam-theme');
    if (saved) {
      document.documentElement.setAttribute('data-theme', saved);
    } else {
      const prefersDark = window.matchMedia('(prefers-color-scheme: dark)').matches;
      document.documentElement.setAttribute('data-theme', prefersDark ? 'dark' : 'light');
    }
  }

  function toggleTheme() {
    const current = document.documentElement.getAttribute('data-theme') || 'dark';
    const next = current === 'dark' ? 'light' : 'dark';
    document.documentElement.setAttribute('data-theme', next);
    localStorage.setItem('writecam-theme', next);
    setStatus(`Theme: ${next}`);
  }

  // Font picker
  function setFont(fontFamily) {
    editor.style.fontFamily = fontFamily;
    localStorage.setItem('writecam-font', fontFamily);
    document.querySelectorAll('.font-option').forEach(el => {
      el.classList.toggle('selected', el.getAttribute('data-font') === fontFamily);
    });
    fontModal.classList.remove('active');
    setStatus(`Font set to ${fontFamily}`);
  }

  // Search logic
  function openSearch(replace = false) {
    searchPanel.classList.add('visible');
    replaceRow.style.display = replace ? 'flex' : 'none';
    if (editor.selectionStart !== editor.selectionEnd) {
      searchInput.value = editor.value.slice(editor.selectionStart, editor.selectionEnd);
    }
    searchInput.focus();
    searchInput.select();
    performSearch();
  }

  function closeSearch() {
    searchPanel.classList.remove('visible');
    editor.focus();
  }

  function performSearch() {
    const query = searchInput.value.toLowerCase();
    searchMatches = [];
    searchMatchIndex = -1;

    if (!query) {
      searchCount.textContent = "";
      return;
    }

    const text = editor.value.toLowerCase();
    let idx = 0;
    while ((idx = text.indexOf(query, idx)) !== -1) {
      searchMatches.push(idx);
      idx += query.length;
    }

    if (searchMatches.length > 0) {
      searchMatchIndex = 0;
      searchCount.textContent = `1 of ${searchMatches.length}`;
      highlightMatch(searchMatches[0], query.length);
    } else {
      searchCount.textContent = "0 matches";
    }
  }

  function highlightMatch(start, length) {
    editor.focus();
    editor.setSelectionRange(start, start + length);
  }

  function nextMatch() {
    if (searchMatches.length === 0) return;
    searchMatchIndex = (searchMatchIndex + 1) % searchMatches.length;
    searchCount.textContent = `${searchMatchIndex + 1} of ${searchMatches.length}`;
    highlightMatch(searchMatches[searchMatchIndex], searchInput.value.length);
  }

  function prevMatch() {
    if (searchMatches.length === 0) return;
    searchMatchIndex = (searchMatchIndex - 1 + searchMatches.length) % searchMatches.length;
    searchCount.textContent = `${searchMatchIndex + 1} of ${searchMatches.length}`;
    highlightMatch(searchMatches[searchMatchIndex], searchInput.value.length);
  }

  function replaceCurrent() {
    if (searchMatches.length === 0 || searchMatchIndex < 0) return;
    const start = searchMatches[searchMatchIndex];
    const len = searchInput.value.length;
    const rep = replaceInput.value;
    editor.setRangeText(rep, start, start + len, 'end');
    performSearch();
    updateCounts();
  }

  function replaceAll() {
    const query = searchInput.value;
    if (!query) return;
    const rep = replaceInput.value;
    editor.value = editor.value.replaceAll(query, rep);
    performSearch();
    updateCounts();
    setStatus("Replaced all occurrences");
  }

  // Event Listeners
  editor.addEventListener('input', updateCounts);

  editor.addEventListener('keydown', (e) => {
    const isCtrl = e.ctrlKey || e.metaKey;

    if (isCtrl && e.key.toLowerCase() === 's') {
      e.preventDefault();
      saveDocument();
    } else if (isCtrl && e.key.toLowerCase() === 'o') {
      e.preventDefault();
      fileInput.click();
    } else if (isCtrl && e.key.toLowerCase() === 'f') {
      e.preventDefault();
      openSearch(false);
    } else if (isCtrl && e.key.toLowerCase() === 'h') {
      e.preventDefault();
      openSearch(true);
    } else if (isCtrl && e.key.toLowerCase() === 'b') {
      e.preventDefault();
      surroundSelection('**', '**');
    } else if (isCtrl && e.key.toLowerCase() === 'i') {
      e.preventDefault();
      surroundSelection('*', '*');
    } else if (isCtrl && e.key.toLowerCase() === 'k') {
      e.preventDefault();
      surroundSelection('[', '](url)');
    } else if ((isCtrl && e.key === '?') || e.key === 'F1') {
      e.preventDefault();
      shortcutsModal.classList.add('active');
    }
  });

  // Buttons
  themeToggleBtn.addEventListener('click', toggleTheme);
  saveBtn.addEventListener('click', saveDocument);
  openBtn.addEventListener('click', () => fileInput.click());
  fileInput.addEventListener('change', openFile);

  fontPickerBtn.addEventListener('click', () => {
    fontModal.classList.add('active');
  });

  shortcutsBtn.addEventListener('click', () => {
    shortcutsModal.classList.add('active');
  });

  closeModals.forEach(btn => {
    btn.addEventListener('click', () => {
      fontModal.classList.remove('active');
      shortcutsModal.classList.remove('active');
    });
  });

  window.addEventListener('click', (e) => {
    if (e.target === fontModal || e.target === shortcutsModal) {
      fontModal.classList.remove('active');
      shortcutsModal.classList.remove('active');
    }
  });

  document.querySelectorAll('.font-option').forEach(el => {
    el.addEventListener('click', () => {
      setFont(el.getAttribute('data-font'));
    });
  });

  searchCloseBtn.addEventListener('click', closeSearch);
  searchInput.addEventListener('input', performSearch);
  searchInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
      e.preventDefault();
      if (e.shiftKey) prevMatch();
      else nextMatch();
    } else if (e.key === 'Escape') {
      closeSearch();
    }
  });
  replaceInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
      e.preventDefault();
      replaceCurrent();
    }
  });
  replaceCurrentBtn.addEventListener('click', replaceCurrent);
  replaceAllBtn.addEventListener('click', replaceAll);

  // Initialize
  initTheme();
  const savedFont = localStorage.getItem('writecam-font');
  if (savedFont) {
    editor.style.fontFamily = savedFont;
    document.querySelectorAll('.font-option').forEach(el => {
      el.classList.toggle('selected', el.getAttribute('data-font') === savedFont);
    });
  }

  updateCounts();
  setStatus("Ready. Press Ctrl+S to save/export.");
})();
