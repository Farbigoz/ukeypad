  function parseFields(line) {
    const fields = {};
    line.replace(/([a-z_]+)=([^ ]+)/g, (_, key, value) => { fields[key] = value; return ''; });
    return fields;
  }

  function parseDescriptorLine(line) {
    if (line === 'OK device_begin') {
      descriptorFrame = {};
      return;
    }
    if (!descriptorFrame) return;
    if (line.startsWith('OK device ')) {
      descriptorFrame.device = parseFields(line.slice('OK device '.length));
    } else if (line.startsWith('OK inputs ')) {
      const fields = parseFields(line.slice('OK inputs '.length));
      descriptorFrame.inputs = {
        count: Number(fields.count),
        types: fields.types ? fields.types.split(',') : [],
        pins: fields.pins ? fields.pins.split(',').map(Number) : []
      };
    } else if (line.startsWith('OK leds ')) {
      descriptorFrame.leds = parseFields(line.slice('OK leds '.length));
    } else if (line.startsWith('OK capabilities ')) {
      descriptorFrame.capabilities = parseFields(line.slice('OK capabilities '.length));
    } else if (line === 'OK device_end') {
      try { descriptor = validateDescriptor(descriptorFrame); renderBindings(); }
      catch (error) { descriptor = null; renderBindings(); log(`ERR code=DEVICE_DESCRIPTOR_INVALID message=${error.message}`, true); }
      descriptorFrame = null;
    }
  }

  function validateDescriptor(value) {
    const device = value.device || {};
    const inputs = value.inputs || {};
    const protocol = Number(device.protocol);
    if (!device.model || !device.firmware || !Number.isInteger(protocol) || protocol !== 1) {
      throw new Error('unsupported protocol or missing device fields');
    }
    if (!Number.isInteger(inputs.count) || inputs.count < 1 || inputs.count > MAX_UI_INPUTS ||
        inputs.types.length !== inputs.count || inputs.pins.length !== inputs.count ||
        inputs.pins.some(pin => !Number.isInteger(pin))) {
      throw new Error('invalid input count, types, or pins');
    }
    return { ...value, device, inputs };
  }
