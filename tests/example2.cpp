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

int add(int x, int y)
{
    return x + y;
}

int subtract(int x, int y)
{
    return x - y;
}

int add_main(ArgumentOptions &parser)
{
    int a = parser.get<int>("first");
    int b = parser.get<int>("second");
    std::cout << "Result: " << add(a, b) << std::endl;
    return 0;
}

int subtract_main(ArgumentOptions &parser)
{
    int a = parser.get<int>("first");
    int b = parser.get<int>("second");
    std::cout << "Result: " << subtract(a, b) << std::endl;
    return 0;
}

int main(int argc, char *argv[])
{
    SubCommandParser cmd("calc", "A simple calculator with subcommands");
    cmd.set_version("1.0.0");
    cmd.footnote("maintainer: xxx email:xxx@example.com\n");

    // push add subcommand
    auto &add_parser = cmd.add_subcommand("add", "Add two numbers", add_main);
    add_parser.add_required<int>('a', "first", "First number");
    add_parser.add_required<int>('b', "second", "Second number");
    add_parser.add_help('h', "help", "Show help message");

    // push subtract subcommand
    auto &sub_parser = cmd.add_subcommand("subtract", "Subtract two numbers", subtract_main);
    sub_parser.add_required<int>('a', "first", "First number");
    sub_parser.add_required<int>('b', "second", "Second number");
    // parse command line arguments
    cmd.parse_args(argc, argv);

    return cmd.execute();
}