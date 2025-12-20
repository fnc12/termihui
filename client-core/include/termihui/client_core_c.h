#ifndef TERMIHUI_CLIENT_CORE_C_H
#define TERMIHUI_CLIENT_CORE_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * TermiHUI Client Core C API
 * Singleton-based API for FFI
 * Safe for use from C, Swift, Kotlin, C#, etc.
 */

/// Get library version
/// @return version string (static, do not free)
const char* termihui_get_version(void);

/// Initialize client core
/// @return true if successful
bool termihui_initialize(void);

/// Shutdown client core
void termihui_shutdown(void);

/// Check if client core is initialized
/// @return true if initialized
bool termihui_is_initialized(void);

/// Send message to client core for processing
/// @param message JSON or other encoded message (null-terminated string)
/// @return response string (valid until next call to this function, do not free)
const char* termihui_send_message(const char* message);

/// Poll single event from client core
/// Call in a loop until NULL is returned
/// @return JSON event string, or NULL if no more events (valid until next call, do not free)
const char* termihui_poll_event(void);

/// Get count of pending events (optional, for preallocating)
/// @return number of pending events
int termihui_pending_events_count(void);

#ifdef __cplusplus
}
#endif

#endif /* TERMIHUI_CLIENT_CORE_C_H */
