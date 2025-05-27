#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <sstream>
#include <regex>
#include <optional>
#include <memory>
#include <map>
#include <algorithm>
#include <cctype>
#include <cassert>

// Simulate a Page class
class Page
{
public:
    std::string url;
    explicit Page(const std::string &url_) : url(url_) {}
};

// Action parameter schema representation
struct ParamModel
{
    std::map<std::string, std::string> properties; // Just for schema printing
    std::optional<int> index;
};

// RegisteredAction definition
struct RegisteredAction
{
    std::string name;
    std::string description;
    std::function<void()> function; // Placeholder for callable
    ParamModel param_model;
    std::optional<std::vector<std::string>> domains; // e.g. ["*.google.com"]
    std::optional<std::function<bool(const Page &)>> page_filter;

    std::string prompt_description() const
    {
        std::ostringstream ss;
        ss << description << ":\n{" << name << ": {";
        for (const auto &[k, v] : param_model.properties)
        {
            if (k != "title")
            {
                ss << k << ": " << v << ", ";
            }
        }
        std::string s = ss.str();
        if (s.size() >= 2)
            s.pop_back(), s.pop_back(); // Remove trailing comma
        s += "}}";
        return s;
    }
};











class ActionModel {
public:
    std::map<std::string, std::map<std::string, std::any>> actions;

    // Get the index of the action (if present)
    std::optional<int> get_index() const {
        for (const auto& [action_name, params] : actions) {
            auto it = params.find("index");
            if (it != params.end() && it->second.type() == typeid(int)) {
                return std::any_cast<int>(it->second);
            }
        }
        return std::nullopt;
    }

    // Set the index of the first action
    void set_index(int index) {
        if (actions.empty()) return;

        auto it = actions.begin(); // First action
        auto& params = it->second;
        params["index"] = index;
    }

    // Debug: print the current index (if any)
    void print_index() const {
        auto index = get_index();
        if (index) {
            std::cout << "Index = " << *index << std::endl;
        } else {
            std::cout << "Index not set." << std::endl;
        }
    }
};





// ActionRegistry definition
class ActionRegistry
{
public:
    std::unordered_map<std::string, RegisteredAction> actions;

    static bool match_domains(const std::optional<std::vector<std::string>> &domains, const std::string &url)
    {
        if (!domains.has_value() || url.empty())
            return true;

        std::regex url_regex(R"((?:https?:\/\/)?([^\/\:]+))");
        std::smatch match;
        std::string domain;

        if (std::regex_search(url, match, url_regex) && match.size() > 1)
        {
            domain = match[1];
        }
        else
        {
            return false;
        }

        for (const std::string &pattern : domains.value())
        {
            std::string regex_pattern = std::regex_replace(pattern, std::regex(R"(\*)"), ".*");
            if (std::regex_match(domain, std::regex(regex_pattern)))
            {
                return true;
            }
        }
        return false;
    }

    static bool match_page_filter(const std::optional<std::function<bool(const Page &)>> &filter, const Page &page)
    {
        if (!filter.has_value())
            return true;
        return filter.value()(page);
    }

    std::string get_prompt_description(const std::optional<Page> &page = std::nullopt)
    {
        std::ostringstream description;

        for (const auto &[name, action] : actions)
        {
            if (!page.has_value())
            {
                // No page provided, include only actions with no filters
                if (!action.page_filter.has_value() && !action.domains.has_value())
                {
                    description << action.prompt_description() << "\n";
                }
            }
            else
            {
                // Page is provided, apply filtering
                if (!action.page_filter.has_value() && !action.domains.has_value())
                {
                    continue; // Skip global actions
                }

                if (match_domains(action.domains, page->url) && match_page_filter(action.page_filter, *page))
                {
                    description << action.prompt_description() << "\n";
                }
            }
        }

        return description.str();
    }
};

// Example usage
int main()
{
    ActionRegistry registry;

    RegisteredAction example_action;
    example_action.name = "click_element";
    example_action.description = "Click on a web element";
    example_action.param_model.properties = {{"selector", "string"}, {"index", "int"}};
    example_action.domains = std::vector<std::string>{"*.example.com"};
    example_action.page_filter = [](const Page &page)
    {
        return page.url.find("clickable") != std::string::npos;
    };

    registry.actions[example_action.name] = example_action;

    Page current_page("https://www.example.com/clickable");
    std::cout << registry.get_prompt_description(current_page);

    return 0;
}
