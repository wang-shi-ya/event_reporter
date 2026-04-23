#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <windows.h>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <ctime>
using namespace std;


/**
 * 事件数据结构
 * 存储事件的基本信息
 */
struct Event {
    string eventName;  // 事件名称
    string eventData;  // 事件数据
    string source;     // 事件来源（程序名称）
    string timestamp;  // 事件时间戳

    /**
     * 构造函数
     * @param name 事件名称
     * @param data 事件数据
     * @param src 事件来源
     */
    Event(const string& name, const string& data, const string& src) : 
        eventName(name), eventData(data), source(src) {
        // 获取当前时间并格式化为字符串
        time_t t = chrono::system_clock::to_time_t(chrono::system_clock::now());
        char buf[100];
        ctime_s(buf, sizeof(buf), &t);  // 安全的时间格式化函数
        timestamp = buf;
        timestamp.pop_back();  // 移除末尾的换行符
    }
};

/**
 * 事件发送器接口
 * 定义事件发送的抽象方法
 */
class IEventSender {
public:
    virtual ~IEventSender() = default;  // 虚析构函数
    virtual void send(const Event&) = 0;  // 发送事件的纯虚方法
};

/**
 * 控制台发送器
 * 将事件输出到控制台
 */
class ConsoleSender : public IEventSender {
public:
    /**
     * 发送事件到控制台
     * @param e 事件对象
     */
    void send(const Event& e) override {
        cout << "[" << e.timestamp << "] [来源:" << e.source << "] 事件: " 
             << e.eventName << " | 数据: " << e.eventData << endl;
    }
};

/**
 * 分类文件发送器
 * 按来源分类存储事件到不同文件
 */
class CategorizedFileSender : public IEventSender {
private:
    string baseDir;  // 基础目录
    mutex mtx;       // 文件操作互斥锁

    /**
     * 获取日志文件路径
     * @param src 事件来源
     * @return 日志文件路径
     */
    string getFile(const string& src) {
        return baseDir + "\\" + src + "_events.log";
    }

public:
    /**
     * 构造函数
     * @param dir 基础目录，默认为当前目录
     */
    CategorizedFileSender(const string& dir = ".") : baseDir(dir) {
        // 创建日志目录（如果不存在）
        CreateDirectoryA(dir.c_str(), NULL);
    }

    /**
     * 发送事件到文件
     * @param e 事件对象
     */
    void send(const Event& e) override {
        lock_guard<mutex> lock(mtx);  // 加锁确保线程安全
        ofstream f(getFile(e.source), ios::app);  // 追加模式打开文件
        if (f.is_open()) {
            f << "[" << e.timestamp << "] 事件: " << e.eventName 
              << " | 数据: " << e.eventData << endl;
            f.close();
        }
    }
};

/**
 * 事件系统
 * 管理事件队列和多线程处理
 */
class EventSystem {
private:
    queue<unique_ptr<Event>> q;  // 事件队列
    mutex qmtx;                  // 队列互斥锁
    condition_variable cv;       // 条件变量
    atomic<bool> running{ true };  // 运行标志
    thread worker;               // 工作线程
    vector<unique_ptr<IEventSender>> senders;  // 事件发送器列表

    /**
     * 处理事件的工作函数
     * 在工作线程中运行
     */
    void process() {
        while (running) {
            unique_ptr<Event> e;
            {
                unique_lock<mutex> lock(qmtx);
                // 等待条件：队列不为空或停止运行
                cv.wait(lock, [this] { return !q.empty() || !running; });
                if (!running && q.empty()) break;  // 停止运行且队列为空，退出循环
                if (!q.empty()) {
                    e = move(q.front());  // 移动语义获取事件
                    q.pop();  // 从队列中移除
                }
            }
            if (e) {  // 如果有事件
                // 通过所有注册的发送器发送事件
                for (auto& s : senders) {
                    s->send(*e);
                }
                cout << "----------------------------------------" << endl;
            }
        }
    }

public:
    /**
     * 构造函数
     * 初始化工作线程
     */
    EventSystem() : worker(&EventSystem::process, this) {}

    /**
     * 析构函数
     * 停止运行并等待工作线程结束
     */
    ~EventSystem() {
        running = false;  // 设置停止标志
        cv.notify_one();  // 通知工作线程
        if (worker.joinable()) worker.join();  // 等待工作线程结束
    }

    /**
     * 注册事件发送器
     * @param s 事件发送器指针
     */
    void registerSender(unique_ptr<IEventSender> s) {
        senders.push_back(move(s));  // 移动语义添加发送器
    }

    /**
     * 上报事件
     * @param n 事件名称
     * @param d 事件数据
     * @param s 事件来源
     */
    void reportEvent(const string& n, const string& d, const string& s) {
        lock_guard<mutex> lock(qmtx);  // 加锁确保线程安全
        q.push(make_unique<Event>(n, d, s));  // 创建事件并加入队列
        cv.notify_one();  // 通知工作线程
    }
};

/**
 * 管道服务器
 * 处理命名管道通信，接收来自其他程序的事件
 */
class PipeServer {
private:
    HANDLE hPipe;  // 管道句柄
    EventSystem* es;  // 事件系统指针
    atomic<bool> running{ true };  // 运行标志
    thread worker;  // 工作线程
    string currentClient = "none";  // 当前连接的客户端

    /**
     * 处理管道消息的工作函数
     * 在工作线程中运行
     */
    void processPipe() {
        char buffer[1024] = { 0 };  // 缓冲区，初始化为0
        DWORD bytesRead = 0;  // 读取的字节数

        while (running) {
            // 创建命名管道
            hPipe = CreateNamedPipeA(
                "\\\\.\\pipe\\EventPipe",  // 管道名称
                PIPE_ACCESS_DUPLEX,  // 双向访问
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,  // 消息模式
                PIPE_UNLIMITED_INSTANCES,  // 无限实例
                1024, 1024, 0, NULL);  // 缓冲区大小和默认超时

            if (hPipe == INVALID_HANDLE_VALUE) {  // 创建失败
                Sleep(100);  // 短暂休眠后重试
                continue;
            }

            // 等待客户端连接
            BOOL connected = ConnectNamedPipe(hPipe, NULL);
            if (connected || GetLastError() == ERROR_PIPE_CONNECTED) {
                // 读取客户端标识
                memset(buffer, 0, sizeof(buffer));
                if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                    string msg(buffer);
                    if (msg.find("PROGRAM:") == 0) {  // 握手消息
                        currentClient = msg.substr(8);  // 提取程序名称
                        cout << "[系统] 当前连接: " << currentClient << endl;
                        // 发送确认消息
                        string ack = "ACK:Connected";
                        DWORD written = 0;
                        WriteFile(hPipe, ack.c_str(), (DWORD)ack.length(), &written, NULL);
                    }
                }

                // 持续接收事件数据
                while (running) {
                    memset(buffer, 0, sizeof(buffer));
                    if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                        string msg(buffer);
                        // 解析JSON格式的事件数据
                        size_t ep = msg.find("\"event\":\"");
                        size_t dp = msg.find("\"data\":\"");
                        if (ep != string::npos && dp != string::npos) {
                            ep += 9;  // 跳过"event":""
                            dp += 8;   // 跳过"data":""
                            string en = msg.substr(ep, msg.find("\"", ep) - ep);  // 提取事件名称
                            string ed = msg.substr(dp, msg.find("\"", dp) - dp);  // 提取事件数据
                            es->reportEvent(en, ed, currentClient);  // 上报事件
                        }
                    }
                    else {
                        cout << "[系统] 客户端 " << currentClient << " 已断开" << endl;
                        cout << "----------------------------------------" << endl;
                        break;
                    }
                }
            }
            // 断开管道连接并关闭句柄
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
            hPipe = INVALID_HANDLE_VALUE;
        }
    }

public:
    /**
     * 构造函数
     * @param system 事件系统指针
     */
    PipeServer(EventSystem* system) : 
        hPipe(INVALID_HANDLE_VALUE), es(system), worker(&PipeServer::processPipe, this) {}

    /**
     * 析构函数
     * 停止运行并等待工作线程结束
     */
    ~PipeServer() {
        running = false;  // 设置停止标志
        if (worker.joinable()) worker.join();  // 等待工作线程结束
    }

    /**
     * 获取当前客户端
     * @return 当前连接的客户端名称
     */
    string getClient() const {
        return currentClient;
    }
};

/**
 * 主函数
 * 程序入口
 */

int main() {

    SetConsoleTitleA("事件监控系统");
    MoveWindow(GetConsoleWindow(), 100, 100, 1000, 700, TRUE);

    // 显示欢迎信息
    cout << "========================================" << endl;
    cout << "          简化版事件上报系统" << endl;
    cout << "========================================" << endl << endl;
    cout << "[状态] 系统就绪，管道: EventPipe" << endl << endl;

    // 初始化事件系统
    EventSystem eventSystem;
    // 注册发送器：控制台和分类文件
    eventSystem.registerSender(make_unique<ConsoleSender>());
    eventSystem.registerSender(make_unique<CategorizedFileSender>("logs"));

    // 初始化管道服务器
    PipeServer pipeServer(&eventSystem);

    // 主循环：监控客户端连接变化
    string lastClient = "";
    while (true) {
        Sleep(1000);  // 每秒钟检查一次
        string client = pipeServer.getClient();
        if (client != lastClient && client != "none") {
            lastClient = client;
            cout << "----------------------------------------" << endl;
        }
    }
    
    return 0;
}