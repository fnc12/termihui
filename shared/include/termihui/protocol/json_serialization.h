#pragma once

#include "client_messages.h"
#include "server_messages.h"
#include <hv/json.hpp>
#include <string>

using json = nlohmann::json;

// ============================================================================
// Client Messages JSON serialization
// ============================================================================

void to_json(json& j, const ExecuteMessage& message);
void from_json(const json& j, ExecuteMessage& message);

void to_json(json& j, const InputMessage& message);
void from_json(const json& j, InputMessage& message);

void to_json(json& j, const CompletionMessage& message);
void from_json(const json& j, CompletionMessage& message);

void to_json(json& j, const ResizeMessage& message);
void from_json(const json& j, ResizeMessage& message);

void to_json(json& j, const ListSessionsMessage& message);
void from_json(const json& j, ListSessionsMessage& message);

void to_json(json& j, const CreateSessionMessage& message);
void from_json(const json& j, CreateSessionMessage& message);

void to_json(json& j, const CloseSessionMessage& message);
void from_json(const json& j, CloseSessionMessage& message);

void to_json(json& j, const GetHistoryMessage& message);
void from_json(const json& j, GetHistoryMessage& message);

void to_json(json& j, const AIChatMessage& message);
void from_json(const json& j, AIChatMessage& message);

void to_json(json& j, const ListLLMProvidersMessage& message);
void from_json(const json& j, ListLLMProvidersMessage& message);

void to_json(json& j, const AddLLMProviderMessage& message);
void from_json(const json& j, AddLLMProviderMessage& message);

void to_json(json& j, const UpdateLLMProviderMessage& message);
void from_json(const json& j, UpdateLLMProviderMessage& message);

void to_json(json& j, const DeleteLLMProviderMessage& message);
void from_json(const json& j, DeleteLLMProviderMessage& message);

void to_json(json& j, const ClientMessage& message);
void from_json(const json& j, ClientMessage& message);

// ============================================================================
// Server Messages JSON serialization
// ============================================================================

void to_json(json& j, const ConnectedMessage& message);
void from_json(const json& j, ConnectedMessage& message);

void to_json(json& j, const ErrorMessage& message);
void from_json(const json& j, ErrorMessage& message);

void to_json(json& j, const OutputMessage& message);
void from_json(const json& j, OutputMessage& message);

void to_json(json& j, const StatusMessage& message);
void from_json(const json& j, StatusMessage& message);

void to_json(json& j, const InputSentMessage& message);
void from_json(const json& j, InputSentMessage& message);

void to_json(json& j, const CompletionResultMessage& message);
void from_json(const json& j, CompletionResultMessage& message);

void to_json(json& j, const ResizeAckMessage& message);
void from_json(const json& j, ResizeAckMessage& message);

void to_json(json& j, const SessionInfo& info);
void from_json(const json& j, SessionInfo& info);

void to_json(json& j, const SessionsListMessage& message);
void from_json(const json& j, SessionsListMessage& message);

void to_json(json& j, const SessionCreatedMessage& message);
void from_json(const json& j, SessionCreatedMessage& message);

void to_json(json& j, const SessionClosedMessage& message);
void from_json(const json& j, SessionClosedMessage& message);

void to_json(json& j, const CommandRecord& record);
void from_json(const json& j, CommandRecord& record);

void to_json(json& j, const HistoryMessage& message);
void from_json(const json& j, HistoryMessage& message);

void to_json(json& j, const CommandStartMessage& message);
void from_json(const json& j, CommandStartMessage& message);

void to_json(json& j, const CommandEndMessage& message);
void from_json(const json& j, CommandEndMessage& message);

void to_json(json& j, const PromptStartMessage& message);
void from_json(const json& j, PromptStartMessage& message);

void to_json(json& j, const PromptEndMessage& message);
void from_json(const json& j, PromptEndMessage& message);

void to_json(json& j, const CwdUpdateMessage& message);
void from_json(const json& j, CwdUpdateMessage& message);

void to_json(json& j, const AIChunkMessage& message);
void from_json(const json& j, AIChunkMessage& message);

void to_json(json& j, const AIDoneMessage& message);
void from_json(const json& j, AIDoneMessage& message);

void to_json(json& j, const AIErrorMessage& message);
void from_json(const json& j, AIErrorMessage& message);

void to_json(json& j, const LLMProviderInfo& info);
void from_json(const json& j, LLMProviderInfo& info);

void to_json(json& j, const LLMProvidersListMessage& message);
void from_json(const json& j, LLMProvidersListMessage& message);

void to_json(json& j, const LLMProviderAddedMessage& message);
void from_json(const json& j, LLMProviderAddedMessage& message);

void to_json(json& j, const LLMProviderUpdatedMessage& message);
void from_json(const json& j, LLMProviderUpdatedMessage& message);

void to_json(json& j, const LLMProviderDeletedMessage& message);
void from_json(const json& j, LLMProviderDeletedMessage& message);

void to_json(json& j, const ServerMessage& message);
void from_json(const json& j, ServerMessage& message);

// ============================================================================
// Helper functions
// ============================================================================

std::string serialize(const ClientMessage& message);
std::string serialize(const ServerMessage& message);
std::string serialize(const ExecuteMessage& message);
std::string serialize(const InputMessage& message);
std::string serialize(const CompletionMessage& message);
std::string serialize(const ResizeMessage& message);
std::string serialize(const ListSessionsMessage& message);
std::string serialize(const CreateSessionMessage& message);
std::string serialize(const CloseSessionMessage& message);
std::string serialize(const GetHistoryMessage& message);
std::string serialize(const ConnectedMessage& message);
std::string serialize(const ErrorMessage& message);
std::string serialize(const OutputMessage& message);
std::string serialize(const StatusMessage& message);
std::string serialize(const InputSentMessage& message);
std::string serialize(const CompletionResultMessage& message);
std::string serialize(const ResizeAckMessage& message);
std::string serialize(const SessionsListMessage& message);
std::string serialize(const SessionCreatedMessage& message);
std::string serialize(const SessionClosedMessage& message);
std::string serialize(const HistoryMessage& message);
std::string serialize(const CommandStartMessage& message);
std::string serialize(const CommandEndMessage& message);
std::string serialize(const PromptStartMessage& message);
std::string serialize(const PromptEndMessage& message);
std::string serialize(const CwdUpdateMessage& message);
std::string serialize(const AIChatMessage& message);
std::string serialize(const AIChunkMessage& message);
std::string serialize(const AIDoneMessage& message);
std::string serialize(const AIErrorMessage& message);
std::string serialize(const ListLLMProvidersMessage& message);
std::string serialize(const AddLLMProviderMessage& message);
std::string serialize(const UpdateLLMProviderMessage& message);
std::string serialize(const DeleteLLMProviderMessage& message);
std::string serialize(const LLMProvidersListMessage& message);
std::string serialize(const LLMProviderAddedMessage& message);
std::string serialize(const LLMProviderUpdatedMessage& message);
std::string serialize(const LLMProviderDeletedMessage& message);

ClientMessage parseClientMessage(const std::string& jsonStr);
ServerMessage parseServerMessage(const std::string& jsonStr);
