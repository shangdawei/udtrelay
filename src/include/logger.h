#ifndef LOGGER_H
#define LOGGER_H

#include <syslog.h>
#include <udtgate.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>

class Logger
{
public:

    Logger(const char * const pr);

    void log_die     (const char * fmt ...);
    void log_debug   (const char * fmt ...);
    void log_debug   (int level, const char * fmt ...);
    void log_info    (const char * fmt ...);
    void log_notice  (const char * fmt ...);
    void log_warning (const char * fmt ...);
    void log_err     (const char * fmt ...);
    void setDebugLevel(const int dl);
    int getDebugLevel();

    void setInteractive  (bool);
    void setPrefix  (const char * const pr);
    
    void syslogOn();
    void syslogOff();
    

private:

    bool        m_stateCont;
    bool        m_stateInter;
    bool        m_stateSyslog;
    int         m_debugLevel;
    std::string m_prefix;

    static const int MAX_MSG_SZ =  1024;

    void _log_debug    (int level, const char * fmt, va_list ap);
    void _log_msg      (const int pri, const char * fmt, va_list ap);
    void _log_msg_pref (const char * pref, const int pri, const char * fmt, va_list ap);
};

#endif

