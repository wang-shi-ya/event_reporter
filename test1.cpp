#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <iostream>
#include <string>

using namespace std;

/**
 * 将字符串转义为JSON格式
 * @param str 原始字符串
 * @return JSON格式的字符串
 */
string escapeJson(const string& str) {
    string result;
    for (char c : str) {
        if (c == '"') {
            result += "\\\"";
        } else if (c == '\\') {
            result += "\\\\";
        } else if (c == '\n') {
            result += "\\n";
        } else if (c == '\r') {
            result += "\\r";
        } else if (c == '\t') {
            result += "\\t";
        } else {
            result += c;
        }
    }
    return result;
}

/**
 * 构造JSON格式的事件字符串
 * @param eventName 事件名称
 * @param eventData 事件数据
 * @return JSON格式的事件字符串
 */
string buildEventJson(const string& eventName, const string& eventData) {
    return "{\"event\":\"" + escapeJson(eventName) + "\",\"data\":\"" + escapeJson(eventData) + "\"}";
}

int main() {
    HANDLE hPipe;
    DWORD bytesWritten = 0, bytesRead = 0;
    char buffer[1024] = { 0 };
    string pipeName = "\\\\.\\pipe\\EventPipe";

    // 连接到命名管道
    hPipe = CreateFileA(
        pipeName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (hPipe == INVALID_HANDLE_VALUE) {
        cout << "连接失败! 错误代码: " << GetLastError() << endl;
        system("pause");
        return 1;
    }

    cout << "连接成功!" << endl;

    // 发送握手消息
    string handshake = "PROGRAM:test1";
    WriteFile(hPipe, handshake.c_str(), (DWORD)handshake.length(), &bytesWritten, NULL);

    // 读取服务器响应
    memset(buffer, 0, sizeof(buffer));
    if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        cout << "服务器响应: " << buffer << endl;
    }
    else {
        cout << "读取响应失败!" << endl;
    }

    cout << endl;
    cout << "========================================" << endl;
    cout << "开始输入事件（输入 'exit' 退出）" << endl;
    cout << "========================================" << endl << endl;

    // 循环接收用户输入并发送事件
    while (true) {
        string eventName, eventData;

        // 输入事件名称
        cout << "请输入事件名称: ";
        getline(cin, eventName);

        // 检查是否退出
        if (eventName == "exit" || eventName == "quit") {
            cout << "退出程序..." << endl;
            break;
        }

        // 输入事件数据
        cout << "请输入事件数据: ";
        getline(cin, eventData);

        // 构造并发送事件
        string eventJson = buildEventJson(eventName, eventData);
        WriteFile(hPipe, eventJson.c_str(), (DWORD)eventJson.length(), &bytesWritten, NULL);
        cout << "已发送事件: " << eventName << " | 数据: " << eventData << endl;
        cout << "----------------------------------------" << endl << endl;
    }

    // 发送退出事件
    string exitEvent = buildEventJson("app_exit", "用户退出程序");
    WriteFile(hPipe, exitEvent.c_str(), (DWORD)exitEvent.length(), &bytesWritten, NULL);
    cout << "已发送退出事件" << endl;

    CloseHandle(hPipe);
    cout << "连接已关闭." << endl;

    system("pause");
    return 0;
}