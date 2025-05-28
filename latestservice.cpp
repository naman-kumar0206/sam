#include <future>
#include <string>
#include <vector>
#include <iostream>
#include <map>
#include <json/json.h> // Assuming a JSON library
#include "browser_use/agent/views.h"
#include "browser_use/browser/context.h"
#include "browser_use/controller/registry/service.h"
#include "browser_use/controller/views.h"
#include "browser_use/utils.h"

// Assume all blackbox classes are available with same names and methods.
// For example: Page, BrowserContext, Registry, ActionModel, ActionResult, etc.

template<typename Context>
class Controller {
public:
    Registry<Context> registry;

    Controller(std::vector<std::string> exclude_actions = {}, 
               std::type_info* output_model = nullptr) 
        : registry(exclude_actions) 
    {
        // Register all default browser actions

        if (output_model != nullptr) {
            // ExtendedOutputModel is a struct with 'success' and 'data'
            // We'll use a generic struct for this purpose
            struct ExtendedOutputModel {
                bool success = true;
                BaseModel data; // Assume BaseModel is a blackbox base class
            };

            registry.action(
                "Complete task - with return text and if the task is finished (success=True) or not yet  completely finished (success=False), because last step is reached",
                typeid(ExtendedOutputModel),
                [this](ExtendedOutputModel params) -> std::future<ActionResult> {
                    return std::async(std::launch::async, [params]() -> ActionResult {
                        // Exclude success from output JSON
                        Json::Value output_dict = params.data.model_dump(); // Assume model_dump returns Json::Value

                        // Enums are not serializable, convert to string
                        for (auto& key : output_dict.getMemberNames()) {
                            if (output_dict[key].isEnum()) { // Pseudo-code
                                output_dict[key] = output_dict[key].asString(); // Pseudo-code
                            }
                        }
                        std::string extracted_content = output_dict.toStyledString();
                        return ActionResult(true, params.success, extracted_content);
                    });
                }
            );
        } else {
            registry.action(
                "Complete task - with return text and if the task is finished (success=True) or not yet  completely finished (success=False), because last step is reached",
                typeid(DoneAction),
                [](DoneAction params) -> std::future<ActionResult> {
                    return std::async(std::launch::async, [params]() -> ActionResult {
                        return ActionResult(true, params.success, params.text);
                    });
                }
            );
        }

        // Basic Navigation Actions

        registry.action(
            "Search the query in Google in the current tab, the query should be a search query like humans search in Google, concrete and not vague or super long. More the single most important items. ",
            typeid(SearchGoogleAction),
            [](SearchGoogleAction params, BrowserContext& browser) -> std::future<ActionResult> {
                return std::async(std::launch::async, [&params, &browser]() -> ActionResult {
                    Page* page = browser.get_current_page().get(); // Assume get_current_page returns std::unique_ptr<Page>
                    page->goto_("https://www.google.com/search?q=" + params.query + "&udm=14");
                    page->wait_for_load_state();
                    std::string msg = "🔍  Searched for \"" + params.query + "\" in Google";
                    std::cout << msg << std::endl; // logger.info
                    return ActionResult(false, true, msg, true); // include_in_memory=true
                });
            }
        );

        registry.action(
            "Navigate to URL in the current tab",
            typeid(GoToUrlAction),
            [](GoToUrlAction params, BrowserContext& browser) -> std::future<ActionResult> {
                return std::async(std::launch::async, [&params, &browser]() -> ActionResult {
                    Page* page = browser.get_current_page().get();
                    page->goto_(params.url);
                    page->wait_for_load_state();
                    std::string msg = "🔗  Navigated to " + params.url;
                    std::cout << msg << std::endl;
                    return ActionResult(false, true, msg, true);
                });
            }
        );

        registry.action(
            "Go back",
            typeid(NoParamsAction),
            [](NoParamsAction, BrowserContext& browser) -> std::future<ActionResult> {
                return std::async(std::launch::async, [&browser]() -> ActionResult {
                    browser.go_back();
                    std::string msg = "🔙  Navigated back";
                    std::cout << msg << std::endl;
                    return ActionResult(false, true, msg, true);
                });
            }
        );
    }
};
      // Wait for x seconds
        registry.action(
            "Wait for x seconds default 3",
            [this](int seconds = 3) -> std::future<ActionResult> {
                return std::async(std::launch::async, [seconds]() -> ActionResult {
                    std::string msg = "🕒  Waiting for " + std::to_string(seconds) + " seconds";
                    std::cout << msg << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(seconds));
                    return ActionResult(false, true, msg, true);
                });
            }
        );

        // Click element by index
        registry.action(
            "Click element by index",
            typeid(ClickElementAction),
            [](ClickElementAction params, BrowserContext& browser) -> std::future<ActionResult> {
                return std::async(std::launch::async, [&params, &browser]() -> ActionResult {
                    auto session = browser.get_session().get();

                    auto selector_map = browser.get_selector_map().get();
                    if (selector_map.find(params.index) == selector_map.end()) {
                        throw std::runtime_error("Element with index " + std::to_string(params.index) + " does not exist - retry or use alternative actions");
                    }

                    auto element_node = browser.get_dom_element_by_index(params.index).get();
                    int initial_pages = session.context.pages.size();

                    if (browser.is_file_uploader(element_node).get()) {
                        std::string msg = "Index " + std::to_string(params.index) +
                            " - has an element which opens file upload dialog. To upload files please use a specific function to upload files ";
                        std::cout << msg << std::endl;
                        return ActionResult(false, true, msg, true);
                    }

                    std::string msg;
                    try {
                        auto download_path = browser._click_element_node(element_node).get();
                        if (!download_path.empty()) {
                            msg = "💾  Downloaded file to " + download_path;
                        } else {
                            msg = "🖱️  Clicked button with index " + std::to_string(params.index) +
                                  ": " + element_node.get_all_text_till_next_clickable_element(2);
                        }
                        std::cout << msg << std::endl;
                        std::cout << "Element xpath: " << element_node.xpath << std::endl;
                        if (session.context.pages.size() > initial_pages) {
                            std::string new_tab_msg = "New tab opened - switching to it";
                            msg += " - " + new_tab_msg;
                            std::cout << new_tab_msg << std::endl;
                            browser.switch_to_tab(-1).get();
                        }
                        return ActionResult(false, true, msg, true);
                    } catch (const std::exception& e) {
                        std::cout << "Element not clickable with index " << params.index << " - most likely the page changed" << std::endl;
                        return ActionResult(true, false, "", false, e.what());
                    }
                });
            }
        );

        // Input text into a input interactive element
        registry.action(
            "Input text into a input interactive element",
            typeid(InputTextAction),
            [](InputTextAction params, BrowserContext& browser, bool has_sensitive_data = false) -> std::future<ActionResult> {
                return std::async(std::launch::async, [&params, &browser, has_sensitive_data]() -> ActionResult {
                    auto selector_map = browser.get_selector_map().get();
                    if (selector_map.find(params.index) == selector_map.end()) {
                        throw std::runtime_error("Element index " + std::to_string(params.index) + " does not exist - retry or use alternative actions");
                    }
                    auto element_node = browser.get_dom_element_by_index(params.index).get();
                    browser._input_text_element_node(element_node, params.text).get();
                    std::string msg;
                    if (!has_sensitive_data) {
                        msg = "⌨️  Input " + params.text + " into index " + std::to_string(params.index);
                    } else {
                        msg = "⌨️  Input sensitive data into index " + std::to_string(params.index);
                    }
                    std::cout << msg << std::endl;
                    std::cout << "Element xpath: " << element_node.xpath << std::endl;
                    return ActionResult(false, true, msg, true);
                });
            }
        );

        // Save PDF
        registry.action(
            "Save the current page as a PDF file",
            [](BrowserContext& browser) -> std::future<ActionResult> {
                return std::async(std::launch::async, [&browser]() -> ActionResult {
                    auto page = browser.get_current_page().get();
                    std::string short_url = std::regex_replace(page.url, std::regex("^https?://(?:www\\.)?|/$"), "");
                    std::string slug = std::regex_replace(short_url, std::regex("[^a-zA-Z0-9]+"), "-");
                    while (!slug.empty() && slug.front() == '-') slug.erase(slug.begin());
                    while (!slug.empty() && slug.back() == '-') slug.pop_back();
                    std::transform(slug.begin(), slug.end(), slug.begin(), ::tolower);
                    std::string sanitized_filename = slug + ".pdf";

                    page.emulate_media("screen").get();
                    page.pdf(sanitized_filename, "A4", false).get();
                    std::string msg = "Saving page with URL " + page.url + " as PDF to ./" + sanitized_filename;
                    std::cout << msg << std::endl;
                    return ActionResult(false, true, msg, true);
                });
            }
        );

        // Switch tab
        registry.action(
            "Switch tab",
            typeid(SwitchTabAction),
            [](SwitchTabAction params, BrowserContext& browser) -> std::future<ActionResult> {
                return std::async(std::launch::async, [&params, &browser]() -> ActionResult {
                    browser.switch_to_tab(params.page_id).get();
                    auto page = browser.get_agent_current_page().get();
                    page.wait_for_load_state().get();
                    std::string msg = "🔄  Switched to tab " + std::to_string(params.page_id);
                    std::cout << msg << std::endl;
                    return ActionResult(false, true, msg, true);
                });
            }
        );

        // Open url in new tab
        registry.action(
            "Open url in new tab",
            typeid(OpenTabAction),
            [](OpenTabAction params, BrowserContext& browser) -> std::future<ActionResult> {
                return std::async(std::launch::async, [&params, &browser]() -> ActionResult {
                    browser.create_new_tab(params.url).get();
                    browser.get_agent_current_page().get();
                    std::string msg = "🔗  Opened new tab with " + params.url;
                    std::cout << msg << std::endl;
                    return ActionResult(false, true, msg, true);
                });
            }
          };

registry.action(
            "Get all options from a native dropdown",
            [this](int index, BrowserContext& browser) -> std::future<ActionResult> {
                return std::async(std::launch::async, [index, &browser]() -> ActionResult {
                    try {
                        auto page = browser.get_current_page().get();
                        auto selector_map = browser.get_selector_map().get();
                        auto dom_element = selector_map[index];

                        std::vector<std::string> all_options;
                        int frame_index = 0;

                        for (auto& frame : page.frames) {
                            try {
                                // Evaluate JS in the frame to get dropdown options
                                // The lambda and JS are passed as in Python, assuming frame.evaluate exists
                                Json::Value options = frame.evaluate(R"(
                                    (xpath) => {
                                        const select = document.evaluate(xpath, document, null,
                                            XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
                                        if (!select) return null;
                                        return {
                                            options: Array.from(select.options).map(opt => ({
                                                text: opt.text,
                                                value: opt.value,
                                                index: opt.index
                                            })),
                                            id: select.id,
                                            name: select.name
                                        };
                                    }
                                )", dom_element.xpath).get();

                                if (!options.isNull()) {
                                    std::cout << "Found dropdown in frame " << frame_index << std::endl;
                                    std::cout << "Dropdown ID: " << options["id"].asString()
                                              << ", Name: " << options["name"].asString() << std::endl;

                                    std::vector<std::string> formatted_options;
                                    for (const auto& opt : options["options"]) {
                                        // Encode text as JSON string for exact match
                                        Json::FastWriter writer;
                                        std::string encoded_text = writer.write(opt["text"]);
                                        // Remove trailing newline from FastWriter
                                        if (!encoded_text.empty() && encoded_text.back() == '\n')
                                            encoded_text.pop_back();
                                        std::ostringstream oss;
                                        oss << opt["index"].asInt() << ": text=" << encoded_text;
                                        formatted_options.push_back(oss.str());
                                    }
                                    all_options.insert(all_options.end(), formatted_options.begin(), formatted_options.end());
                                }
                            } catch (const std::exception& frame_e) {
                                std::cout << "Frame " << frame_index << " evaluation failed: " << frame_e.what() << std::endl;
                            }
                            frame_index += 1;
                        }

                        std::string msg;
                        if (!all_options.empty()) {
                            std::ostringstream oss;
                            for (const auto& opt : all_options) {
                                oss << opt << "\n";
                            }
                            oss << "Use the exact text string in select_dropdown_option";
                            msg = oss.str();
                            std::cout << msg << std::endl;
                            return ActionResult(false, true, msg, true);
                        } else {
                            msg = "No options found in any frame for dropdown";
                            std::cout << msg << std::endl;
                            return ActionResult(false, true, msg, true);
                        }
                    } catch (const std::exception& e) {
                        std::cout << "Failed to get dropdown options: " << e.what() << std::endl;
                        std::string msg = std::string("Error getting options: ") + e.what();
                        std::cout << msg << std::endl;
                        return ActionResult(false, true, msg, true);
                    }
                });
            }
        );















































#include <future>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <json/json.h> // Or any other JSON library you use

// ... (other necessary includes)

registry.action(
    "Get all options from a native dropdown",
    [this](int index, BrowserContext& browser) -> std::future<ActionResult> {
        return std::async(std::launch::async, [index, &browser]() -> ActionResult {
            try {
                // Get current page and selector map
                auto page = browser.get_current_page().get();
                auto selector_map = browser.get_selector_map().get();
                auto dom_element = selector_map[index];

                std::vector<std::string> all_options;
                int frame_index = 0;

                for (auto& frame : page.frames) {
                    try {
                        // Evaluate JavaScript in the frame to extract dropdown options
                        Json::Value options = frame.evaluate(R"(
                            (xpath) => {
                                const select = document.evaluate(xpath, document, null,
                                    XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
                                if (!select) return null;
                                return {
                                    options: Array.from(select.options).map(opt => ({
                                        text: opt.text,
                                        value: opt.value,
                                        index: opt.index
                                    })),
                                    id: select.id,
                                    name: select.name
                                };
                            }
                        )", dom_element.xpath).get();

                        if (!options.isNull()) {
                            std::cout << "Found dropdown in frame " << frame_index << std::endl;
                            std::cout << "Dropdown ID: " << options["id"].asString()
                                      << ", Name: " << options["name"].asString() << std::endl;

                            for (const auto& opt : options["options"]) {
                                // Encode text as JSON string for exact match
                                Json::FastWriter writer;
                                std::string encoded_text = writer.write(opt["text"]);
                                // Remove trailing newline from FastWriter
                                if (!encoded_text.empty() && encoded_text.back() == '\n')
                                    encoded_text.pop_back();
                                std::ostringstream oss;
                                oss << opt["index"].asInt() << ": text=" << encoded_text;
                                all_options.push_back(oss.str());
                            }
                        }
                    } catch (const std::exception& frame_e) {
                        std::cout << "Frame " << frame_index << " evaluation failed: " << frame_e.what() << std::endl;
                    }
                    frame_index += 1;
                }

                std::string msg;
                if (!all_options.empty()) {
                    std::ostringstream oss;
                    for (const auto& opt : all_options) {
                        oss << opt << "\n";
                    }
                    oss << "Use the exact text string in select_dropdown_option";
                    msg = oss.str();
                    std::cout << msg << std::endl;
                    return ActionResult(false, true, msg, true);
                } else {
                    msg = "No options found in any frame for dropdown";
                    std::cout << msg << std::endl;
                    return ActionResult(false, true, msg, true);
                }
            } catch (const std::exception& e) {
                std::cout << "Failed to get dropdown options: " << e.what() << std::endl;
                std::string msg = std::string("Error getting options: ") + e.what();
                std::cout << msg << std::endl;
                return ActionResult(false, true, msg, true);
            }
        });
    }
);


























// select_dropdown_option
registry.action(
    "Select dropdown option for interactive element index by the text of the option you want to select",
    [this](int index, std::string text, BrowserContext& browser) -> std::future<ActionResult> {
        return std::async(std::launch::async, [index, text, &browser]() -> ActionResult {
            try {
                auto page = browser.get_current_page().get();
                auto selector_map = browser.get_selector_map().get();
                auto dom_element = selector_map[index];

                if (dom_element.tag_name != "select") {
                    std::string msg = "Cannot select option: Element with index " + std::to_string(index) + " is a " + dom_element.tag_name + ", not a select";
                    std::cout << msg << std::endl;
                    return ActionResult(false, true, msg, true);
                }

                std::string xpath = "//" + dom_element.xpath;

                int frame_index = 0;
                for (auto& frame : page.frames) {
                    try {
                        std::cout << "Trying frame " << frame_index << " URL: " << frame.url << std::endl;

                        std::string find_dropdown_js = R"js(
                            (xpath) => {
                                try {
                                    const select = document.evaluate(xpath, document, null,
                                        XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
                                    if (!select) return null;
                                    if (select.tagName.toLowerCase() !== 'select') {
                                        return {
                                            error: `Found element but it's a ${select.tagName}, not a SELECT`,
                                            found: false
                                        };
                                    }
                                    return {
                                        id: select.id,
                                        name: select.name,
                                        found: true,
                                        tagName: select.tagName,
                                        optionCount: select.options.length,
                                        currentValue: select.value,
                                        availableOptions: Array.from(select.options).map(o => o.text.trim())
                                    };
                                } catch (e) {
                                    return {error: e.toString(), found: false};
                                }
                            }
                        )js";

                        auto dropdown_info = frame.evaluate(find_dropdown_js, dom_element.xpath).get();

                        if (dropdown_info && dropdown_info["found"].asBool()) {
                            auto selected_option_values = frame.locator(xpath).nth(0).select_option(text, 1000).get();
                            std::string msg = "selected option " + text + " with value " + selected_option_values;
                            std::cout << msg << " in frame " << frame_index << std::endl;
                            return ActionResult(false, true, msg, true);
                        } else {
                            std::cout << "Frame " << frame_index << " error: " << dropdown_info["error"].asString() << std::endl;
                        }
                    } catch (const std::exception& e) {
                        std::cout << "Frame " << frame_index << " attempt failed: " << e.what() << std::endl;
                        std::cout << "Frame type: " << typeid(frame).name() << std::endl;
                        std::cout << "Frame URL: " << frame.url << std::endl;
                    }
                    frame_index++;
                }

                std::string msg = "Could not select option '" + text + "' in any frame";
                std::cout << msg << std::endl;
                return ActionResult(false, true, msg, true);

            } catch (const std::exception& e) {
                std::string msg = std::string("Selection failed: ") + e.what();
                std::cout << msg << std::endl;
                return ActionResult(true, false, "", false, msg);
            }
        });
    }
);

// drag_drop
registry.action(
    "Drag and drop elements or between coordinates on the page - useful for canvas drawing, sortable lists, sliders, file uploads, and UI rearrangement",
    [this](DragDropAction params, BrowserContext& browser) -> std::future<ActionResult> {
        return std::async(std::launch::async, [params, &browser]() -> ActionResult {
            try {
                auto page = browser.get_current_page().get();

                // Helper lambdas for getting elements and coordinates
                auto get_drag_elements = [&](std::string source_selector, std::string target_selector) -> std::pair<ElementHandle*, ElementHandle*> {
                    ElementHandle* source_element = nullptr;
                    ElementHandle* target_element = nullptr;
                    try {
                        auto source_locator = page.locator(source_selector);
                        auto target_locator = page.locator(target_selector);
                        if (source_locator.count().get() > 0) {
                            source_element = source_locator.first().element_handle().get();
                        }
                        if (target_locator.count().get() > 0) {
                            target_element = target_locator.first().element_handle().get();
                        }
                    } catch (...) {
                        // Log error
                    }
                    return {source_element, target_element};
                };

                auto get_element_coordinates = [&](ElementHandle* source_element, ElementHandle* target_element, Position* source_pos, Position* target_pos) -> std::pair<std::pair<int,int>, std::pair<int,int>> {
                    std::pair<int,int> source_coords = {-1, -1};
                    std::pair<int,int> target_coords = {-1, -1};
                    try {
                        if (source_pos) {
                            source_coords = {source_pos->x, source_pos->y};
                        } else if (source_element) {
                            auto box = source_element->bounding_box().get();
                            if (box) {
                                source_coords = {static_cast<int>(box["x"] + box["width"] / 2), static_cast<int>(box["y"] + box["height"] / 2)};
                            }
                        }
                        if (target_pos) {
                            target_coords = {target_pos->x, target_pos->y};
                        } else if (target_element) {
                            auto box = target_element->bounding_box().get();
                            if (box) {
                                target_coords = {static_cast<int>(box["x"] + box["width"] / 2), static_cast<int>(box["y"] + box["height"] / 2)};
                            }
                        }
                    } catch (...) {
                        // Log error
                    }
                    return {source_coords, target_coords};
                };

                auto execute_drag_operation = [&](int source_x, int source_y, int target_x, int target_y, int steps, int delay_ms) -> std::pair<bool, std::string> {
                    try {
                        page.mouse.move(source_x, source_y).get();
                        page.mouse.down().get();
                        for (int i = 1; i <= steps; ++i) {
                            double ratio = static_cast<double>(i) / steps;
                            int intermediate_x = static_cast<int>(source_x + (target_x - source_x) * ratio);
                            int intermediate_y = static_cast<int>(source_y + (target_y - source_y) * ratio);
                            page.mouse.move(intermediate_x, intermediate_y).get();
                            if (delay_ms > 0) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                            }
                        }
                        page.mouse.move(target_x, target_y).get();
                        page.mouse.move(target_x, target_y).get();
                        page.mouse.up().get();
                        return {true, "Drag operation completed successfully"};
                    } catch (const std::exception& e) {
                        return {false, std::string("Error during drag operation: ") + e.what()};
                    }
                };

                int source_x = -1, source_y = -1, target_x = -1, target_y = -1;
                int steps = params.steps ? params.steps : 10;
                int delay_ms = params.delay_ms ? params.delay_ms : 5;

                if (!params.element_source.empty() && !params.element_target.empty()) {
                    auto [source_element, target_element] = get_drag_elements(params.element_source, params.element_target);
                    if (!source_element || !target_element) {
                        return ActionResult(true, false, "", false, "Failed to find source or target element");
                    }
                    auto [src_coords, tgt_coords] = get_element_coordinates(source_element, target_element, params.element_source_offset.get(), params.element_target_offset.get());
                    if (src_coords.first == -1 || tgt_coords.first == -1) {
                        return ActionResult(true, false, "", false, "Failed to determine source or target coordinates");
                    }
                    source_x = src_coords.first;
                    source_y = src_coords.second;
                    target_x = tgt_coords.first;
                    target_y = tgt_coords.second;
                } else if (params.coord_source_x && params.coord_source_y && params.coord_target_x && params.coord_target_y) {
                    source_x = *params.coord_source_x;
                    source_y = *params.coord_source_y;
                    target_x = *params.coord_target_x;
                    target_y = *params.coord_target_y;
                } else {
                    return ActionResult(true, false, "", false, "Must provide either source/target selectors or source/target coordinates");
                }

                auto [success, message] = execute_drag_operation(source_x, source_y, target_x, target_y, steps, delay_ms);

                if (!success) {
                    return ActionResult(true, false, "", false, message);
                }

                std::string msg;
                if (!params.element_source.empty() && !params.element_target.empty()) {
                    msg = "🖱️ Dragged element '" + params.element_source + "' to '" + params.element_target + "'";
                } else {
                    msg = "🖱️ Dragged from (" + std::to_string(source_x) + ", " + std::to_string(source_y) + ") to (" + std::to_string(target_x) + ", " + std::to_string(target_y) + ")";
                }

                std::cout << msg << std::endl;
                return ActionResult(false, true, msg, true);

            } catch (const std::exception& e) {
                std::string error_msg = std::string("Failed to perform drag and drop: ") + e.what();
                std::cout << error_msg << std::endl;
                return ActionResult(true, false, "", false, error_msg);
            }
        });
    }
);

// get_sheet_contents
registry.action(
    "Google Sheets: Get the contents of the entire sheet",
    [this](BrowserContext& browser) -> std::future<ActionResult> {
        return std::async(std::launch::async, [&browser]() -> ActionResult {
            try {
                auto page = browser.get_current_page().get();

                page.keyboard.press("Enter").get();
                page.keyboard.press("Escape").get();
                page.keyboard.press("ControlOrMeta+A").get();
                page.keyboard.press("ControlOrMeta+C").get();

                auto extracted_tsv = page.evaluate("() => navigator.clipboard.readText()").get();

                return ActionResult(false, true, extracted_tsv, true);
            } catch (const std::exception& e) {
                std::string msg = std::string("Failed to get sheet contents: ") + e.what();
                std::cout << msg << std::endl;
                return ActionResult(true, false, "", false, msg);
            }
        });
    }
);


































 // Google Sheets: Select a specific cell or range of cells
        registry.action(
            "Google Sheets: Select a specific cell or range of cells",
            // domains={"sheets.google.com"},
            [this](BrowserContext& browser, std::string cell_or_range) -> std::future<ActionResult> {
                return std::async(std::launch::async, [&browser, cell_or_range]() -> ActionResult {
                    auto page = browser.get_current_page().get();

                    page.keyboard.press("Enter").get();
                    page.keyboard.press("Escape").get();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    page.keyboard.press("Home").get();
                    page.keyboard.press("ArrowUp").get();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    page.keyboard.press("Control+G").get();
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    page.keyboard.type(cell_or_range, 0.05).get();
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    page.keyboard.press("Enter").get();
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    page.keyboard.press("Escape").get();
                    return ActionResult(false, true, "Selected cell " + cell_or_range, false);
                });
            }
        );

        // Google Sheets: Get the contents of a specific cell or range of cells
        registry.action(
            "Google Sheets: Get the contents of a specific cell or range of cells",
            // domains={"sheets.google.com"},
            [this](BrowserContext& browser, std::string cell_or_range) -> std::future<ActionResult> {
                return std::async(std::launch::async, [this, &browser, cell_or_range]() -> ActionResult {
                    auto page = browser.get_current_page().get();

                    // Call select_cell_or_range
                    this->select_cell_or_range(browser, cell_or_range).get();

                    page.keyboard.press("ControlOrMeta+C").get();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    auto extracted_tsv = page.evaluate("() => navigator.clipboard.readText()").get();
                    return ActionResult(false, true, extracted_tsv, true);
                });
            }
        );

        // Google Sheets: Clear the currently selected cells
        registry.action(
            "Google Sheets: Clear the currently selected cells",
            // domains={"sheets.google.com"},
            [this](BrowserContext& browser) -> std::future<ActionResult> {
                return std::async(std::launch::async, [&browser]() -> ActionResult {
                    auto page = browser.get_current_page().get();
                    page.keyboard.press("Backspace").get();
                    return ActionResult(false, true, "Cleared selected range", false);
                });
            }
        );

        // Google Sheets: Input text into the currently selected cell
        registry.action(
            "Google Sheets: Input text into the currently selected cell",
            // domains={"sheets.google.com"},
            [this](BrowserContext& browser, std::string text) -> std::future<ActionResult> {
                return std::async(std::launch::async, [&browser, text]() -> ActionResult {
                    auto page = browser.get_current_page().get();
                    page.keyboard.type(text, 0.1).get();
                    page.keyboard.press("Enter").get();
                    page.keyboard.press("ArrowUp").get();
                    return ActionResult(false, true, "Inputted text " + text, false);
                });
            }
        );

        // Google Sheets: Batch update a range of cells
        registry.action(
            "Google Sheets: Batch update a range of cells",
            // domains={"sheets.google.com"},
            [this](BrowserContext& browser, std::string range, std::string new_contents_tsv) -> std::future<ActionResult> {
                return std::async(std::launch::async, [this, &browser, range, new_contents_tsv]() -> ActionResult {
                    auto page = browser.get_current_page().get();

                    // Call select_cell_or_range
                    this->select_cell_or_range(browser, range).get();

                    // Simulate paste event from clipboard with TSV content
                    std::string js = "const clipboardData = new DataTransfer();"
                        "clipboardData.setData('text/plain', `" + new_contents_tsv + "`);"
                        "document.activeElement.dispatchEvent(new ClipboardEvent('paste', {clipboardData}));";
                    page.evaluate(js).get();

                    return ActionResult(false, true, "Updated cell " + range + " with " + new_contents_tsv, false);
                });
            }
        );
    }

    // Register
    void action(const std::string& description, /* other params as needed */) {
        // Just forward to registry.action
        registry.action(description /*, ...*/);
    }

    // Act
    ActionResult act(
        ActionModel action,
        BrowserContext& browser_context,
        BaseChatModel* page_extraction_llm = nullptr,
        std::map<std::string, std::string>* sensitive_data = nullptr,
        std::vector<std::string>* available_file_paths = nullptr,
        Context* context = nullptr
    ) {
        // time_execution_sync("--act") // If you have such a macro or function, wrap the body with it

        try {
            auto params_map = action.model_dump(true); // exclude_unset=true
            for (auto& [action_name, params] : params_map) {
                if (params != nullptr) {
                    auto result_future = registry.execute_action(
                        action_name,
                        params,
                        browser_context,
                        page_extraction_llm,
                        sensitive_data,
                        available_file_paths,
                        context
                    );
                    auto result = result_future.get();

                    // Laminar.set_span_output(result); // If you use Laminar, add here

                    if (result.type() == typeid(std::string)) {
                        return ActionResult(false, true, std::any_cast<std::string>(result), false);
                    } else if (result.type() == typeid(ActionResult)) {
                        return std::any_cast<ActionResult>(result);
                    } else if (result.type() == typeid(nullptr)) {
                        return ActionResult();
                    } else {
                        throw std::runtime_error("Invalid action result type");
                    }
                }
            }
            return ActionResult();
        } catch (const std::exception& e) {
            throw e;
        }
    }

    // Helper for select_cell_or_range so it can be called from other lambdas
    std::future<ActionResult> select_cell_or_range(BrowserContext& browser, std::string cell_or_range) {
        return std::async(std::launch::async, [&browser, cell_or_range]() -> ActionResult {
            auto page = browser.get_current_page().get();
            page.keyboard.press("Enter").get();
            page.keyboard.press("Escape").get();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            page.keyboard.press("Home").get();
            page.keyboard.press("ArrowUp").get();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            page.keyboard.press("Control+G").get();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            page.keyboard.type(cell_or_range, 0.05).get();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            page.keyboard.press("Enter").get();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            page.keyboard.press("Escape").get();
            return ActionResult(false, true, "Selected cell " + cell_or_range, false);
        });
    }
};
