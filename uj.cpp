// registry.hpp

#pragma once
#include <string>
#include <unordered_map>
#include <map>
#include <functional>
#include <vector>
#include <memory>
#include <future>
#include <optional>
#include <iostream>
#include <set>
#include <regex>
#include <type_traits>

// Blackbox classes
class BaseChatModel
{
}; // langchain_core.language_models.chat_models.BaseChatModel
class BrowserContext
{
}; // browser_use.browser.context.BrowserContext
class Page
{
}; // Stub for page object
class ActionModel
{
}; // Base class for param models

// Simulated model schema
struct Parameter
{
    std::string name;
    std::string type;
    std::string defaultValue;
};

using ParameterSchema = std::map<std::string, Parameter>;

// Action metadata
struct RegisteredAction
{
    std::string name;
    std::string description;
    std::function<void()> function; // Placeholder (you can change this to std::function<ReturnType(Args...)>)
    ParameterSchema paramSchema;
    std::vector<std::string> domains;
    std::function<bool(Page *)> pageFilter;
};

// Simulated registry
class ActionRegistry
{
public:
    std::unordered_map<std::string, RegisteredAction> actions;

    bool match_domains(const std::vector<std::string> &domains, const std::string &url)
    {
        if (domains.empty())
            return true;
        for (const auto &domain : domains)
        {
            if (url.find(domain) != std::string::npos)
                return true;
        }
        return false;
    }

    bool match_page_filter(std::function<bool(Page *)> filter, Page *page)
    {
        return filter ? filter(page) : true;
    }

    std::string get_prompt_description(Page *page)
    {
        // Return description of all actions
        std::string desc;
        for (const auto &[name, action] : actions)
        {
            desc += name + ": " + action.description + "\n";
        }
        return desc;
    }
};

// Telemetry placeholder
class ProductTelemetry
{
public:
    void capture(const std::string &event)
    {
        std::cout << "Captured telemetry: " << event << std::endl;
    }
};

// ---------------------------- Registry ------------------------------

template <typename Context>
class Registry
{
public:
    Registry(std::vector<std::string> excludeActions = {}) : excludeActions_(std::move(excludeActions)) {}

    void register_action(
        const std::string &name,
        const std::string &description,
        std::function<void()> func, // Replace with proper signature
        ParameterSchema paramSchema,
        std::vector<std::string> domains = {},
        std::function<bool(Page *)> pageFilter = nullptr)
    {
        if (std::find(excludeActions_.begin(), excludeActions_.end(), name) != excludeActions_.end())
            return;

        RegisteredAction action{name, description, func, paramSchema, domains, pageFilter};
        registry_.actions[name] = action;
    }

    void execute_action(
        const std::string &actionName,
        const std::map<std::string, std::string> &params,
        BrowserContext *browser = nullptr,
        BaseChatModel *llm = nullptr,
        std::map<std::string, std::string> sensitiveData = {},
        std::vector<std::string> filePaths = {},
        Context *context = nullptr)
    {
        if (registry_.actions.find(actionName) == registry_.actions.end())
            throw std::runtime_error("Action not found");

        auto &action = registry_.actions[actionName];
        auto validatedParams = replace_sensitive_data(params, sensitiveData);

        // Simulated check for browser, context, etc.
        if (actionName == "some_browser_action" && !browser)
            throw std::runtime_error("Missing browser for action");

        // Call function (placeholder)
        action.function();
    }

    std::string get_prompt_description(Page *page = nullptr)
    {
        return registry_.get_prompt_description(page);
    }

private:
    ActionRegistry registry_;
    ProductTelemetry telemetry_;
    std::vector<std::string> excludeActions_;

    std::map<std::string, std::string> replace_sensitive_data(
        std::map<std::string, std::string> params,
        const std::map<std::string, std::string> &sensitiveData)
    {
        std::set<std::string> missing;
        std::regex secret_pattern("<secret>(.*?)</secret>");

        for (auto &[key, value] : params)
        {
            std::smatch match;
            while (std::regex_search(value, match, secret_pattern))
            {
                std::string placeholder = match[1];
                auto it = sensitiveData.find(placeholder);
                if (it != sensitiveData.end() && !it->second.empty())
                {
                    value.replace(match.position(0), match.length(0), it->second);
                }
                else
                {
                    missing.insert(placeholder);
                }
            }
        }

        if (!missing.empty())
        {
            std::cerr << "Missing or empty sensitive keys: ";
            for (const auto &m : missing)
                std::cerr << m << " ";
            std::cerr << std::endl;
        }

        return params;
    }
};
