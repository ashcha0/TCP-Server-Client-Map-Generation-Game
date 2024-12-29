#include <iostream>
#include <winsock2.h>
#include <thread>
#include <mutex>

const int MAP_WIDTH = 100;
const int MAP_HEIGHT = 100;

std::mutex outputMutex;  // 用于保护输出，避免多线程同时输出时混乱

// 接收数据（包括玩家位置和地图）
void receiveData(SOCKET clientSocket) {
    char buffer[100];
    int bytesReceived;

    while (true) {
        bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';  // 确保字符串终止

            // 加锁以保护输出
            {
                std::lock_guard<std::mutex> guard(outputMutex);
                std::cout << buffer << std::endl;  // 输出接收到的数据
            }
        }
        else if (bytesReceived == 0) {
            std::cout << "Connection closed by server." << std::endl;
            break;
        }
        else {
            std::cerr << "Error receiving data from server." << std::endl;
            break;
        }
    }
}

int main() {
    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed!" << std::endl;
        return 1;
    }

    // 创建套接字
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed!" << std::endl;
        WSACleanup();
        return 1;
    }

    // 设置服务器地址
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(23456);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");  // 连接到本机

    std::cout << "Connecting to server at 127.0.0.1:23456..." << std::endl;

    // 连接到服务器
    if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection to server failed!" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server!" << std::endl;

    // 启动接收线程
    std::thread receiveThread(receiveData, clientSocket);

    // 主线程循环，处理用户输入
    char command;
    while (true) {
        std::cout << "Enter movement command (W/A/S/D) or Q to quit: \n";
        std::cin >> command;
        if (command == 'Q' || command == 'q') {
            std::terminate;  // 退出循环
            closesocket(clientSocket);
            WSACleanup();
            return 0;
        }

        // 向服务器发送命令
        send(clientSocket, &command, sizeof(command), 0);
    }

    // 等待接收线程结束
    receiveThread.join();

    // 关闭套接字和清理资源
    closesocket(clientSocket);
    WSACleanup();
    return 0;
}