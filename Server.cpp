#include <iostream>
#include <algorithm>
#include <vector>
#include <cmath>
#include <ctime>
#include <winsock2.h>
#include <map>
#include <thread>
#include <mutex>
std::map<int, std::pair<int, int>> playerPositions;
std::mutex playerPositionsMutex;

const int MAP_WIDTH = 100;  // 地图宽度
const int MAP_HEIGHT = 100;  // 地图高度
const double SCALE = 0.1;  // 噪声缩放

// 插值函数
double fade(double t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}
// 梯度函数
double grad(int hash, double x, double y) {
    int h = hash & 15;
    double u = h < 8 ? x : y;
    double v = h < 4 ? y : (h == 12 || h == 14 ? x : 0.0);
    return (h & 1 ? -1 : 1) * (u + v);
}
// 线性插值函数
double lerp(double t, double a, double b) {
    return a + t * (b - a);
}
// 生成Perlin噪声
double perlin(double x, double y, const std::vector<int>& p) {
    int X = (int)floor(x) & 255;
    int Y = (int)floor(y) & 255;
    x -= floor(x);
    y -= floor(y);
    double u = fade(x);
    double v = fade(y);
    int A = p[X] + Y;
    int B = p[X + 1] + Y;

    return lerp(v, lerp(u, grad(p[A], x, y), grad(p[B], x - 1, y)),
        lerp(u, grad(p[A + 1], x, y - 1), grad(p[B + 1], x - 1, y - 1)));
}
// 初始化Perlin噪声数组
std::vector<int> p(512);
void initNoise() {
    for (int i = 0; i < 256; ++i) {
        p[i] = i;
    }
    for (int i = 0; i < 256; ++i) {
        int j = rand() % 256;
        std::swap(p[i], p[j]);
    }
    for (int i = 0; i < 256; ++i) {
        p[i + 256] = p[i];
    }
}

// 地图格子类
class Tile {
public:
    double height;
    enum TerrainType { WATER, LAND, MOUNTAIN } terrain;

    // 默认构造函数
    Tile() : height(0.0), terrain(LAND) {}

    Tile(double h) : height(h), terrain(LAND) {
        if (height < -0.05) terrain = WATER;
        else if (height > 0.2) terrain = MOUNTAIN;
    }

    void print() {
        switch (terrain) {
        case WATER: std::cout << "~ "; break;
        case LAND: std::cout << ". "; break;
        case MOUNTAIN: std::cout << "0 "; break;
        }
    }
};

int my_max(int a, int b) {
    return (a > b) ? a : b;
}
int my_min(int a, int b) {
    return (a < b) ? a : b;
}
// 更新玩家位置，确保其在地图范围内
void updatePlayerPosition(int& playerX, int& playerY) {
    playerX = my_max(0, my_min(playerX, MAP_WIDTH - 1));
    playerY = my_max(0, my_min(playerY, MAP_HEIGHT - 1));
}

void acceptClientConnections(SOCKET serverSocket) {
    while (true) {
        // 接受客户端连接
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Client connection failed!" << std::endl;
            continue;
        }
        std::cout << "Client connected!" << std::endl;

        // 为每个客户端启动一个新线程
        std::thread clientThread(clientHandler, clientSocket, std::ref(playerPositions));
        clientThread.detach();  // 分离线程，使其独立运行，不会阻塞主线程
    }
}

// 处理每个客户端的线程
void clientHandler(SOCKET clientSocket, std::map<int, std::pair<int, int>>& playerPositions) {
    char buffer[100];
    int playerX = 50, playerY = 50;

    // 生成地图
    std::vector<std::vector<Tile>> map(MAP_HEIGHT, std::vector<Tile>(MAP_WIDTH));
    // 初始化噪声
    initNoise();
    // 使用 Perlin 噪声生成地图的地形
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            // 通过 Perlin 噪声生成每个格子的高度值
            double height = perlin(x * SCALE, y * SCALE, p);

            // 根据高度值来确定地形类型
            map[y][x] = Tile(height);  // Tile 会自动根据高度来判断地形
        }
    }

    // 发送地图给客户端
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        // 在每一行开始前输出换行
        std::cout << std::endl;

        // 构建当前行的字符串
        std::string line;

        for (int x = 0; x < MAP_WIDTH; ++x) {
            char terrain = ' ';
            switch (map[y][x].terrain) {
            case Tile::WATER: terrain = '~'; break;
            case Tile::LAND: terrain = '.'; break;
            case Tile::MOUNTAIN: terrain = '0'; break;
            }

            // 将地形字符添加到当前行字符串中
            line += terrain;
        }

        // 输出当前行的地图字符
        std::cout << line;

        // 发送整行数据给客户端
        send(clientSocket, line.c_str(), line.size(), 0);
    }


    while (true) {
        int received = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (received == SOCKET_ERROR || received == 0) {
            std::cerr << "Error receiving data from client." << std::endl;
            break;
        }

        // 玩家按键控制
        if (buffer[0] == 'W') playerY--;
        else if (buffer[0] == 'S') playerY++;
        else if (buffer[0] == 'A') playerX--;
        else if (buffer[0] == 'D') playerX++;
        else if (buffer[0] == 'Q') break;

        // 更新玩家位置
        updatePlayerPosition(playerX, playerY);

        // 使用互斥锁保护对共享资源的访问
        std::lock_guard<std::mutex> lock(playerPositionsMutex);
        playerPositions[clientSocket] = std::make_pair(playerX, playerY);

        // 向客户端发送玩家坐标
        sprintf_s(buffer, "%d %d", playerX, playerY);
        send(clientSocket, buffer, strlen(buffer), 0);
    }

    closesocket(clientSocket);
}

int main() {
    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed!" << std::endl;
        return 1;
    }

    // 创建套接字
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed!" << std::endl;
        WSACleanup();
        return 1;
    }

    // 设置服务器地址
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(23456);  // 监听端口23456
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");  // 连接本机

    // 绑定套接字
    if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Binding failed!" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // 监听客户端连接
    if (listen(serverSocket, 5) == SOCKET_ERROR) {
        std::cerr << "Listen failed!" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port 23456..." << std::endl;

    // 调用多线程接受客户端连接
    acceptClientConnections(serverSocket);

    closesocket(serverSocket);
    WSACleanup();

    return 0;
}