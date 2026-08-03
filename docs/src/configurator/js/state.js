  const MAX_UI_INPUTS = 32;
  const keys = [
    ...'ABCDEFGHIJKLMNOPQRSTUVWXYZ'.split(''), ...'0123456789'.split(''),
    ...Array.from({ length: 24 }, (_, i) => `F${i + 1}`),
    'ENTER', 'ESC', 'TAB', 'SPACE', 'BACKSPACE', 'INSERT', 'DELETE',
    'HOME', 'END', 'PAGEUP', 'PAGEDOWN', 'UP', 'DOWN', 'LEFT', 'RIGHT',
    'CAPSLOCK', 'PRINTSCREEN', 'SCROLLLOCK', 'MENU', 'MUTE', 'VOLUP', 'VOLDN',
    'CTRL', 'SHIFT', 'ALT', 'GUI', 'WIN', 'LCTRL', 'RCTRL', 'LSHIFT',
    'RSHIFT', 'LALT', 'RALT', 'ALTGR', 'LWIN', 'RWIN', 'CMD'
  ];

  let port = null;
  let reader = null;
  let keepReading = false;
  let receiveBuffer = '';
  let pending = [];
  let commandChain = Promise.resolve();
  let listing = false;
  let descriptor = null;
  let descriptorFrame = null;
  const currentBindings = new Map();
  const numberFormat = new Intl.NumberFormat('ru-RU');
  const infoFieldIds = ['infoModel', 'infoFirmware', 'infoProtocol', 'infoConfigVersion', 'infoInputs', 'infoLeds', 'infoScan', 'infoUsb', 'infoCapabilities'];
  const statsFieldIds = ['statsScan', 'statsEvents', 'statsOverflow', 'statsQueue'];
