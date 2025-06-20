#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>
#include <libwebsockets.h>
#include <cstring>
#include <chrono>

using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;

std::string WS_URL_PATH = "";
static struct lws_context *context;
static struct lws *wsi_client = nullptr;
static std::string received_payload;
static int send_counter = 1;
static bool dom_received = false;
static bool command_sent = false;

std::string get_websocket_url_from_chrome() {
    boost::asio::io_context ioc;
    tcp::resolver resolver(ioc);
    boost::beast::tcp_stream stream(ioc);

    auto const results = resolver.resolve("localhost", "9222");
    stream.connect(results);

    http::request<http::string_body> req{http::verb::get, "/json", 11};
    req.set(http::field::host, "localhost");
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    http::write(stream, req);

    boost::beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);
    stream.socket().shutdown(tcp::socket::shutdown_both);

    json j = json::parse(res.body());
    std::string ws_url = j[0]["webSocketDebuggerUrl"];
    return ws_url;
}

std::string build_command(int id, const std::string &method, const json &params = {}) {
    json j;
    j["id"] = id;
    j["method"] = method;
    if (!params.empty()) {
        j["params"] = params;
    }
    return j.dump();
}

static int callback_cdp(struct lws *wsi, enum lws_callback_reasons reason,
                        void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            std::cout << "✅ WebSocket connection established.\n";
            lws_callback_on_writable(wsi);
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            if (!command_sent) {
                json params = {
                    {"expression", "document.documentElement.outerHTML"}
                };
                std::string msg = build_command(send_counter++, "Runtime.evaluate", params);

                std::vector<unsigned char> buf(LWS_PRE + msg.size());
                std::memcpy(buf.data() + LWS_PRE, msg.c_str(), msg.size());
                lws_write(wsi, buf.data() + LWS_PRE, msg.size(), LWS_WRITE_TEXT);
                command_sent = true;
            }
            break;
        }

        case LWS_CALLBACK_CLIENT_RECEIVE: {
            std::string response((const char*)in, len);
            received_payload += response;

            try {
                auto j = json::parse(received_payload);
                if (j.contains("result") && j["result"].contains("result") &&
                    j["result"]["result"].contains("value")) {
                    std::string html = j["result"]["result"]["value"];
                    std::cout << "📜 Full Dynamic HTML:\n" << html << "\n";

                    std::ofstream file("extracted_dom.html");
                    file << html;
                    file.close();

                    dom_received = true;
                    lws_cancel_service(context);
                }
                received_payload.clear();
            } catch (...) {
                // Partial message, wait for more
            }
            break;
        }

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            std::cerr << "❌ Connection error.\n";
            dom_received = true;
            break;

        case LWS_CALLBACK_CLOSED:
            std::cout << "🔒 Connection closed.\n";
            dom_received = true;
            break;

        default:
            break;
    }

    return 0;
}

static struct lws_protocols protocols[] = {
    {
        .name = "cdp-protocol",
        .callback = callback_cdp,
        .per_session_data_size = 0,
        .rx_buffer_size = 65536,
        .id = 0,
        .user = nullptr,
        .tx_packet_size = 0,
    },
    { nullptr, nullptr, 0, 0, 0, nullptr, 0 }
};

void run_websocket() {
    struct lws_context_creation_info info = {};
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;

    context = lws_create_context(&info);
    if (!context) {
        std::cerr << "❌ Failed to create LWS context.\n";
        return;
    }

    struct lws_client_connect_info ccinfo = {};
    ccinfo.context = context;
    ccinfo.address = "localhost";
    ccinfo.port = 9222;
    ccinfo.path = WS_URL_PATH.c_str();
    ccinfo.host = lws_canonical_hostname(context);
    ccinfo.origin = "origin";
    ccinfo.protocol = protocols[0].name;
    ccinfo.ssl_connection = 0;

    wsi_client = lws_client_connect_via_info(&ccinfo);
    if (!wsi_client) {
        std::cerr << "❌ WebSocket connection failed.\n";
        return;
    }

    auto start_time = std::chrono::steady_clock::now();
    while (!dom_received) {
        lws_service(context, 100);

        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(15)) {
            std::cerr << "⏱ Timeout: DOM not received in 15 seconds.\n";
            break;
        }
    }

    lws_context_destroy(context);
}

int main() {
    std::cout << "🔍 Fetching WebSocket URL from Chrome...\n";

    std::string ws_url = get_websocket_url_from_chrome();
    if (ws_url.empty()) {
        std::cerr << "❌ Could not retrieve WebSocket URL.\n";
        return 1;
    }

    std::cout << "✅ WebSocket URL: " << ws_url << "\n";

    std::size_t path_start = ws_url.find("/devtools/");
    if (path_start == std::string::npos) {
        std::cerr << "❌ Invalid WebSocket URL format.\n";
        return 1;
    }

    WS_URL_PATH = ws_url.substr(path_start);
    std::cout << "📎 Extracted WebSocket Path: " << WS_URL_PATH << "\n";

    std::cout << "🚀 Starting WebSocket connection...\n";
    run_websocket();

    std::cout << "✅ Done.\n";
    return 0;
}
