#include <iostream>
#include <winsock2.h>
#include <thread>
#include <mutex>

const int MAP_WIDTH = 100;
const int MAP_HEIGHT = 100;

std::mutex outputMutex;  // ���ڱ��������������߳�ͬʱ���ʱ����

// �������ݣ��������λ�ú͵�ͼ��
void receiveData(SOCKET clientSocket) {
    char buffer[100];
    int bytesReceived;

    while (true) {
        bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';  // ȷ���ַ�����ֹ

            // �����Ա������
            {
                std::lock_guard<std::mutex> guard(outputMutex);
                std::cout << buffer << std::endl;  // ������յ�������
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
    // ��ʼ��Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed!" << std::endl;
        return 1;
    }

    // �����׽���
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed!" << std::endl;
        WSACleanup();
        return 1;
    }

    // ���÷�������ַ
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(23456);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");  // ���ӵ�����

    std::cout << "Connecting to server at 127.0.0.1:23456..." << std::endl;

    // ���ӵ�������
    if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection to server failed!" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server!" << std::endl;

    // ���������߳�
    std::thread receiveThread(receiveData, clientSocket);

    // ���߳�ѭ���������û�����
    char command;
    while (true) {
        std::cout << "Enter movement command (W/A/S/D) or Q to quit: \n";
        std::cin >> command;
        if (command == 'Q' || command == 'q') {
            std::terminate;  // �˳�ѭ��
            closesocket(clientSocket);
            WSACleanup();
            return 0;
        }

        // ���������������
        send(clientSocket, &command, sizeof(command), 0);
    }

    // �ȴ������߳̽���
    receiveThread.join();

    // �ر��׽��ֺ�������Դ
    closesocket(clientSocket);
    WSACleanup();
    return 0;
}