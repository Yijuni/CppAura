#pragma once
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include <sstream>
#include <memory>
#include <functional>
#include <unordered_map>
#include <string>


#include"AIStrategy.h"

/**
 * 插件式工厂（Plugin-based Factory）
 * 并不是简单工厂，想要让此工厂返回对应的策略对象，只需要
 * 往注册表注册一个对应策略对象的构造器
 * 特点是可以在运行时动态的添加策略构造器
 */
class StrategyFactory {

public:
    //策略构造器
    using Creator = std::function<std::shared_ptr<AIStrategy>()>;

    static StrategyFactory& instance();

    void registerStrategy(const std::string& name, Creator creator);

    std::shared_ptr<AIStrategy> create(const std::string& name);

private:
    StrategyFactory() = default;
    //这里保存的某种策略对象的创造器，可以根据传入的名称决定调用哪个构造器
    std::unordered_map<std::string, Creator> creators;
};


//传入对应的策略名称，和策略对应的类，就可以注册一个策略构造器到单例工厂内
template<typename T>
struct StrategyRegister {
    StrategyRegister(const std::string& name) {
        StrategyFactory::instance().registerStrategy(name, [] {
            std::shared_ptr<AIStrategy> instance = std::make_shared<T>();
            return instance;
        });
    }
};