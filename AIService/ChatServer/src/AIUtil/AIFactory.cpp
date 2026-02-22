#include "AIFactory.h"
#include <muduo/base/Logging.h>
StrategyFactory &StrategyFactory::instance()
{
    static StrategyFactory factory;
    return factory;
}

void StrategyFactory::registerStrategy(const std::string &name, Creator creator)
{
    creators[name] = creator;
}

std::shared_ptr<AIStrategy> StrategyFactory::create(const std::string &name)
{
    auto iter = creators.find(name);
    if(iter==creators.end()){
        throw std::runtime_error("Unknown strategy: " + name);
        LOG_ERROR<<"Unknown strategy: " + name;
    }
    return iter->second();
}
