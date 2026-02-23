#include "AIConfig.h"
//加载tool的配置文件，包含对话prompt模板和可用的工具
bool AIConfig::loadFromFile(const std::string &path)
{
    std::ifstream file(path);
    if(!file.is_open()){
        std::cerr << "[AIConfig] Unable to open configuration file: " << path << std::endl;
        return false;
    }

    json j;
    file >> j;

    //解析对话模版
    if(!j.contains("prompt_template") || !j["prompt_template"].is_string()){
        std::cerr << "[AIConfig] prompt_template is missing" << std::endl;
        return false;
    }
    promptTemplate_ = j["prompt_template"].get<std::string>();

    //解析工具,是对象列表
    if(j.contains("tools") && j["tools"].is_array()){
        for(auto& tool : j["tools"]){
            AITool t;
            t.name = tool.value("name","");
            t.desc = tool.value("desc","");
            // params是个json对象
            if(tool.contains("params") && tool["params"].is_object()){
                //key-value对
                for(auto& [key,val] : tool["params"].items()){
                    t.params[key] = val.get<std::string>();
                }
            }
            tools_.push_back(std::move(t));
        }
    }   
    return true;
}

//替换掉prompt里面的内容
std::string AIConfig::buildPrompt(const std::string &userInput) const
{
    std::string result = promptTemplate_;
    // 两个反斜杠，一个是转义 '\' 本身,告诉编译器这是个字面量
    // 转移成'\'后，形成'\{user_input\}'，
    //这个反斜杠是告诉正则表达式不要把'{}'，{ 是正则表达式的元字符（用于量词，如 a{3}），需要转译
    result = std::regex_replace(result, std::regex("\\{user_input\\}"), userInput);
    result = std::regex_replace(result, std::regex("\\{tool_list\\}"), buildToolList());
    return result;
}

//解析模型回复的消息
AIToolCall AIConfig::parseAIResponse(const std::string &response) const
{
    AIToolCall result;
    try{    
        json j = json::parse(response);

        if(j.contains("tool") && j["tool"].is_string()){
            result.name = j["tool"].get<std::string>();
            if(j.contains("args") && j["args"].is_object()){
                result.args = j["args"];
                 result.isToolCall = true;
            }
        }
    }catch(...){//解析失败那就默认不调用工具
        result.isToolCall = false;
    }
    return result;
}

std::string AIConfig::buildToolResultPrompt(const std::string &userInput, const std::string &toolName, const json &toolArgs, const json &toolResult) const
{
    std::ostringstream oss;
    oss << "下面是用户说的话：" << userInput << "\n"
        << "我刚才调用了工具 [" << toolName << "] ，参数为："
        << toolArgs.dump() << "\n"
        << "工具返回的结果如下：\n" << toolResult.dump(4) << "\n"
        << "请根据以上信息，用自然语言回答用户。";
    return oss.str();
}

std::string AIConfig::buildToolList() const
{
    std::ostringstream oss;
    for(const auto& tool : tools_){
        oss<<tool.name<<"(";
        bool first = true;
        for(const auto& [key,value] : tool.params){
            if(!first){
                oss<<",";
            }
            oss<<key;
            first = false;
        }
        oss<<")->"<<tool.desc<<"\n";
    }
    return oss.str();
}
