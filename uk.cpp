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
    std::cout << "=== Testing ActionRegistry and RegisteredAction ===" << std::endl;

    // Create an ActionRegistry
    ActionRegistry registry;

    // Register a sample action
    RegisteredAction click_action;
    click_action.name = "click_element";
    click_action.description = "Click on a web element";
    click_action.param_model.properties = {{"selector", "string"}, {"index", "int"}};
    click_action.domains = std::vector<std::string>{"*.example.com"};

    // Page filter: only allow pages with 'clickable' in the URL
    click_action.page_filter = [](const Page &page)
    {
        return page.url.find("clickable") != std::string::npos;
    };

    // Register the action
    registry.actions[click_action.name] = click_action;

    // Create a Page object
    Page valid_page("https://www.example.com/clickable");
    Page invalid_page("https://www.example.com/nonclickable");
    Page wrong_domain("https://www.other.com/clickable");

    // Test: Get prompt description for valid page
    std::cout << "\n[Valid Page] Prompt Description:\n";
    std::cout << registry.get_prompt_description(valid_page) << std::endl;

    // Test: Invalid due to page content
    std::cout << "[Invalid Page (no 'clickable')] Prompt Description:\n";
    std::cout << registry.get_prompt_description(invalid_page) << std::endl;

    // Test: Invalid due to domain mismatch
    std::cout << "[Invalid Page (wrong domain)] Prompt Description:\n";
    std::cout << registry.get_prompt_description(wrong_domain) << std::endl;

    // Test: Global prompt with no page context (should skip due to domain/page_filter)
    std::cout << "[No Page Provided] Prompt Description:\n";
    std::cout << registry.get_prompt_description() << std::endl;

    std::cout << "\n=== Testing ActionModel Index Management ===" << std::endl;

    ActionModel model;
    // Simulate adding one action with params
    model.actions["click_element"] = {{"selector", std::string("button.submit")}, {"index", 2}};

    // Print initial index
    model.print_index(); // Should print: Index = 2

    // Set a new index
    model.set_index(5);
    model.print_index(); // Should print: Index = 5

    // Set and get index when no actions exist
    ActionModel empty_model;
    empty_model.print_index(); // Should print: Index not set.
    empty_model.set_index(10); // Should have no effect
    empty_model.print_index(); // Still: Index not set.

    std::cout << "\n=== All Tests Completed Successfully ===" << std::endl;

    return 0;
}
