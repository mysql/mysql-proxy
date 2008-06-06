; // The Windows message text file for supporting ReportEvent

; // For now, we only have english messages

LanguageNames=(English=0x409:MSG00409)

; // Since the mapping from our log levels to those supported by Windows is incomplete,
; // we define one message for each of the three relevant Windows levels and indicate
; // our log level in the message
; // see log_lvl_map in chassis-log.c for the mapping

; // We currently spam the Application log, as we don't set the necessary registry keys yet.

MessageId=0x1
Severity=Error
Facility=Application
Language=English
%1

MessageId=0x2
Severity=Warning
Facility=Application
Language=English
%1

MessageId=0x4
Severity=Informational
Facility=Application
Language=English
%1
