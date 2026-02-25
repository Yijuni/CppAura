#include "MQManager.h"
#include <muduo/base/Logging.h>
void MQManager::publish(const std::string &queue, const std::string &msg)
{
    // 原子变量自增1，并返回旧值,Round-Robin 分配请求到不同连接
    size_t index = counter_.fetch_add(1) % poolSize_;
    auto &conn = pool_[index];

    // 每个channel独享锁
    std::unique_lock<std::mutex> lock(conn->mtx);
    // RabbitMQ 不直接接受原始字符串，而是要求消息是 AMQP 协议定义的“消息对象”需要转换
    auto message = AmqpClient::BasicMessage::Create(msg);
    try
    {
        // 默认 交换机，路由键，消息
        conn->channel->BasicPublish("", queue, message);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "消息发送失败：RabbitMQ :" << e.what();
    }
}

MQManager::MQManager(size_t poolSize, const std::string &host)
    : counter_(0), rabbitmq_host_(host),poolSize_(poolSize)
{
    for (int i = 0; i < poolSize_; i++)
    {
        auto conn = std::make_shared<MQConn>();
        // 创建一个连接
        // TODO... 最后一个参数vhost 是 RabbitMQ 的“命名空间隔离”机制，
        //  不同 vhost 中的 queue/exchange 是完全隔离的（即使同名也不冲突）
        conn->channel = AmqpClient::Channel::Create(rabbitmq_host_, 5672, "guest", "guest", "/");
        pool_.push_back(conn);
    }
}

// ------------------- RabbitMQThreadPool -------------------
// 专门用来消费队列消息的
void RabbitMQThreadPool::start()
{
    for (int i = 0; i < thread_num_; ++i)
    {
        workers_.emplace_back(&RabbitMQThreadPool::worker, this, i);
    }
}

void RabbitMQThreadPool::shutdown()
{
    stop_ = true;
    for (auto &t : workers_)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
}

void RabbitMQThreadPool::worker(int id)
{
    // 注意：RabbitMQ是主动推送模式
    // 多个消费者（可以订阅多个队列或多次订阅同一队列）完全可以复用同一个 Channel
    // 不过这里每个线程单独一个channel
    try
    {
        // 单独创建一个channel
        auto channel = AmqpClient::Channel::Create(rabbitmq_host_, 5672, "guest", "guest", "/");

        // 队列名,是否仅检查存在(false：若不存在则创建),是否持久化,是否排他，无消费者时自动删除（不会自动删除）
        channel->DeclareQueue(queue_name_, false, true, false, false);

        /**
         * 一个 Channel 可以同时消费多个队列，
         * consumer_tag 是用来区分不同消费流（消费者）的唯一标识
         * 其实consumer_tag就是不同消费者的标识
         * 向MQ注册我可以消费queue_name的消息，服务器可以推送queue_name的消息了
         * 消费者必须显式调用 BasicAck，否则 RabbitMQ 会认为消息未成功处理
         */
        std::string consumer_tag = channel->BasicConsume(
            queue_name_, // 队列名
            "",          // consumer tag（空表示自动生成）
            true,        // no_ack = false → **需要手动 ACK**
            false,       // exclusive = false → 允许多个消费者,不排他
            false        // no_wait = false → 同步调用
        );

        // QoS (Quality of Service)：控制 RabbitMQ 向消费者推送消息的速度
        // 1 表示：最多预取 1 条未确认的消息
        channel->BasicQos(consumer_tag, 1);

        while (!stop_)
        {
            AmqpClient::Envelope::ptr_t env;

            // BasicConsumeMessage 是阻塞拉取，但带超时（500ms）
            // 消费者本地会有一个缓冲区（由客户端库管理）
            bool ok = channel->BasicConsumeMessage(consumer_tag, env, 500);

            if (ok && env)
            {
                // 获取原始消息
                std::string msg = env->Message()->Body();
                // 执行业务逻辑（如执行SQL）
                handler_(msg);
                // 必须先处理成功，再 ACK(真正执行完成)向服务器发送信息我已经处理完这条消息
                channel->BasicAck(env);
            }
        }
        //向服务器发送取消消费，服务器就不再推送
        channel->BasicCancel(consumer_tag);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Thread " << id << " exception: " << e.what() << std::endl;
        LOG_ERROR << "Thread " << id << " exception: " << e.what();
    }
}
