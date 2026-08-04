  const translations = {
    ru: {
      pageTitle: 'USB HID keypad configurator', subtitle: 'Standalone Web Serial configurator. Работает в Chrome/Edge на desktop.', connect: 'Подключить CDC', disconnect: 'Отключить', clearLog: 'Очистить лог', boot: 'Перейти в bootloader', connected: 'Подключено', notConnected: 'Не подключено', hintConnect: 'Подключи keypad по USB и нажми «Подключить CDC». CDC-конфигурация и HID работают одновременно.', unsupported: 'Этот браузер не поддерживает Web Serial API. Используй Chrome или Edge на desktop.', info: 'Информация', refreshInfo: 'Обновить info', deviceDescriptor: 'Описание устройства', model: 'Модель', firmware: 'Прошивка', protocol: 'Протокол', nvsFormat: 'Формат NVS', inputs: 'Входы', leds: 'Светодиоды', scan: 'Сканирование', capabilities: 'Возможности', debounce: 'Debounce', samples: 'samples', read: 'Прочитать', apply: 'Применить', debounceHint: 'При 2000 Гц: 4 samples ≈ 2 мс. Значение сохраняется в NVS командой «Сохранить в NVS».', bindings: 'Бинды', slot: 'Слот', type: 'Тип', key: 'Клавиша', save: 'Сохранить в NVS', reset: 'Сбросить defaults', duplicates: 'Дубликаты разрешены: две физические кнопки могут иметь один HID-код.', test: 'Тест кнопок', enableTest: 'Включить test', disable: 'Выключить', testHint: 'Тест использует тот же timer 2000 Гц и integrator debounce. События появляются в отдельном поле, а HID продолжает работать.', stats: 'Статистика', clear: 'Очистить', scans: 'Сканов', events: 'Событий', overflows: 'Переполнений', queuePeak: 'Пик очереди', buttonEvents: 'События кнопок', autoScroll: 'автопрокрутка', emptyState: 'Включите test и нажмите кнопку.', cdcLog: 'Технический лог CDC', hz: ' Гц', confirmBoot: 'Перейти в ROM bootloader? Устройство отключится от USB.', cdcDisconnected: 'CDC не подключён', language: 'Язык', russian: 'RU', english: 'EN'
    },
    en: {
      pageTitle: 'USB HID keypad configurator', subtitle: 'Standalone Web Serial configurator. Works in Chrome/Edge on desktop.', connect: 'Connect CDC', disconnect: 'Disconnect', clearLog: 'Clear log', boot: 'Enter bootloader', connected: 'Connected', notConnected: 'Not connected', hintConnect: 'Connect the keypad via USB and press “Connect CDC”. CDC configuration and HID work simultaneously.', unsupported: 'This browser does not support Web Serial API. Use Chrome or Edge on desktop.', info: 'Info', refreshInfo: 'Refresh info', deviceDescriptor: 'Device descriptor', model: 'Model', firmware: 'Firmware', protocol: 'Protocol', nvsFormat: 'NVS format', inputs: 'Inputs', leds: 'LEDs', scan: 'Scan', capabilities: 'Capabilities', debounce: 'Debounce', samples: 'samples', read: 'Read', apply: 'Apply', debounceHint: 'At 2000 Hz: 4 samples ≈ 2 ms. The value is saved to NVS by “Save to NVS”.', bindings: 'Bindings', slot: 'Slot', type: 'Type', key: 'Key', save: 'Save to NVS', reset: 'Reset defaults', duplicates: 'Duplicates are allowed: two physical buttons can share one HID code.', test: 'Button test', enableTest: 'Enable test', disable: 'Disable', testHint: 'Test uses the same 2000 Hz timer and integrator debounce. Events appear separately while HID keeps working.', stats: 'Statistics', clear: 'Clear', scans: 'Scans', events: 'Events', overflows: 'Overflows', queuePeak: 'Queue peak', buttonEvents: 'Button events', autoScroll: 'auto-scroll', emptyState: 'Enable test and press a button.', cdcLog: 'CDC technical log', hz: ' Hz', confirmBoot: 'Enter ROM bootloader? The device will disconnect from USB.', cdcDisconnected: 'CDC is not connected', language: 'Language', russian: 'RU', english: 'EN'
    }
  };
  let currentLang = 'ru';
  let numberFormat = new Intl.NumberFormat('ru-RU');
  function detectLang() {
    const saved = localStorage.getItem('ukeypad_lang');
    if (saved && translations[saved]) return saved;
    const browserLang = (navigator.language || '').slice(0, 2).toLowerCase();
    return translations[browserLang] ? browserLang : 'ru';
  }
  function t(key) { return translations[currentLang][key] || translations.ru[key] || key; }
  function applyLang(lang) {
    if (!translations[lang]) return;
    currentLang = lang;
    localStorage.setItem('ukeypad_lang', lang);
    numberFormat = new Intl.NumberFormat(lang === 'ru' ? 'ru-RU' : 'en-US');
    document.documentElement.lang = lang;
    document.title = t('pageTitle');
    document.querySelectorAll('[data-i18n]').forEach(el => { el.textContent = t(el.dataset.i18n); });
    const selector = document.getElementById('langSelect');
    if (selector) selector.value = lang;
  }
  currentLang = detectLang();
  // Apply after the DOM is parsed; the script is bundled at the end of body.
  window.addEventListener('DOMContentLoaded', () => applyLang(currentLang));
