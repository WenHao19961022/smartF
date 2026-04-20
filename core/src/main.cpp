#include <iostream>
#include <thread>

// 模块函数声明
void ServerConnection() {
    std::cout << "ServerConnection running in thread " << std::this_thread::get_id() << std::endl;
    // 模块A的具体逻辑
}

void Stm32Connection() {
    std::cout << "Stm32Connection running in thread " << std::this_thread::get_id() << std::endl;
    // 模块B的具体逻辑
}

void CvModel() {
    std::cout << "CvModel running in thread " << std::this_thread::get_id() << std::endl;
    // 模块C的具体逻辑
}

void Core() {
    std::cout << "Core running in thread " << std::this_thread::get_id() << std::endl;
    // 模块D的具体逻辑
}

int main() {
    // 创建4个线程，每个线程运行一个模块
    std::thread t1(ServerConnection);
    std::thread t2(Stm32Connection);
    std::thread t3(CvModel);
    std::thread t4(Core);

    // 等待所有线程结束
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    std::cout << "All modules finished." << std::endl;
    return 0;
}