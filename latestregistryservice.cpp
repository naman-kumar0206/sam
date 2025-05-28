#include <future>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <regex>
#include <iostream>
#include <typeinfo>
#include <any>
#include <thread>
#include <chrono>
#include "browser_use/browser/context.h"
#include "browser_use/controller/registry/views.h"
#include "browser_use/telemetry/service.h"
#include "browser_use/telemetry/views.h"
#include "browser_use/utils.h"

template<typename Context>
class Registry {
public:
    ActionRegistry registry;
    ProductTelemetry telemetry;
    std::vector<std::string> exclude_actions;

    Registry(std::vector<std::string> exclude_actions_ = {}) :
        registry(),
        telemetry(),
        exclude_actions(exclude_actions_)
    {}

    // Create a param model from function signature (stub)
    template<typename Func>
    BaseModel* _create_param_model(Func function) {
        // In C++, you would use reflection or manual struct definition.
        // Here, assume you have a way to create a parameter model from a function.
        // Return a pointer to a BaseModel-derived object.
        return new ActionModel(); // Placeholder
    }

    // Action registration
    template<typename Func>
    void action(
        const std::string& description,
        BaseModel* param_model = nullptr,
        std::vector<std::string> domains = {},
        std::function<bool(std::any)> page_filter = nullptr
    ) {
        // Decorator pattern isn't native to C++, so just register the function directly.
        // Assume Func is a std::function or lambda.
        // You can overload this or use std::function with variadic templates for flexibility.
        // This is a stub for illustration.
        // Example usage:
        // registry.action("desc", param_model, domains, page_filter, my_func);

        // Not implemented: wrapping sync functions to async.
        // In C++, you would use std::async as needed when calling.

        // Example registration:
        // RegisteredAction action = ...;
        // registry.actions[func_name] = action;
    }

    // Execute action (async)
    template<typename... Args>
    std::future<std::any> execute_action(
        const std::string& action_name,
        std::map<std::string, std::any> params,
        BrowserContext* browser = nullptr,
        BaseChatModel* page_extraction_llm = nullptr,
        std::map<std::string, std::string>* sensitive_data = nullptr,
        std::vector<std::string>* available_file_paths = nullptr,
        Context* context = nullptr
    ) {
        return std::async(std::launch::async, [=]() -> std::any {
            if (registry.actions.find(action_name) == registry.actions.end()) {
                throw std::runtime_error("Action " + action_name + " not found");
            }
            RegisteredAction& action = registry.actions[action_name];

            // Validate params using the param model (assume validate() exists)
            BaseModel* validated_params = action.param_model->validate(params);

            // Check for sensitive data replacement
            if (sensitive_data) {
                validated_params = this->_replace_sensitive_data(validated_params, *sensitive_data);
            }

            // Prepare extra arguments
            std::map<std::string, std::any> extra_args;
            auto param_names = action.param_model->get_param_names(); // Assume such a method exists

            if (std::find(param_names.begin(), param_names.end(), "context") != param_names.end() && !context) {
                throw std::runtime_error("Action " + action_name + " requires context but none provided.");
            }
            if (std::find(param_names.begin(), param_names.end(), "browser") != param_names.end() && !browser) {
                throw std::runtime_error("Action " + action_name + " requires browser but none provided.");
            }
            if (std::find(param_names.begin(), param_names.end(), "page_extraction_llm") != param_names.end() && !page_extraction_llm) {
                throw std::runtime_error("Action " + action_name + " requires page_extraction_llm but none provided.");
            }
            if (std::find(param_names.begin(), param_names.end(), "available_file_paths") != param_names.end() && !available_file_paths) {
                throw std::runtime_error("Action " + action_name + " requires available_file_paths but none provided.");
            }

            if (std::find(param_names.begin(), param_names.end(), "context") != param_names.end()) {
                extra_args["context"] = context;
            }
            if (std::find(param_names.begin(), param_names.end(), "browser") != param_names.end()) {
                extra_args["browser"] = browser;
            }
            if (std::find(param_names.begin(), param_names.end(), "page_extraction_llm") != param_names.end()) {
                extra_args["page_extraction_llm"] = page_extraction_llm;
            }
            if (std::find(param_names.begin(), param_names.end(), "available_file_paths") != param_names.end()) {
                extra_args["available_file_paths"] = available_file_paths;
            }
            if (action_name == "input_text" && sensitive_data) {
                extra_args["has_sensitive_data"] = true;
            }

            // Call the function (assume all arguments can be passed as a map)
            // In C++, you may need to use std::apply or manual unpacking.
            try {
                return action.function(validated_params, extra_args);
            } catch (const std::exception& e) {
                throw std::runtime_error("Error executing action " + action_name + ": " + e.what());
            }
        });
    }

    // Replace sensitive data
    BaseModel* _replace_sensitive_data(BaseModel* params, const std::map<std::string, std::string>& sensitive_data) {
        std::regex secret_pattern("<secret>(.*?)</secret>");
        std::set<std::string> all_missing_placeholders;

        // Assume params->model_dump() returns a map<string, any>
        auto params_dump = params->model_dump();

        std::function<std::any(const std::any&)> replace_secrets;
        replace_secrets = [&](const std::any& value) -> std::any {
            if (value.type() == typeid(std::string)) {
                std::string str = std::any_cast<std::string>(value);
                std::smatch match;
                std::string result = str;
                std::string::const_iterator searchStart(str.cbegin());
                while (std::regex_search(searchStart, str.cend(), match, secret_pattern)) {
                    std::string placeholder = match[1];
                    if (sensitive_data.find(placeholder) != sensitive_data.end() && !sensitive_data.at(placeholder).empty()) {
                        result.replace(match.position(0), match.length(0), sensitive_data.at(placeholder));
                    } else {
                        all_missing_placeholders.insert(placeholder);
                    }
                    searchStart = match.suffix().first;
                }
                return result;
            } else if (value.type() == typeid(std::map<std::string, std::any>)) {
                std::map<std::string, std::any> obj = std::any_cast<std::map<std::string, std::any>>(value);
                for (auto& [k, v] : obj) {
                    obj[k] = replace_secrets(v);
                }
                return obj;
            } else if (value.type() == typeid(std::vector<std::any>)) {
                std::vector<std::any> arr = std::any_cast<std::vector<std::any>>(value);
                for (auto& v : arr) {
                    v = replace_secrets(v);
                }
                return arr;
            }
            return value;
        };

        auto processed_params = replace_secrets(params_dump);

        // Log missing placeholders
        if (!all_missing_placeholders.empty()) {
            std::cout << "Missing or empty keys in sensitive_data dictionary: ";
            for (const auto& key : all_missing_placeholders) {
                std::cout << key << ", ";
            }
            std::cout << std::endl;
        }

        // Assume model_validate reconstructs a BaseModel from processed_params
        return params->model_validate(processed_params);
    }
};






























// complete
#include <future>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <regex>
#include <iostream>
#include <memory>
#include <type_traits>
#include <any>
#include <optional>
#include "browser_use/browser/context.h"
#include "browser_use/controller/registry/views.h"
#include "browser_use/telemetry/service.h"
#include "browser_use/telemetry/views.h"
#include "browser_use/utils.h"

template<typename Context>
class Registry {
public:
    ActionRegistry registry;
    ProductTelemetry telemetry;
    std::vector<std::string> exclude_actions;

    Registry(const std::vector<std::string>& exclude_actions_ = {})
        : registry(), telemetry(), exclude_actions(exclude_actions_) {}

    // Create a param model from function signature (stub)
    template<typename Func>
    BaseModel* _create_param_model(Func function) {
        // In C++, you would use reflection or manual struct definition.
        // Here, assume you have a way to create a parameter model from a function.
        // Return a pointer to a BaseModel-derived object.
        return new ActionModel(); // Placeholder
    }

    // Action registration (decorator pattern replaced with direct registration)
    template<typename Func>
    void action(
        const std::string& description,
        BaseModel* param_model = nullptr,
        std::vector<std::string> domains = {},
        std::function<bool(std::any)> page_filter = nullptr,
        Func func = nullptr
    ) {
        // Skip registration if action is in exclude_actions
        if (func && std::find(exclude_actions.begin(), exclude_actions.end(), func_name(func)) != exclude_actions.end()) {
            return;
        }
        // Create param model from function if not provided
        BaseModel* actual_param_model = param_model ? param_model : _create_param_model(func);

        // In C++, you may need to wrap sync functions for async execution
        // For simplicity, assume all functions are async-compatible

        RegisteredAction action_obj{
            func_name(func),
            description,
            func,
            actual_param_model,
            domains,
            page_filter
        };
        registry.actions[func_name(func)] = action_obj;
    }

    // Execute action (async)
    std::future<std::any> execute_action(
        const std::string& action_name,
        std::map<std::string, std::any> params,
        BrowserContext* browser = nullptr,
        BaseChatModel* page_extraction_llm = nullptr,
        std::map<std::string, std::string>* sensitive_data = nullptr,
        std::vector<std::string>* available_file_paths = nullptr,
        Context* context = nullptr
    ) {
        return std::async(std::launch::async, [=]() -> std::any {
            if (registry.actions.find(action_name) == registry.actions.end()) {
                throw std::runtime_error("Action " + action_name + " not found");
            }
            RegisteredAction& action = registry.actions[action_name];

            // Validate params using the param model (assume validate() exists)
            BaseModel* validated_params = action.param_model->validate(params);

            // Check for sensitive data replacement
            if (sensitive_data) {
                validated_params = this->_replace_sensitive_data(validated_params, *sensitive_data);
            }

            // Prepare extra arguments
            std::map<std::string, std::any> extra_args;
            auto param_names = action.param_model->get_param_names(); // Assume such a method exists

            if (std::find(param_names.begin(), param_names.end(), "context") != param_names.end() && !context) {
                throw std::runtime_error("Action " + action_name + " requires context but none provided.");
            }
            if (std::find(param_names.begin(), param_names.end(), "browser") != param_names.end() && !browser) {
                throw std::runtime_error("Action " + action_name + " requires browser but none provided.");
            }
            if (std::find(param_names.begin(), param_names.end(), "page_extraction_llm") != param_names.end() && !page_extraction_llm) {
                throw std::runtime_error("Action " + action_name + " requires page_extraction_llm but none provided.");
            }
            if (std::find(param_names.begin(), param_names.end(), "available_file_paths") != param_names.end() && !available_file_paths) {
                throw std::runtime_error("Action " + action_name + " requires available_file_paths but none provided.");
            }

            if (std::find(param_names.begin(), param_names.end(), "context") != param_names.end()) {
                extra_args["context"] = context;
            }
            if (std::find(param_names.begin(), param_names.end(), "browser") != param_names.end()) {
                extra_args["browser"] = browser;
            }
            if (std::find(param_names.begin(), param_names.end(), "page_extraction_llm") != param_names.end()) {
                extra_args["page_extraction_llm"] = page_extraction_llm;
            }
            if (std::find(param_names.begin(), param_names.end(), "available_file_paths") != param_names.end()) {
                extra_args["available_file_paths"] = available_file_paths;
            }
            if (action_name == "input_text" && sensitive_data) {
                extra_args["has_sensitive_data"] = true;
            }

            // Call the function (assume all arguments can be passed as a map)
            try {
                return action.function(validated_params, extra_args);
            } catch (const std::exception& e) {
                throw std::runtime_error("Error executing action " + action_name + ": " + e.what());
            }
        });
    }

    // Replace sensitive data
    BaseModel* _replace_sensitive_data(BaseModel* params, const std::map<std::string, std::string>& sensitive_data) {
        std::regex secret_pattern("<secret>(.*?)</secret>");
        std::set<std::string> all_missing_placeholders;

        // Assume params->model_dump() returns a map<string, any>
        auto params_dump = params->model_dump();

        std::function<std::any(const std::any&)> replace_secrets;
        replace_secrets = [&](const std::any& value) -> std::any {
            if (value.type() == typeid(std::string)) {
                std::string str = std::any_cast<std::string>(value);
                std::smatch match;
                std::string result = str;
                std::string::const_iterator searchStart(str.cbegin());
                while (std::regex_search(searchStart, str.cend(), match, secret_pattern)) {
                    std::string placeholder = match[1];
                    if (sensitive_data.find(placeholder) != sensitive_data.end() && !sensitive_data.at(placeholder).empty()) {
                        result.replace(match.position(0), match.length(0), sensitive_data.at(placeholder));
                    } else {
                        all_missing_placeholders.insert(placeholder);
                    }
                    searchStart = match.suffix().first;
                }
                return result;
            } else if (value.type() == typeid(std::map<std::string, std::any>)) {
                std::map<std::string, std::any> obj = std::any_cast<std::map<std::string, std::any>>(value);
                for (auto& [k, v] : obj) {
                    obj[k] = replace_secrets(v);
                }
                return obj;
            } else if (value.type() == typeid(std::vector<std::any>)) {
                std::vector<std::any> arr = std::any_cast<std::vector<std::any>>(value);
                for (auto& v : arr) {
                    v = replace_secrets(v);
                }
                return arr;
            }
            return value;
        };

        auto processed_params = replace_secrets(params_dump);

        // Log missing placeholders
        if (!all_missing_placeholders.empty()) {
            std::cout << "Missing or empty keys in sensitive_data dictionary: ";
            for (const auto& key : all_missing_placeholders) {
                std::cout << key << ", ";
            }
            std::cout << std::endl;
        }

        // Assume model_validate reconstructs a BaseModel from processed_params
        return params->model_validate(processed_params);
    }

    // Create action model for LLM APIs
    BaseModel* create_action_model(const std::vector<std::string>& include_actions = {}, Page* page = nullptr) {
        std::map<std::string, RegisteredAction> available_actions;
        for (const auto& [name, action] : registry.actions) {
            if (!include_actions.empty() && std::find(include_actions.begin(), include_actions.end(), name) == include_actions.end()) {
                continue;
            }

            if (!page) {
                if (!action.page_filter && action.domains.empty()) {
                    available_actions[name] = action;
                }
                continue;
            }

            bool domain_is_allowed = registry._match_domains(action.domains, page->url);
            bool page_is_allowed = registry._match_page_filter(action.page_filter, page);

            if (domain_is_allowed && page_is_allowed) {
                available_actions[name] = action;
            }
        }

        // Build fields for the model (assume ActionModel supports dynamic field creation)
        std::map<std::string, std::pair<std::optional<BaseModel*>, std::string>> fields;
        for (const auto& [name, action] : available_actions) {
            fields[name] = std::make_pair(std::nullopt, action.description);
        }

        // Telemetry
        std::vector<RegisteredFunction> registered_functions;
        for (const auto& [name, action] : available_actions) {
            registered_functions.push_back(RegisteredFunction{name, action.param_model->model_json_schema()});
        }
        telemetry.capture(ControllerRegisteredFunctionsTelemetryEvent{registered_functions});

        // Assume create_model returns a BaseModel pointer
        return create_model("ActionModel", ActionModel::base(), fields);
    }

    // Get prompt description
    std::string get_prompt_description(Page* page = nullptr) {
        return registry.get_prompt_description(page);
    }

private:
    // Helper to get function name (C++ can't get lambda name directly; use a string or macro in practice)
    template<typename Func>
    std::string func_name(Func func) {
        // In practice, you may need to require the name as a parameter or use typeid(func).name()
        return "func"; // Placeholder
    }
};
