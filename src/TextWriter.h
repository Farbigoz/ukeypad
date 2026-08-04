#ifndef UKEYPAD_TEXT_WRITER_H
#define UKEYPAD_TEXT_WRITER_H

#ifndef HEX
#define HEX 16
#endif

class TextWriter {
public:
    using TextFn = void (*)(void* context, const char* value);
    using NumberFn = void (*)(void* context, long long value, int base);

    TextWriter(void* context, TextFn printText, NumberFn printNumber,
               TextFn printlnText, NumberFn printlnNumber)
        : _context(context)
        , _printText(printText)
        , _printNumber(printNumber)
        , _printlnText(printlnText)
        , _printlnNumber(printlnNumber)
    {
    }

    void print(const char* value) { _printText(_context, value); }
    void print(long long value, int base = 10) { _printNumber(_context, value, base); }
    void println(const char* value = "") { _printlnText(_context, value); }
    void println(long long value, int base = 10) { _printlnNumber(_context, value, base); }

private:
    void* _context;
    TextFn _printText;
    NumberFn _printNumber;
    TextFn _printlnText;
    NumberFn _printlnNumber;
};

#endif // UKEYPAD_TEXT_WRITER_H
