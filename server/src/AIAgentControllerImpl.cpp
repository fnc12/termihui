#include "AIAgentControllerImpl.h"
#include <hv/json.hpp>
#include <fmt/core.h>
#include <algorithm>

using json = nlohmann::json;

AIAgentControllerImpl::AIAgentControllerImpl() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    multiHandle = curl_multi_init();
}

AIAgentControllerImpl::~AIAgentControllerImpl() {
    // Cleanup all active requests
    for (auto& [handle, req] : activeRequests) {
        curl_multi_remove_handle(multiHandle, handle);
        if (req->headers) {
            curl_slist_free_all(req->headers);
        }
        curl_easy_cleanup(handle);
    }
    activeRequests.clear();
    
    if (multiHandle) {
        curl_multi_cleanup(multiHandle);
    }
    curl_global_cleanup();
}

void AIAgentControllerImpl::setEndpoint(std::string ep) {
    fmt::print("AI: setEndpoint called with '{}'\n", ep);
    endpoint = std::move(ep);
}

void AIAgentControllerImpl::setModel(std::string m) {
    model = std::move(m);
}

void AIAgentControllerImpl::setApiKey(std::string key) {
    apiKey = std::move(key);
}

size_t AIAgentControllerImpl::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t totalSize = size * nmemb;
    auto* req = static_cast<ActiveRequest*>(userdata);
    req->responseBuffer.append(ptr, totalSize);
    return totalSize;
}

std::string AIAgentControllerImpl::buildRequestBody(uint64_t sessionId, const std::string& message) {
    json messages = json::array();
    
    // Add system message
    messages.push_back({
        {"role", "system"},
        {"content", "You are a helpful terminal assistant. Help the user with their questions about command line, programming, and system administration. Be concise and practical."}
    });
    
    // Add chat history
    auto it = chatHistory.find(sessionId);
    if (it != chatHistory.end()) {
        for (const auto& msg : it->second) {
            messages.push_back({
                {"role", msg.role},
                {"content", msg.content}
            });
        }
    }
    
    // Add new user message
    messages.push_back({
        {"role", "user"},
        {"content", message}
    });
    
    json requestBody = {
        {"messages", std::move(messages)},
        {"stream", true}
    };
    
    // Add model if configured
    if (!model.empty()) {
        requestBody["model"] = model;
    }
    
    return requestBody.dump();
}

void AIAgentControllerImpl::sendMessage(uint64_t sessionId, const std::string& message) {
    fmt::print("AI: sendMessage called, endpoint='{}', model='{}'\n", endpoint, model);
    
    // Add user message to history
    chatHistory[sessionId].push_back({std::string("user"), std::string(message)});
    
    // Create new request
    auto activeRequest = std::make_unique<ActiveRequest>();
    activeRequest->sessionId = sessionId;
    activeRequest->postData = buildRequestBody(sessionId, message);
    
    // Setup CURL handle
    CURL* handle = curl_easy_init();
    if (!handle) {
        fmt::print(stderr, "Failed to create CURL handle\n");
        return;
    }
    
    activeRequest->handle = handle;
    
    // Build URL (store in ActiveRequest to keep it alive - CURL stores pointer only!)
    activeRequest->url = endpoint + "/v1/chat/completions";
    fmt::print("AI: POST to {}\n", activeRequest->url);
    fmt::print("AI: Request body: {}\n", activeRequest->postData);
    
    // Setup headers
    activeRequest->headers = curl_slist_append(nullptr, "Content-Type: application/json");
    activeRequest->headers = curl_slist_append(activeRequest->headers, "Accept: text/event-stream");
    if (!apiKey.empty()) {
        std::string authHeader = "Authorization: Bearer " + apiKey;
        activeRequest->headers = curl_slist_append(activeRequest->headers, authHeader.c_str());
    }
    
    // Configure request
    curl_easy_setopt(handle, CURLOPT_URL, activeRequest->url.c_str());
    curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);  // Debug output
    curl_easy_setopt(handle, CURLOPT_POST, 1L);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, activeRequest->postData.c_str());
    curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(activeRequest->postData.size()));
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, activeRequest->headers);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, activeRequest.get());
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 300L);  // 5 minutes timeout
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 30L);  // 30 seconds for initial connection (some LLM servers are slow to respond)
    
    // Add to multi handle
    CURLMcode mc = curl_multi_add_handle(multiHandle, handle);
    if (mc != CURLM_OK) {
        fmt::print(stderr, "AI: curl_multi_add_handle error: {}\n", curl_multi_strerror(mc));
    }
    
    // Store request
    activeRequests[handle] = std::move(activeRequest);
    
    fmt::print("AI: Started request for session {} (active requests: {})\n", sessionId, activeRequests.size());
}

std::vector<AIEvent> AIAgentControllerImpl::parseSSEBuffer(ActiveRequest& activeRequest) {
    std::vector<AIEvent> events;
    
    // Combine partial line with new data
    std::string& buffer = activeRequest.responseBuffer;
    std::string& partial = activeRequest.partialLine;
    
    // Process complete lines
    size_t pos = 0;
    while (pos < buffer.size()) {
        size_t lineEnd = buffer.find('\n', pos);
        if (lineEnd == std::string::npos) {
            // Incomplete line - save for next time
            partial = buffer.substr(pos);
            buffer.clear();
            break;
        }
        
        std::string line = partial + buffer.substr(pos, lineEnd - pos);
        partial.clear();
        pos = lineEnd + 1;
        
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // Skip empty lines
        if (line.empty()) {
            continue;
        }
        
        // Parse SSE data line
        if (line.rfind("data: ", 0) == 0) {
            std::string data = line.substr(6);
            fmt::print("AI: SSE data: {}\n", data);
            
            // Check for stream end
            if (data == "[DONE]") {
                // Add assistant response to history
                if (!activeRequest.accumulatedContent.empty()) {
                    chatHistory[activeRequest.sessionId].push_back({"assistant", activeRequest.accumulatedContent});
                }
                // Pass full response content in Done event for persistence
                events.push_back({AIEvent::Type::Done, activeRequest.sessionId, activeRequest.accumulatedContent});
                continue;
            }
            
            // Parse JSON
            try {
                json j = json::parse(data);
                
                // Extract content delta from choices[0].delta.content
                auto choicesIt = j.find("choices");
                if (choicesIt == j.end() || choicesIt->empty()) {
                    fmt::print(stderr, "AI: SSE response missing 'choices' array\n");
                    continue;
                }
                
                auto& choice = (*choicesIt)[0];
                auto deltaIt = choice.find("delta");
                if (deltaIt == choice.end()) {
                    fmt::print(stderr, "AI: SSE response missing 'delta' in choice\n");
                    continue;
                }
                
                auto contentIt = deltaIt->find("content");
                if (contentIt == deltaIt->end() || contentIt->is_null()) {
                    // No content in this delta - normal for role-only chunks
                    continue;
                }
                
                std::string content = contentIt->get<std::string>();
                if (!content.empty()) {
                    fmt::print("AI: Chunk content: '{}'\n", content);
                    activeRequest.accumulatedContent += content;
                    events.push_back({AIEvent::Type::Chunk, activeRequest.sessionId, std::move(content)});
                }
            } catch (const json::exception& e) {
                fmt::print(stderr, "AI: JSON parse error: {}\n", e.what());
            }
        }
    }
    
    // Clear processed data
    if (pos >= buffer.size()) {
        buffer.clear();
    } else {
        buffer = buffer.substr(pos);
    }
    
    return events;
}

void AIAgentControllerImpl::cleanupRequest(CURL* handle) {
    auto it = activeRequests.find(handle);
    if (it != activeRequests.end()) {
        curl_multi_remove_handle(multiHandle, handle);
        if (it->second->headers) {
            curl_slist_free_all(it->second->headers);
        }
        curl_easy_cleanup(handle);
        activeRequests.erase(it);
    }
}

std::vector<AIEvent> AIAgentControllerImpl::update() {
    std::vector<AIEvent> events;
    
    if (activeRequests.empty()) {
        return events;
    }
    
    // Perform multi operations
    int stillRunning = 0;
    CURLMcode mc = curl_multi_perform(multiHandle, &stillRunning);
    fmt::print("AI: update() - activeRequests={}, stillRunning={}, mc={}\n", 
               activeRequests.size(), stillRunning, static_cast<int>(mc));
    if (mc != CURLM_OK) {
        fmt::print(stderr, "AI: curl_multi_perform error: {}\n", curl_multi_strerror(mc));
    }
    
    // Process data from all active requests
    for (auto& [handle, req] : activeRequests) {
        if (!req->responseBuffer.empty()) {
            auto newEvents = parseSSEBuffer(*req);
            events.insert(events.end(), newEvents.begin(), newEvents.end());
        }
    }
    
    // Check for completed transfers
    CURLMsg* msg;
    int msgsLeft;
    std::vector<CURL*> toCleanup;
    
    while ((msg = curl_multi_info_read(multiHandle, &msgsLeft))) {
        if (msg->msg == CURLMSG_DONE) {
            CURL* handle = msg->easy_handle;
            CURLcode result = msg->data.result;
            
            auto it = activeRequests.find(handle);
            if (it != activeRequests.end()) {
                uint64_t sessionId = it->second->sessionId;
                fmt::print("AI: Request done - CURLcode={} ({}), URL='{}'\n", 
                           static_cast<int>(result), curl_easy_strerror(result), it->second->url);
                
                if (result != CURLE_OK) {
                    // Error occurred
                    std::string error = curl_easy_strerror(result);
                    events.push_back({AIEvent::Type::Error, sessionId, error});
                    fmt::print(stderr, "AI: Request failed for session {}: {} (code {})\n", sessionId, error, static_cast<int>(result));
                } else {
                    // Check HTTP status code
                    long httpCode = 0;
                    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &httpCode);
                    
                    if (httpCode >= 400) {
                        std::string error = fmt::format("HTTP error {}", httpCode);
                        events.push_back({AIEvent::Type::Error, sessionId, error});
                        fmt::print(stderr, "AI: HTTP error for session {}: {}\n", sessionId, httpCode);
                    } else {
                        // Process any remaining data
                        if (!it->second->responseBuffer.empty()) {
                            auto newEvents = parseSSEBuffer(*it->second);
                            events.insert(events.end(), newEvents.begin(), newEvents.end());
                        }
                        
                        // Check if we need to send Done event
                        bool hasDone = std::any_of(events.begin(), events.end(), [sessionId](const AIEvent& e) {
                            return e.type == AIEvent::Type::Done && e.sessionId == sessionId;
                        });
                        
                        if (!hasDone) {
                            // Add assistant response to history
                            if (!it->second->accumulatedContent.empty()) {
                                chatHistory[sessionId].push_back({"assistant", it->second->accumulatedContent});
                            }
                            // Pass full response content in Done event for persistence
                            events.push_back({AIEvent::Type::Done, sessionId, it->second->accumulatedContent});
                        }
                        
                        fmt::print("AI: Request completed for session {}\n", sessionId);
                    }
                }
                
                toCleanup.push_back(handle);
            }
        }
    }
    
    // Cleanup completed requests
    for (CURL* handle : toCleanup) {
        cleanupRequest(handle);
    }
    
    return events;
}

void AIAgentControllerImpl::clearHistory(uint64_t sessionId) {
    chatHistory.erase(sessionId);
    fmt::print("AI: Cleared history for session {}\n", sessionId);
}
