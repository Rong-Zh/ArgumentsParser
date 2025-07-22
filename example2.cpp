/***
 * @Author: Rong-Zh
 * @Date: 2025-07-10 22:17:48
 * @FilePath: /ArgumentsParser/example2.cpp
 * @LastEditTime: 2025-07-22 22:01:29
 * @Version: 1.0
 * @Description:
 */

#include "SubCommandParser.h"
#include <iostream>

int add(ArgumentOptions &parser)
{
    int a = parser.get<int>("first");
    int b = parser.get<int>("second");
    return a + b;
}

int subtract(ArgumentOptions &parser)
{
    int a = parser.get<int>("first");
    int b = parser.get<int>("second");
    return a - b;
}

int main(int argc, char *argv[])
{
    SubCommandParser cmd("calc", "A simple calculator with subcommands");

    // 添加 add 子命令
    auto &add_parser = cmd.add_subcommand("add", "Add two numbers", add);
    add_parser.add_required<int>('a', "first", "First number");
    add_parser.add_required<int>('b', "second", "Second number");

    // 添加 subtract 子命令
    auto &sub_parser = cmd.add_subcommand("subtract", "Subtract two numbers", subtract);
    sub_parser.add_required<int>('a', "first", "First number");
    sub_parser.add_required<int>('b', "second", "Second number");
    // 解析命令行参数
    cmd.parse_args(argc, argv);

    int result;
    if (cmd.execute(result))
    {
        std::cout << "Result: " << result << std::endl;
    }    
    return 0;
}