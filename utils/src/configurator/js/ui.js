  const $ = id => document.getElementById(id);
  const log = (text, error = false) => {
    const stamp = new Date().toLocaleTimeString();
    $('log').textContent += `[${stamp}] ${text}\n`;
    $('log').scrollTop = $('log').scrollHeight;
    if (error) $('status').textContent = text;
  };

  function setText(id, value) {
    const element = $(id);
    if (element) element.textContent = value || '—';
  }

  function clearStructuredPanels() {
    [...infoFieldIds, ...statsFieldIds].forEach(id => setText(id, '—'));
    $('statsOverflow').classList.remove('stat-warn');
    $('events').replaceChildren();
    $('events').classList.add('empty-state');
    $('events').textContent = t('emptyState');
  }

  function renderInfo(line) {
    const fields = parseFields(line.slice('OK info '.length));
    setText('infoModel', fields.model);
    setText('infoFirmware', fields.firmware);
    setText('infoProtocol', fields.protocol);
    setText('infoConfigVersion', fields.config_version);
    setText('infoInputs', fields.input_count ? `${fields.input_count} (${fields.input_types || '—'})` : '—');
    setText('infoLeds', fields.led_count);
    setText('infoScan', fields.scan_hz ? `${numberFormat.format(Number(fields.scan_hz))} ${t('hz')}` : '—');
    setText('infoUsb', fields.usb);
    setText('infoCapabilities', fields.capabilities ? fields.capabilities.split(',').map(item => item.replace('=', ': ')).join(' · ') : '—');
  }

  function renderStats(line) {
    if (line === 'OK stats=cleared') {
      ['statsScan', 'statsEvents', 'statsOverflow', 'statsQueue'].forEach(id => setText(id, '0'));
      $('statsOverflow').classList.remove('stat-warn');
      return;
    }
    const fields = parseFields(line.slice('OK stats '.length));
    setText('statsScan', fields.scan ? numberFormat.format(Number(fields.scan)) : '—');
    setText('statsEvents', fields.events ? numberFormat.format(Number(fields.events)) : '—');
    setText('statsOverflow', fields.overflow ? numberFormat.format(Number(fields.overflow)) : '0');
    setText('statsQueue', fields.queue_max ? numberFormat.format(Number(fields.queue_max)) : '0');
    $('statsOverflow').classList.toggle('stat-warn', Number(fields.overflow) > 0);
  }
