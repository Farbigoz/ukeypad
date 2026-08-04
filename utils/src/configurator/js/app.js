  $('connect').onclick = connect;
  $('disconnect').onclick = disconnect;
  $('clearLog').onclick = () => { $('log').textContent = ''; };
  $('boot').onclick = async () => {
    if (!confirm('Перейти в ROM bootloader? Устройство отключится от USB.')) return;
    await command('boot');
  };
  $('clearEvents').onclick = () => {
    $('events').replaceChildren();
    $('events').classList.add('empty-state');
    $('events').textContent = 'Включите test и нажмите кнопку.';
  };
  $('info').onclick = () => command('info');
  $('getDevice').onclick = () => command('get_device');
  $('readBindings').onclick = () => command('list');
  $('save').onclick = () => command('save');
  $('reset').onclick = async () => { await command('reset'); setTimeout(() => command('list'), 100); };
  $('testOn').onclick = () => command('test on');
  $('testOff').onclick = () => command('test off');
  $('readStats').onclick = () => command('stats');
  $('clearStats').onclick = () => command('stats clear');
  $('readDebounce').onclick = () => command('debounce');
  $('setDebounce').onclick = () => {
    const value = Number($('debounceValue').value);
    if (!Number.isInteger(value) || value < 1 || value > 255) {
      log('ERR code=INVALID_VALUE debounce_range=1..255', true); return;
    }
    command(`debounce set ${value}`);
  };

  $('bindings').addEventListener('change', event => {
    if (!event.target.matches('select')) return;
    const slot = Number(event.target.id.split('-')[1]);
    command(`bind ${slot} ${event.target.value}`);
  });

  if ('serial' in navigator) {
    navigator.serial.addEventListener('disconnect', event => {
      if (event.target === port) disconnect();
    });
  } else $('unsupported').hidden = false;

  clearDescriptor(); clearStructuredPanels(); setConnected(false);
