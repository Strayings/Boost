#include "AICommand.h"

#include <windows.h>
#include <wininet.h>

#include <sstream>
#include <string>
#include <vector>

#include "../../Client/Managers/ModuleManager/ModuleManager.h"
#include "../Client/Client.h"

#pragma comment(lib, "wininet.lib")

static const std::string GEMINI_API_KEY = "AIzaSyA89cJVPY_VTqM8R9kHpByLlKkVK052Uzk";
static const std::string GEMINI_API_URL =
    "https://generativelanguage.googleapis.com/v1beta/models/"
    "gemini-2.5-flash-preview-05-20:generateContent";

using json = nlohmann::json;

static std::string fetchGeminiResponse(const std::string& prompt) {
    std::string userId = "user_123";
    std::string payload = "{\"contents\": [";
    payload += "{\"role\": \"user\", \"parts\": [{\"text\": \"" + prompt + "\"}]}";
    payload += "]}";

    HINTERNET hInternet = InternetOpenA("AIRequest", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if(!hInternet) {
        return "Network error.";
    }

    URL_COMPONENTSA urlComp{sizeof(urlComp)};
    char host[256] = {0}, path[2048] = {0};
    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = _countof(host);
    urlComp.lpszUrlPath = path;
    urlComp.dwUrlPathLength = _countof(path);

    InternetCrackUrlA(GEMINI_API_URL.c_str(), 0, 0, &urlComp);

    HINTERNET hConnect = InternetConnectA(hInternet, host, INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL,
                                          INTERNET_SERVICE_HTTP, 0, 0);
    if(!hConnect) {
        InternetCloseHandle(hInternet);
        return "Connection error.";
    }

    HINTERNET hRequest =
        HttpOpenRequestA(hConnect, "POST", path, NULL, NULL, NULL, INTERNET_FLAG_SECURE, 0);
    if(!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "Request error.";
    }

    std::string headers =
        "Content-Type: application/json\r\nAuthorization: Bearer " + GEMINI_API_KEY + "\r\n";

    if(!HttpSendRequestA(hRequest, headers.c_str(), -1, (LPVOID)payload.c_str(),
                         (DWORD)payload.size())) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "Request sending failed.";
    }

    std::string response;
    char buffer[4096];
    DWORD bytesRead;
    while(InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        response.append(buffer, bytesRead);
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    try {
        json j = json::parse(response);

        if(!j.contains("candidates") || j["candidates"].empty()) {
            return "Error: No candidates in response. Full API response: " + response;
        }

        std::string reply = j["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
        return reply;
    } catch(const std::exception& e) {
        return "Error: Failed to parse Gemini response - " + std::string(e.what()) +
               ". Full response: " + response;
    }
}

AICommand::AICommand() : CommandBase("ai", "Ask Gemini AI", "[prompt]", {"askai", "gemini"}) {}

bool AICommand::execute(const std::vector<std::string>& args) {
    if(args.size() < 2) {
        GI::DisplayClientMessage("%sUsage: %s.ai <prompt>%s", MCTF::RED, MCTF::WHITE, MCTF::RESET);
        return false;
    }

    std::string prompt;
    for(size_t i = 1; i < args.size(); ++i) {
        prompt += args[i] + " ";
    }
    if(!prompt.empty()) {
        prompt.pop_back();
    }

    std::string aiResponse = fetchGeminiResponse(prompt);
    Client::AIMessage(("[AI] " + aiResponse).c_str());

    return true;
}
