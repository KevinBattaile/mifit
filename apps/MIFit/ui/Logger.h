#ifndef mifit_ui_Logger_h
#define mifit_ui_Logger_h

#include <string>

#if defined(__GNUC__) || defined(__clang__)
#define MIFIT_PRINTF_ATTR(fmt_idx, arg_idx) __attribute__((format(printf, fmt_idx, arg_idx)))
#else
#define MIFIT_PRINTF_ATTR(fmt_idx, arg_idx)
#endif

class Logger
{
public:
    static void log(const char *format, ...) MIFIT_PRINTF_ATTR(1, 2);
    static void debug(const char *format, ...) MIFIT_PRINTF_ATTR(1, 2);

    static void message(const char *format, ...) MIFIT_PRINTF_ATTR(1, 2);
    static void footer(const char *format, ...) MIFIT_PRINTF_ATTR(1, 2);


    static void log(const std::string &str)
    {
        log("%s", str.c_str());
    }

    static void debug(const std::string &str)
    {
        debug("%s", str.c_str());
    }

    static void message(const std::string &str)
    {
        message("%s", str.c_str());
    }

    static void footer(const std::string &str)
    {
        footer("%s", str.c_str());
    }

};

#endif // mifit_ui_Logger_h
