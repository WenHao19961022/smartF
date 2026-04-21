#include <iostream>
#include <thread>

// 模块函数声明
void ConnectServer() {
    std::cout << "ConnectServer running in thread " << std::this_thread::get_id() << std::endl;
    // 服务器连接线程具体逻辑
}


void ConnectStm32() {
    std::cout << "ConnectStm32 running in thread " << std::this_thread::get_id() << std::endl;
    // stm32连接具体逻辑
}

void CvModel() {
    std::cout << "CvModel running in thread " << std::this_thread::get_id() << std::endl;
    // 视觉模块具体逻辑
}

void Core() {
    std::cout << "Core running in thread " << std::this_thread::get_id() << std::endl;
    // 主线程具体逻辑
}

int main() {
    // 创建4个线程，每个线程运行一个模块
    std::thread t1(ConnectServer);
    std::thread t2(ConnectStm32);
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