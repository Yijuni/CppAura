#pragma once

#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>
#include <iostream>
#include <chrono>
#include <functional>

/**
 * 用来往mq发送数据
 */
class MQManager {
public:
    static MQManager& instance() {
        static MQManager mgr;
        return mgr;
    }

    /// @brief 多个生产者可以往同一个 queue 发消息
    /// @param queue 路由键，队列名称
    /// @param msg 
    void publish(const std::string& queue, const std::string& msg);

private:
    struct MQConn {
        // channel：AMQP 通道（RabbitMQ 中，一个连接可复用多个通道，但这里每个连接只用一个通道）
        AmqpClient::Channel::ptr_t channel;
        // mtx：每个连接独占一个互斥锁，用于保护该通道的并发写入
        std::mutex mtx;
    };

    MQManager(size_t poolSize = 5,const std::string& host="localhost");

    MQManager(const MQManager&) = delete;
    MQManager& operator=(const MQManager&) = delete;

    std::vector<std::shared_ptr<MQConn>> pool_;
    size_t poolSize_;
    std::atomic<size_t> counter_;
    std::string rabbitmq_host_;
};

/**
 * 用来从mq消费数据
 */
class RabbitMQThreadPool {
public:
    using HandlerFunc = std::function<void(const std::string&)>;

    RabbitMQThreadPool(const std::string& host,
        const std::string& queue,
        int thread_num,
        HandlerFunc handler)
        : stop_(false),
        rabbitmq_host_(host),
        queue_name_(queue),
        thread_num_(thread_num),
        handler_(handler) {}

    void start();
    void shutdown();

    ~RabbitMQThreadPool() {
        shutdown();
    }

private:
    void worker(int id);

private:
    std::vector<std::thread> workers_;
    std::atomic<bool> stop_;
    std::string queue_name_;
    int thread_num_;
    std::string rabbitmq_host_;
    HandlerFunc handler_;
};