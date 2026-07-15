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
    ArgumentParser parser("complex_demo", "Complex argument parsing test");
    parser.set_version("1.0.0");
    // 1. String argument, using const char* (test Default<std::string>(const char*) constructor)
    parser.add_argument<std::string>(
        's', "str", ArgType::Option, "String argument", "hello");

    // 2. String argument, explicit Default<std::string>
    parser.add_argument<std::string>(
        't', "text", ArgType::Option, "Text argument", "world");

    // 3. int argument, default 0
    parser.add_argument<int>(
        'i', "int", ArgType::Option, "Integer argument", 0);

    // 4. double argument, default 0.5
    parser.add_argument<double>(
        'd', "dbl", ArgType::Option, "Floating-point argument", 0.5);

    // 5. bool argument, default false
    parser.add_argument<bool>(
        'b', "bool", ArgType::Option, "Boolean argument", false);

    // Required argument (commented out)
    // parser.add_required<int>("number", "Enter an integer");
    // Optional argument with range constraint
    parser.add_argument<int, Range<int>>(
        'a', "age", ArgType::Option, "Age (18~99)", 20, {18, 88});
    // Optional argument with choices constraint
    parser.add_argument<std::string, Choices<std::string>>(
        'c', "color", ArgType::Option, "Color", "red", Choices<std::string>{{"red", "green", "blue"}});
    // double type argument
    parser.add_optional<double>('m', "discount", "Discount", 0.7);
    parser.add_optional<double>("name", "Discount", 0.0);
    // flag argument
    // Add help argument (at arbitrary position)
    parser.add_help('h', "help", "Show help message");

    parser.add_flag('v', "verbose", "Show verbose output");
    parser.add_flag('x', "xvalue", "Use x");
    parser.add_flag('y', "yvalue", "Use y");
    parser.add_flag("quiet", "Quiet mode");

    // Repeatable argument: -f val1 -f val2 -f val3
    parser.add_repeatable<int>('f', "field", "Database field spec (repeatable, e.g. -f db1:bed:col1 -f db2:vcf:col2)");

    // Positional arguments
    parser.add_positional<std::string>("input", "Input filename");
    parser.add_positional<std::string>("output", "Output filename");

    parser.footnote("Maintainer: RongZh, Email: xxxxxxxxxx@google.com");

    parser.parse_args(argc, argv);

    // Retrieve all argument values
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
    auto fields = parser.get<std::vector<int>>("field");

    std::cout << "------------------- Parse Results -------------------" << std::endl;
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
    std::cout << "fields   : [" << fields.size() << "] ";
    for (const auto &f : fields)
        std::cout << f << " ";
    std::cout << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    return 0;
}
