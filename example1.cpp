/***
 * @Author: Rong-Zh
 * @Date: 2024-12-10 20:36:01
 * @FilePath: /utils/ArgV2.cpp
 * @LastEditTime: 2025-07-09 19:02:07
 * @Version: 1.0
 * @Description:
 */
#include "ArgumentParser.h"
#include <iostream>

int main(int argc, char *argv[])
{
    ArgumentParser parser("complex_demo", "复杂参数解析测试");
    parser.set_version("1.0.0");
        // 1. 字符串参数，直接用 const char*（测试 Default<std::string>(const char*) 构造）
    parser.add_argument<std::string>(
        's', "str", ArgType::Optional, "字符串参数", "hello");

    // 2. 字符串参数，显式 Default<std::string>
    parser.add_argument<std::string>(
        't', "text", ArgType::Optional, "文本参数", "world");

    // 3. int 参数，默认值为 0
    parser.add_argument<int>(
        'i', "int", ArgType::Optional, "整数参数", 0);

    // 4. double 参数，默认值为 0.5
    parser.add_argument<double>(
        'd', "dbl", ArgType::Optional, "浮点参数", 0.5);

    // 5. bool 参数，默认值为 true
    parser.add_argument<bool>(
        'b', "bool", ArgType::Optional, "布尔参数", false);

    // 必需参数
    parser.add_required<int>("number", "输入一个整数");
    // 可选参数，带范围约束
    parser.add_argument<int, Range<int>>(
        'a', "age", ArgType::Optional, "年龄(18~99)", 20, {18, 88});
    // 可选参数，choices约束
    parser.add_argument<std::string, Choices<std::string>>(
        'c', "color", ArgType::Optional, "颜色", "red", Choices<std::string>{{"red", "green", "blue"}});
    // double类型参数
    parser.add_optional<double>('m', "discount", "折扣", 0.7);
    parser.add_optional<double>("name", "折扣", 0.0);
    // flag参数
    parser.add_flag('v', "verbose", "显示详细信息");
    parser.add_flag('x', "xvalue", "使用x");
    
    parser.add_flag('y', "yvalue", "使用y");

    parser.add_flag("quiet", "静默模式");
    // 位置参数
    parser.add_positional<std::string>("input", "输入文件名");
    parser.add_positional<std::string>("output", "输出文件名");

    parser.footnote("Maintainer: RongZh, Email: xxxxxxxxxx@google.com");

    //bool flag = parser.deprecate("verbose");
    // if (flag)
    // {
    //     std::cout << "The 'verbose' flag is deprecated." << std::endl;
    // }
    parser.print_help();
    parser.parse_args(argc, argv);

    // 获取所有参数的值
    int number = parser.get<int>("number");
    int age = parser.get<int>("age");
    std::string color = parser.get<std::string>("color");
    double discount = parser.get<double>("m");
    double name = parser.get<double>("name");
    bool verbose = parser.get<bool>("verbose");
    bool quiet = parser.get<bool>("quiet");
    std::string input = parser.get<std::string>("input");
    std::string output = parser.get<std::string>("output");
    std::string str = parser.get<std::string>("str");
    std::string text = parser.get<std::string>("text");
    int i = parser.get<int>("i");
    double dbl = parser.get<double>("dbl");
    bool b = parser.get<bool>("bool");
    bool xvalue = parser.get<bool>("xvalue");
    bool yvalue = parser.get<bool>("yvalue");

    std::cout << "------------------- 解析结果 -------------------" << std::endl;
    std::cout << "number   : " << number << std::endl;
    std::cout << "age      : " << age << std::endl;
    std::cout << "color    : " << color << std::endl;
    std::cout << "discount : " << discount << std::endl;
    std::cout << "name     : " << name << std::endl;
    std::cout << "verbose  : " << (verbose ? "true" : "false") << std::endl;
    std::cout << "quiet    : " << (quiet ? "true" : "false") << std::endl;
    std::cout << "input    : " << input << std::endl;
    std::cout << "output   : " << output << std::endl;
    std::cout << "str      : " << str << std::endl;
    std::cout << "text     : " << text << std::endl;
    std::cout << "int      : " << i << std::endl;
    std::cout << "dbl      : " << dbl << std::endl;
    std::cout << "bool     : " << (b ? "true" : "false") << std::endl;
    std::cout << "xvalue   : " << (xvalue ? "true" : "false") << std::endl;
    std::cout << "yvalue   : " << (yvalue ? "true" : "false") << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    return 0;
}