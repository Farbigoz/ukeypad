  const setConnected = connected => {
    $('dot').classList.toggle('connected', connected);
    $('status').textContent = connected ? t('connected') : t('notConnected');
    $('connect').disabled = connected;
    $('disconnect').disabled = !connected;
    document.querySelectorAll('button:not(#connect):not(#disconnect):not(#clearLog), input, select')
      .forEach(el => { if (el.dataset.always !== 'true') el.disabled = !connected; });
  };

  async function readLoop() {
    const decoder = new TextDecoderStream();
    port.readable.pipeTo(decoder.writable).catch(() => {});
    reader = decoder.readable.getReader();
    keepReading = true;
    try {
      while (keepReading) {
        const { value, done } = await reader.read();
        if (done) break;
        receiveBuffer += value;
        const lines = receiveBuffer.split(/\r?\n/);
        receiveBuffer = lines.pop() || '';
        lines.filter(line => line.length).forEach(handleLine);
      }
    } catch (error) {
      if (keepReading) log(`ERR read ${error.message}`, true);
    } finally {
      reader.releaseLock(); reader = null;
    }
  }

  function handleLine(line) {
    log(`< ${line}`);
    parseDescriptorLine(line);
    if (line.startsWith('OK info ')) renderInfo(line);
    if (line.startsWith('OK stats ') || line === 'OK stats=cleared') renderStats(line);
    if (line.startsWith('OK test event=')) appendEvent(line);
    if (line.startsWith('Current bindings:')) listing = true;
    if (listing && line.match(/^\s*slot /)) {
      const match = line.match(/^\s*slot (\d+) -> ([^ ]+)/);
      if (match) {
        const slot = Number(match[1]);
        const binding = match[2];
        currentBindings.set(slot, binding);
        const select = $('key-' + slot);
        if (select && keys.includes(binding)) select.value = binding;
      }
    }
    if (listing && line === 'OK list') listing = false;
    if (line.startsWith('OK debounce_samples=')) {
      const value = line.match(/debounce_samples=(\d+)/)?.[1];
      if (value) $('debounceValue').value = value;
    }
    if (pending.length) {
      const request = pending[0];
      if (!request.matches || request.matches(line)) {
        pending.shift();
        request.resolve(line);
      }
    }
  }

  async function send(command, wait = false, matches = null) {
    if (!port?.writable) throw new Error(t('cdcDisconnected'));
    const writer = port.writable.getWriter();
    let response = null;
    let responsePromise = null;
    if (wait) {
      responsePromise = new Promise(resolve => {
        pending.push({ resolve, matches });
      });
    }
    try {
      log(`> ${command}`);
      await writer.write(new TextEncoder().encode(`${command}\n`));
    } finally {
      writer.releaseLock();
    }
    if (responsePromise) response = await responsePromise;
    return response;
  }

  function responseMatches(command, line) {
    if (command === 'info') return line.startsWith('OK info ');
    if (command === 'get_device') return line === 'OK device_end';
    if (command === 'list') return line === 'OK list';
    if (command === 'debounce') return line.startsWith('OK debounce_samples=');
    return line.startsWith('OK ') || line.startsWith('ERR ');
  }

  async function requestCommand(command) {
    if (!port?.writable) throw new Error(t('cdcDisconnected'));
    await send(command, true, line => responseMatches(command, line));
  }

  async function command(command) {
    if (!port?.writable) return;
    await send(command);
  }

  async function connect() {
    if (!('serial' in navigator)) { $('unsupported').hidden = false; return; }
    try {
      port = await navigator.serial.requestPort();
      await port.open({ baudRate: 115200 });
      clearDescriptor();
      setConnected(true);
      log('OK connected baud=115200');
      readLoop();
      // Serialize startup commands: the descriptor must be received before list
      // can populate the profile-sized binding selectors.
      setTimeout(() => {
        commandChain = commandChain
          .then(() => requestCommand('info'))
          .then(() => requestCommand('get_device'))
          .then(() => requestCommand('list'))
          .then(() => requestCommand('debounce'))
          .catch(error => log(`ERR code=COMMAND_FAILED message=${error.message}`, true));
      }, 300);
    } catch (error) { log(`ERR code=CONNECT_FAILED message=${error.message}`, true); }
  }

  async function disconnect() {
    keepReading = false;
    try { if (reader) await reader.cancel(); } catch (_) {}
    try { if (port) await port.close(); } catch (_) {}
    reader = null; port = null; pending = []; clearDescriptor(); clearStructuredPanels(); setConnected(false); log('OK disconnected');
  }
