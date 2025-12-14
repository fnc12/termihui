#include "JsonHelper.h"
#include "hv/json.hpp"

using json = nlohmann::json;

std::string JsonHelper::createResponse(const std::string& type, const std::string& data, int exitCode, bool running)
{
    json response;
    response["type"] = type;
    
    if (type == "output" && !data.empty()) {
        response["data"] = data;
    } else if (type == "status") {
        response["running"] = running;
        response["exit_code"] = exitCode;
    } else if (type == "error" && !data.empty()) {
        response["message"] = data;
        response["error_code"] = "COMMAND_FAILED";
    } else if (type == "connected") {
        response["server_version"] = "1.0.0";
    } else if (type == "input_sent") {
        response["bytes"] = exitCode;
    }
    
    return response.dump();
}

