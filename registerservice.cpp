#include <iostream>
#include <string>
#include <map>
#include <functional>
#include <vector>
#include <stdexcept>
#include <future>
#include <regex>
#include <optional>
#include <set>
#include <nlohmann/json.hpp>

// Using nlohmann::json for Pydantic-like behavior
using json = nlohmann::json;

// Forward declarations for blackbox dependencies
class BrowserContext; // from browser_use.browser.context
class BaseChatModel; // from langchain_core.language_models.chat_models
class ActionModel; // from browser_use.controller.registry.views
class RegisteredAction; // from browser_use.controller.registry.views
class ProductTelemetry; // from browser_use.telemetry.service
class ControllerRegisteredFunctionsTelemetryEvent; // from browser_use.telemetry.views
class RegisteredFunction; // from browser_use.telemetry.views

// Placeholder for timing decorator
#define time_execution_async(label) // no-op

// Forward declaration of registry helpers (blackbox logic)
class ActionRegistry {
public:
    std::map<std::string, RegisteredAction*> actions;
    bool _match_domains(const std::vector<std::string>* domains, const std::string& url);
    bool _match_page_filter(std::function<bool(const json&)> page_filter, const json& page);
    std::string get_prompt_description(std::optional<json> page = std::nullopt);
};

// The actual Registry class

template<typename Context>
class Registry {
public:
    ActionRegistry registry;
    ProductTelemetry telemetry;
    std::vector<std::string> exclude_actions;

    Registry(std::vector<std::string> exclude = {}) : exclude_actions(std::move(exclude)) {}

    std::function<json(json)> action(
        const std::string& description,
        std::optional<std::function<json(json)>> param_model = std::nullopt,
        std::optional<std::vector<std::string>> domains = std::nullopt,
        std::optional<std::function<bool(json)>> page_filter = std::nullopt
    ) {
        return [this, description, param_model, domains, page_filter](std::function<json(json)> func) {
            std::string name = ""; // assume function name will be provided via other means or reflection
            if (std::find(exclude_actions.begin(), exclude_actions.end(), name) != exclude_actions.end()) {
                return func;
            }

            RegisteredAction* action = new RegisteredAction(name, description, func, param_model, domains, page_filter);
            registry.actions[name] = action;
            return func;
        };
    }

    time_execution_async("--execute_action")
    std::future<json> execute_action(
        const std::string& action_name,
        const json& params,
        BrowserContext* browser = nullptr,
        BaseChatModel* page_extraction_llm = nullptr,
        std::optional<std::map<std::string, std::string>> sensitive_data = std::nullopt,
        std::optional<std::vector<std::string>> available_file_paths = std::nullopt,
        Context* context = nullptr
    ) {
        return std::async(std::launch::async, [=]() -> json {
            if (registry.actions.find(action_name) == registry.actions.end()) {
                throw std::runtime_error("Action " + action_name + " not found");
            }

            RegisteredAction* action = registry.actions[action_name];
            json validated_params = params; // Assume validation is done elsewhere

            if (sensitive_data.has_value()) {
                validated_params = _replace_sensitive_data(validated_params, sensitive_data.value());
            }

            // Simulate param injection
            json extra_args;
            if (action->requires_browser() && !browser) throw std::runtime_error("Browser required");
            if (action->requires_llm() && !page_extraction_llm) throw std::runtime_error("LLM required");
            if (action->requires_file_paths() && !available_file_paths.has_value()) throw std::runtime_error("File paths required");
            if (action->requires_context() && !context) throw std::runtime_error("Context required");

            return action->function(validated_params);
        });
    }

    json _replace_sensitive_data(const json& params, const std::map<std::string, std::string>& sensitive_data) {
        std::regex pattern("<secret>(.*?)</secret>");
        std::set<std::string> missing_placeholders;

        std::function<json(const json&)> replacer = [&](const json& val) -> json {
            if (val.is_string()) {
                std::string s = val;
                std::smatch match;
                while (std::regex_search(s, match, pattern)) {
                    std::string placeholder = match[1];
                    auto it = sensitive_data.find(placeholder);
                    if (it != sensitive_data.end()) {
                        s.replace(match.position(0), match.length(0), it->second);
                    } else {
                        missing_placeholders.insert(placeholder);
                    }
                }
                return s;
            } else if (val.is_object() || val.is_array()) {
                json res = val;
                for (auto& el : res.items()) {
                    res[el.key()] = replacer(el.value());
                }
                return res;
            } else {
                return val;
            }
        };

        json result = replacer(params);
        if (!missing_placeholders.empty()) {
            std::cerr << "Missing placeholders: ";
            for (const auto& p : missing_placeholders) std::cerr << p << ", ";
            std::cerr << std::endl;
        }
        return result;
    }

    json create_action_model(std::optional<std::vector<std::string>> include_actions = std::nullopt, std::optional<json> page = std::nullopt) {
        json model;
        std::vector<RegisteredFunction> registered_functions;

        for (const auto& [name, action] : registry.actions) {
            if (include_actions.has_value() && std::find(include_actions->begin(), include_actions->end(), name) == include_actions->end())
                continue;

            if (!page.has_value()) {
                if (!action->has_filter()) {
                    model[name] = action->get_schema();
                    registered_functions.emplace_back(name, action->get_schema());
                }
                continue;
            }

            if (registry._match_domains(action->get_domains(), page->at("url")) && registry._match_page_filter(action->get_page_filter(), page.value())) {
                model[name] = action->get_schema();
                registered_functions.emplace_back(name, action->get_schema());
            }
        }

        telemetry.capture(ControllerRegisteredFunctionsTelemetryEvent(registered_functions));
        return model;
    }

    std::string get_prompt_description(std::optional<json> page = std::nullopt) {
        return registry.get_prompt_description(page);
    }
};
