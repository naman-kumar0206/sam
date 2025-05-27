#include <iostream>
#include <string>
#include <vector>
#include <future>
#include <optional>
#include <map>
#include <memory>
#include <functional>

// Placeholder for blackbox external modules
class ActionResult {
public:
    ActionResult(bool done, bool success, std::string content, bool memory = false)
        : is_done(done), success(success), extracted_content(content), include_in_memory(memory) {}

    bool is_done;
    bool success;
    std::string extracted_content;
    bool include_in_memory;
};

class Page {
public:
    void goto_url(const std::string& url) {
        std::cout << "Navigating to: " << url << std::endl;
        // Simulate async page load
    }

    void wait_for_load_state() {
        std::cout << "Waiting for page to load..." << std::endl;
    }
};

class BrowserContext {
public:
    std::shared_ptr<Page> get_current_page() {
        return std::make_shared<Page>();
    }
};

// --- Data Models ---
struct SearchGoogleAction {
    std::string query;
};

struct DoneAction {
    std::string text;
    bool success;
};

// --- Registry Blackbox ---
template<typename Context>
class Registry {
public:
    Registry(const std::vector<std::string>& excluded) {
        excluded_actions = excluded;
    }

    template<typename ParamType>
    void action(const std::string& description,
                std::function<std::future<ActionResult>(ParamType, std::shared_ptr<Context>)> func) {
        // Register the function
        std::cout << "Registered action: " << description << std::endl;
    }

    template<typename ParamType>
    void action(const std::string& description,
                std::function<std::future<ActionResult>(ParamType)> func) {
        // Overload without context
        std::cout << "Registered action: " << description << std::endl;
    }

private:
    std::vector<std::string> excluded_actions;
};

// --- Controller Class ---
template<typename Context>
class Controller {
public:
    Controller(std::vector<std::string> exclude_actions = {}, bool has_output_model = false) {
        registry = std::make_unique<Registry<Context>>(exclude_actions);

        if (has_output_model) {
            // Simulating extended output handling
            registry->action("Complete task with return text and success flag",
                [](auto params) -> std::future<ActionResult> {
                    return std::async(std::launch::async, [=]() {
                        std::string serialized = "{}";  // Simulate JSON dump
                        return ActionResult(true, true, serialized);
                    });
                });
        } else {
            registry->action<DoneAction>(
                "Complete task with return text and success flag",
                [](DoneAction params) -> std::future<ActionResult> {
                    return std::async(std::launch::async, [=]() {
                        return ActionResult(true, params.success, params.text);
                    });
                });
        }

        // Register Google search action
        registry->action<SearchGoogleAction>(
            "Search the query in Google in the current tab...",
            [](SearchGoogleAction params, std::shared_ptr<Context> browser) -> std::future<ActionResult> {
                return std::async(std::launch::async, [=]() {
                    auto page = browser->get_current_page();
                    page->goto_url("https://www.google.com/search?q=" + params.query + "&udm=14");
                    page->wait_for_load_state();
                    std::string msg = "🔍 Searched for \"" + params.query + "\" in Google";
                    std::cout << msg << std::endl;
                    return ActionResult(false, true, msg, true);
                });
            });
    }

private:
    std::unique_ptr<Registry<Context>> registry;
};
