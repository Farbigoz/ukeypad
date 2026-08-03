  function appendEvent(line) {
    const fields = parseFields(line.slice('OK test '.length));
    const row = document.createElement('div');
    const stamp = new Date().toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    row.className = `event-row ${fields.event === 'PRESS' ? 'event-press' : 'event-release'}`;
    row.textContent = `[${stamp}] ${fields.event || 'EVENT'}  ${fields.key || '—'}`;
    if ($('events').classList.contains('empty-state')) {
      $('events').replaceChildren();
      $('events').classList.remove('empty-state');
    }
    $('events').appendChild(row);
    if ($('autoScrollEvents').checked) $('events').scrollTop = $('events').scrollHeight;
  }
