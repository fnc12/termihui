#ifndef TERMIHUI_CLIENT_CORE_C_H
#define TERMIHUI_CLIENT_CORE_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * TermiHUI Client Core C API
 * Handle-based API for FFI (uses int instead of pointers)
 * Safe for use from C, Swift, Kotlin, C#, etc.
 */

/// Create new client core instance
/// @return handle (positive int) or -1 on error
int termihui_create_client(void);

/// Destroy client core instance
/// @param handle client core handle
void termihui_destroy_client(int handle);

/// Initialize client core
/// @param handle client core handle
/// @return true if successful
bool termihui_initialize_client(int handle);

/// Shutdown client core
/// @param handle client core handle
void termihui_shutdown_client(int handle);

/// Check if client core handle is valid
/// @param handle client core handle
/// @return true if handle is valid
bool termihui_is_valid_client(int handle);

/// Get library version
/// @return version string (static, do not free)
const char* termihui_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* TERMIHUI_CLIENT_CORE_C_H */

