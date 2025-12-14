#pragma once

#include <string>

/**
 * Helper class for creating JSON responses
 */
class JsonHelper {
public:
    /**
     * Create standardized JSON response
     * @param type response type (output, status, error, connected, etc.)
     * @param data data payload (used for output and error messages)
     * @param exitCode exit code (used for status and input_sent responses)
     * @param running running status (used for status responses)
     * @return JSON string ready to send
     */
    static std::string createResponse(
        const std::string& type, 
        const std::string& data = "", 
        int exitCode = 0, 
        bool running = false
    );
};

