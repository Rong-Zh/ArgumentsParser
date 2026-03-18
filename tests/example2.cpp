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

int add(int x, int y) { return x + y; }
int subtract(int x, int y) { return x - y; }

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

// annotate subcommand: demonstrates repeatable -f arguments
int annotate_main(ArgumentOptions &parser)
{
    std::string input = parser.get<std::string>("input");
    std::string output = parser.get<std::string>("output");
    auto fields = parser.get<std::vector<std::string>>("field");

    std::cout << "input : " << input << std::endl;
    std::cout << "output: " << output << std::endl;
    std::cout << "fields: [" << fields.size() << "]" << std::endl;
    for (size_t i = 0; i < fields.size(); ++i)
        std::cout << "  [" << i << "] " << fields[i] << std::endl;
    return 0;
}

int main(int argc, char *argv[])
{
    SubCommandParser cmd("calc", "A simple calculator with subcommands");
    cmd.set_version("1.0.0");
    cmd.footnote("maintainer: xxx email:xxx@example.com\n");

    // add subcommand
    auto &add_parser = cmd.add_subcommand("add", "Add two numbers", add_main);
    add_parser.add_required<int>('a', "first", "First number");
    add_parser.add_required<int>('b', "second", "Second number");
    add_parser.add_help('h', "help", "Show help message");

    // subtract subcommand
    auto &sub_parser = cmd.add_subcommand("subtract", "Subtract two numbers", subtract_main);
    sub_parser.add_required<int>('a', "first", "First number");
    sub_parser.add_required<int>('b', "second", "Second number");
    sub_parser.add_help('h', "help", "Show help message");

    // annotate subcommand: repeatable -f / --field
    // Usage: calc annotate -i in.vcf -o out.vcf -f db1:bed:col1 -f db2:vcf:col2 -f db3:genepred
    auto &ann_parser = cmd.add_subcommand("annotate", "Annotate VCF with multiple databases", annotate_main);
    ann_parser.add_required<std::string>('i', "input", "Input VCF file");
    ann_parser.add_required<std::string>('o', "output", "Output VCF file");
    ann_parser.add_repeatable<std::string>('f', "field", "Database field spec (repeatable): dbname:type[:prefix[:col1,col2]]");
    ann_parser.add_help('h', "help", "Show help message");

    cmd.parse_args(argc, argv);
    return cmd.execute();
}