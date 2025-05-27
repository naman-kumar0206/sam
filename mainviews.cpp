#include <string>
#include <optional>
#include <map>
#include <any>

// Position model
struct Position {
    int x;
    int y;
};

// Action Input Models

struct SearchGoogleAction {
    std::string query;
};

struct GoToUrlAction {
    std::string url;
};

struct ClickElementAction {
    int index;
    std::optional<std::string> xpath = std::nullopt;
};

struct InputTextAction {
    int index;
    std::string text;
    std::optional<std::string> xpath = std::nullopt;
};

struct DoneAction {
    std::string text;
    bool success;
};

struct SwitchTabAction {
    int page_id;
};

struct OpenTabAction {
    std::string url;
};

struct CloseTabAction {
    int page_id;
};

struct ScrollAction {
    std::optional<int> amount = std::nullopt;  // Number of pixels to scroll, nullopt means scroll by one page
};

struct SendKeysAction {
    std::string keys;
};

struct ExtractPageContentAction {
    std::string value;
};

// Special Case: NoParamsAction — accepts anything and discards
class NoParamsAction {
public:
    // Simulate discarding all inputs like Pydantic's validator
    void ignore_all_inputs(std::map<std::string, std::any>& values) {
        values.clear(); // Discard everything
    }
};

// DragDropAction model
struct DragDropAction {
    // Element-based approach
    std::optional<std::string> element_source = std::nullopt;
    std::optional<std::string> element_target = std::nullopt;
    std::optional<Position> element_source_offset = std::nullopt;
    std::optional<Position> element_target_offset = std::nullopt;

    // Coordinate-based approach
    std::optional<int> coord_source_x = std::nullopt;
    std::optional<int> coord_source_y = std::nullopt;
    std::optional<int> coord_target_x = std::nullopt;
    std::optional<int> coord_target_y = std::nullopt;

    // Common options
    std::optional<int> steps = 10;
    std::optional<int> delay_ms = 5;
};
