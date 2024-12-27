#include <iostream>
#include <algorithm>
#include <vector>
#include <cmath>
#include <ctime>
#include <winsock2.h>
#include <map>

const int MAP_WIDTH = 100;  // ��ͼ���
const int MAP_HEIGHT = 100;  // ��ͼ�߶�
const double SCALE = 0.1;  // ��������

// ��ֵ����
double fade(double t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}
// �ݶȺ���
double grad(int hash, double x, double y) {
    int h = hash & 15;
    double u = h < 8 ? x : y;
    double v = h < 4 ? y : (h == 12 || h == 14 ? x : 0.0);
    return (h & 1 ? -1 : 1) * (u + v);
}
// ���Բ�ֵ����
double lerp(double t, double a, double b) {
    return a + t * (b - a);
}
// ����Perlin����
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
// ��ʼ��Perlin��������
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

// ��ͼ������
class Tile {
public:
    double height;
    enum TerrainType { WATER, LAND, MOUNTAIN } terrain;

    // Ĭ�Ϲ��캯��
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
// �������λ�ã�ȷ�����ڵ�ͼ��Χ��
void updatePlayerPosition(int& playerX, int& playerY) {
    playerX = my_max(0, my_min(playerX, MAP_WIDTH - 1));
    playerY = my_max(0, my_min(playerY, MAP_HEIGHT - 1));
}

// ����ÿ���ͻ��˵��߳�
void clientHandler(SOCKET clientSocket, std::map<int, std::pair<int, int>>& playerPositions) {
    char buffer[10];
    int playerX = 50, playerY = 50;

    // ���ɵ�ͼ
    std::vector<std::vector<Tile>> map(MAP_HEIGHT, std::vector<Tile>(MAP_WIDTH));
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            map[y][x] = Tile(perlin(x * SCALE, y * SCALE, p));
        }
    }

    // ���͵�ͼ���ͻ���
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        // ��ÿһ�п�ʼǰ�������
        std::cout << std::endl;

        for (int x = 0; x < MAP_WIDTH; ++x) {
            char terrain = ' ';
            switch (map[y][x].terrain) {
            case Tile::WATER: terrain = '~'; break;
            case Tile::LAND: terrain = '.'; break;
            case Tile::MOUNTAIN: terrain = '0'; break;
            }

            // ��������ַ�
            std::cout << terrain;

            // ���͵������ݸ��ͻ���
            send(clientSocket, &terrain, sizeof(terrain), 0);
        }
    }


    while (true) {
        int received = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (received == SOCKET_ERROR || received == 0) {
            std::cerr << "Error receiving data from client." << std::endl;
            break;
        }

        // ��Ұ�������
        if (buffer[0] == 'W') playerY--;
        else if (buffer[0] == 'S') playerY++;
        else if (buffer[0] == 'A') playerX--;
        else if (buffer[0] == 'D') playerX++;

        // �������λ��
        updatePlayerPosition(playerX, playerY);

        // ������ҵ����굽ӳ����
        playerPositions[clientSocket] = std::make_pair(playerX, playerY);

        // ��ͻ��˷����������
        sprintf_s(buffer, "%d %d", playerX, playerY);
        send(clientSocket, buffer, strlen(buffer), 0);
    }

    closesocket(clientSocket);
}

int main() {
    srand(time(0));
    initNoise();

    // ��ʼ��Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed!" << std::endl;
        return 1;
    }

    // �����׽���
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed!" << std::endl;
        WSACleanup();
        return 1;
    }

    // ���÷�������ַ
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(23456);  // �����˿�23456
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");  // ���ӱ���

    // ���׽���
    if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Binding failed!" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // �����ͻ�������
    if (listen(serverSocket, 1) == SOCKET_ERROR) {
        std::cerr << "Listen failed!" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port 23456..." << std::endl;

    // ���ܿͻ�������
    SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Client connection failed!" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Client connected!" << std::endl;

    // ���ɵ�ͼ
    std::vector<std::vector<Tile>> map(MAP_HEIGHT, std::vector<Tile>(MAP_WIDTH));
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            double noise_value = perlin(x * SCALE, y * SCALE, p);
            map[y][x] = Tile(noise_value);  // Ϊÿ�����ӷ���߶Ⱥ͵�������
        }
    }

    // ��ͻ��˷��͵�ͼ����
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        // ������ͼ��һ��
        std::string line;
        for (int x = 0; x < MAP_WIDTH; ++x) {
            char terrainType = (map[y][x].terrain == Tile::WATER) ? '~' :
                (map[y][x].terrain == Tile::MOUNTAIN) ? '0' : '.';
            line += terrainType;  // ����ǰ���ӵĵ����ַ��ӵ����ַ�����
        }

        // ������������
        send(clientSocket, line.c_str(), line.size(), 0);
    }



    // ���λ��
    int playerX = 50, playerY = 50;
    char buffer[10];

    // ��������������ͻ�������
    while (true) {
        // ������Ұ�������
        int received = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (received == SOCKET_ERROR) {
            std::cerr << "Error receiving data." << std::endl;
            break;
        }
        if (received == 0) {
            // �ͻ��˹ر�����
            break;
        }

        // ������������ƶ�
        if (buffer[0] == 'W') playerY--;  // ��
        else if (buffer[0] == 'S') playerY++;  // ��
        else if (buffer[0] == 'A') playerX--;  // ��
        else if (buffer[0] == 'D') playerX++;  // ��

        // �������λ�ã�ȷ�����ڵ�ͼ��Χ��
        updatePlayerPosition(playerX, playerY);

        // �������λ�ø��ͻ���
        sprintf(buffer, "%d %d", playerX, playerY);
        send(clientSocket, buffer, strlen(buffer), 0);
    }

    // ����ͻ���
    std::map<int, std::pair<int, int>> playerPositions;
    clientHandler(clientSocket, playerPositions);

    // �رտͻ��˺ͷ������׽���
    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}
