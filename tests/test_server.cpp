#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

class TestClient 
{
public:
    TestClient(const std::string& server_ip, int tcp_port, int udp_port)
        : server_ip_(server_ip), tcp_port_(tcp_port), udp_port_(udp_port) 
    {
    }

    void runTests() 
    {
        std::cout << "\n=== Network Server Test Client ===\n\n";
        
        std::cout << "1. Testing TCP connection...\n";
        testTcp();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::cout << "\n2. Testing UDP connection...\n";
        testUdp();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::cout << "\n3. Running stress test...\n";
        stressTest();
        
        std::cout << "\n=== All tests completed ===\n";
    }

private:
    void testTcp() 
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) 
        {
            std::cerr << "\tFailed to create TCP socket\n";
            return;
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(tcp_port_);
        inet_pton(AF_INET, server_ip_.c_str(), &server_addr.sin_addr);

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) 
        {
            std::cerr << "\tFailed to connect to TCP server\n";
            close(sock);
            return;
        }

        std::cout << "\tConnected to TCP server\n";

        sendAndReceive(sock, "Hello, Server!", "\tTesting echo: ");
        
        sendAndReceive(sock, "/time", "\tTesting /time: ");
        
        sendAndReceive(sock, "/stats", "\tTesting /stats: ");
        
        sendAndReceive(sock, "/unknown", "\tTesting unknown cmd: ");

        close(sock);
        std::cout << "\tTCP tests completed\n";
    }

    void testUdp() 
    {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) 
        {
            std::cerr << "\tFailed to create UDP socket\n";
            return;
        }

        struct sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(udp_port_);
        inet_pton(AF_INET, server_ip_.c_str(), &server_addr.sin_addr);

        std::cout << "\tUDP socket created\n";

        sendAndReceiveUdp(sock, &server_addr, "UDP Hello!", "\tTesting echo: ");
        
        sendAndReceiveUdp(sock, &server_addr, "/time", "\tTesting /time: ");
        
        sendAndReceiveUdp(sock, &server_addr, "/stats", "\tTesting /stats: ");

        close(sock);
        std::cout << "\tUDP tests completed\n";
    }

    void stressTest() 
    {
        std::cout << "\tStarting stress test with 5 concurrent connections...\n";
        
        std::vector<std::thread> threads;
        
        for (int i = 0; i < 5; ++i) 
        {
            threads.emplace_back([this, i]() 
            {
                int sock = socket(AF_INET, SOCK_STREAM, 0);
                if (sock < 0) return;

                struct sockaddr_in server_addr{};
                server_addr.sin_family = AF_INET;
                server_addr.sin_port = htons(tcp_port_);
                inet_pton(AF_INET, server_ip_.c_str(), &server_addr.sin_addr);

                if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) 
                {
                    close(sock);
                    return;
                }

                for (int j = 0; j < 10; ++j) 
                {
                    std::string msg = "Client " + std::to_string(i) + " message " + std::to_string(j) + "\n";
                    send(sock, msg.c_str(), msg.length(), 0);
                    
                    char buffer[1024];
                    recv(sock, buffer, sizeof(buffer), 0);
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                std::string cmd = "/stats\n";
                send(sock, cmd.c_str(), cmd.length(), 0);
                
                char buffer[1024];
                int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
                if (bytes > 0) 
                {
                    buffer[bytes] = '\0';
                    if (i == 0) 
                    {
                        std::cout << "\tCurrent stats:\n" << buffer;
                    }
                }

                close(sock);
            });
        }

        for (auto& t : threads) 
        {
            t.join();
        }

        std::cout << "\tStress test completed\n";
    }

    void sendAndReceive(int sock, const std::string& message, const std::string& prefix) 
    {
        std::string msg = message + "\n";
        send(sock, msg.c_str(), msg.length(), 0);
        
        char buffer[1024];
        int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes > 0) 
        {
            buffer[bytes] = '\0';
            std::string response(buffer);
            if (!response.empty() && response.back() == '\n') 
            {
                response.pop_back();
            }
            std::cout << prefix << "'" << message << "' -> '" << response << "'\n";
        }
    }

    void sendAndReceiveUdp(int sock, struct sockaddr_in* server_addr, 
                          const std::string& message, const std::string& prefix) 
    {
        std::string msg = message + "\n";
        sendto(sock, msg.c_str(), msg.length(), 0, 
               (struct sockaddr*)server_addr, sizeof(*server_addr));
        
        char buffer[1024];
        socklen_t addr_len = sizeof(*server_addr);
        int bytes = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                           (struct sockaddr*)server_addr, &addr_len);
        
        if (bytes > 0) 
        {
            buffer[bytes] = '\0';
            std::string response(buffer);
            if (!response.empty() && response.back() == '\n') 
            {
                response.pop_back();
            }
            std::cout << prefix << "'" << message << "' -> '" << response << "'\n";
        }
    }

private:
    std::string server_ip_;
    int tcp_port_;
    int udp_port_;
};

int main(int argc, char* argv[]) 
{
    std::string server_ip = "127.0.0.1";
    int tcp_port = 8080;
    int udp_port = 8081;

    if (argc > 1) server_ip = argv[1];
    if (argc > 2) tcp_port = std::atoi(argv[2]);
    if (argc > 3) udp_port = std::atoi(argv[3]);

    std::cout << "Connecting to server at " << server_ip 
              << " (TCP:" << tcp_port << ", UDP:" << udp_port << ")\n";

    TestClient client(server_ip, tcp_port, udp_port);
    client.runTests();

    return 0;
}
