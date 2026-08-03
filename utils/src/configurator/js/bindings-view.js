  function renderBindings() {
    const body = $('bindings');
    body.replaceChildren();
    if (!descriptor) return;
    descriptor.inputs.types.forEach((type, slot) => {
      const row = document.createElement('tr');
      row.innerHTML = `<td>${slot}</td><td>${type}</td><td>GPIO${descriptor.inputs.pins[slot]}</td><td><select id="key-${slot}"></select></td>`;
      const select = row.querySelector('select');
      keys.forEach(key => {
        const option = document.createElement('option');
        option.value = key; option.textContent = key; select.appendChild(option);
      });
      body.appendChild(row);
      const binding = currentBindings.get(slot);
      if (binding && keys.includes(binding)) select.value = binding;
    });
  }

  function clearDescriptor() {
    descriptor = null;
    descriptorFrame = null;
    currentBindings.clear();
    $('bindings').replaceChildren();
  }
