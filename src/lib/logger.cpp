#include <logger.h>

#include <stdlib.h>
#include <string.h>

Logger logger();

Logger::Logger(const char * const pr):
m_stateInter(true),
m_stateCont(false),
m_stateSyslog(false),
m_debugLevel(0),
m_prefix(pr)
{
}

void Logger::log_die (const char * fmt ...) {
	va_list ap;
	va_start(ap, fmt);
	_log_msg(LOG_ERR, fmt, ap);
	va_end(ap);
	if (not m_stateInter)
		log_err("Exiting.\n");
	exit(1);
}
void Logger::log_debug (const char * fmt ...) {
	va_list ap;
	va_start(ap, fmt);
	_log_debug(1, fmt, ap);
	va_end(ap);
}
void Logger::log_debug (int level, const char * fmt ...) {
	va_list ap;
	va_start(ap, fmt);
	//printf("log_debug: level = %d debug_level = %d\n", level, m_debugLevel);
	if(level <= m_debugLevel)
		_log_debug(level, fmt, ap);
	va_end(ap);
}
void Logger::log_info (const char * fmt ...) {
	va_list ap;
	va_start(ap, fmt);
	_log_msg(LOG_INFO, fmt, ap);
	va_end(ap);
}
void Logger::log_notice (const char * fmt ...) {
	va_list ap;
	va_start(ap, fmt);
	_log_msg_pref("* ", LOG_NOTICE, fmt, ap);
	va_end(ap);
}
void Logger::log_warning (const char * fmt ...) {
	va_list ap;
	va_start(ap, fmt);
	
	_log_msg_pref("! ",LOG_WARNING, fmt, ap);
	va_end(ap);
}
void Logger::log_err (const char * fmt ...) {
	va_list ap;
	va_start(ap, fmt);
	_log_msg(LOG_ERR, fmt, ap);
	va_end(ap);
}
void Logger::_log_debug (int level, const char * fmt, va_list ap) {

	const int max_sz = 7;
	char pref[max_sz];
	
	int i=0;
	for(i=0; i>= 0 and i<=level-1 and i<max_sz-2; i++) {
		pref[i]='>';
	}
	pref[i++]=' ';
	pref[i]='\0';
	
	_log_msg_pref(pref, LOG_DEBUG, fmt, ap);
}
void Logger::_log_msg (const int pri, const char * fmt, va_list ap) {
	char fmsg[MAX_MSG_SZ];
	
	fmsg[0] = '\0';

	if(m_stateSyslog)
		snprintf(fmsg,sizeof(fmsg), "[%d] ",getpgrp());
	
	strncat(fmsg, fmt,  MAX_MSG_SZ-strlen(fmsg)-1);
	
	if (m_stateSyslog) {
		vsyslog(pri, fmsg, ap);
		return;
	}
	
	FILE* fd = 
		(pri == LOG_ERR or pri == LOG_CRIT or pri == LOG_ALERT)	? stderr:stdout;

	vfprintf(fd,fmsg,ap); fflush(fd);

    m_stateCont = fmsg[strlen(fmt)-1] == '\n' ? false:true;
}
void Logger::_log_msg_pref (const char * pref, const int pri, const char * fmt, va_list ap) {
	char msg[MAX_MSG_SZ];
	strncpy(msg, pref, MAX_MSG_SZ-1);
	strncat(msg, fmt,  MAX_MSG_SZ-strlen(msg)-1);
	
	_log_msg(pri, msg, ap);
}

void Logger::setPrefix(const char * const pr) {
    std::string str(pr);
    m_prefix = str;
}
void Logger::setInteractive(bool state) {
    m_stateInter = state;
}
void Logger::setDebugLevel(int dl) {
	m_debugLevel = dl;
}
int Logger::getDebugLevel() {
	return m_debugLevel;
}

void Logger::syslogOn() {
	if(m_stateSyslog) return;
	openlog(m_prefix.c_str(), 0, LOG_DAEMON);
	m_stateSyslog = true;
}
void Logger::syslogOff() {
	if(!m_stateSyslog) return;
	closelog();
	m_stateSyslog = false;
}
