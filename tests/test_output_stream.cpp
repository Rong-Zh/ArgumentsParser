#include "ArgumentParser.h"
#include "SubCommandParser.h"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

TEST(OutputStream, ArgumentParserHelp)
{
    ArgumentParser parser("tool", "description");
    parser.add_help();

    std::ostringstream output;
    parser.print_help(output);

    EXPECT_NE(output.str().find("Usage: tool"), std::string::npos);
}

TEST(OutputStream, ArgumentParserRestoresFormatting)
{
    ArgumentParser parser("tool", "description");
    parser.add_flag('v', "verbose", "verbose output");

    std::ostringstream output;
    output.setf(std::ios::right, std::ios::adjustfield);
    output.fill('*');
    output.precision(3);
    const std::ios::fmtflags flags = output.flags();

    parser.print_help(output);

    EXPECT_EQ(output.flags(), flags);
    EXPECT_EQ(output.fill(), '*');
    EXPECT_EQ(output.precision(), 3);
}

TEST(OutputStream, ArgumentParserVersion)
{
    ArgumentParser parser("tool", "description");
    parser.set_version("1.2.3");

    std::ostringstream output;
    parser.print_version(output);

    EXPECT_EQ(output.str(), "tool 1.2.3\n");
}

TEST(OutputStream, SubCommandParserHelp)
{
    SubCommandParser parser("tool", "description");
    parser.add_subcommand("run", "run a task", [](ArgumentOptions&) { return 0; });

    std::ostringstream output;
    parser.print_help(output);

    EXPECT_NE(output.str().find("Usage: tool <command>"), std::string::npos);
    EXPECT_NE(output.str().find("run"), std::string::npos);
}

TEST(OutputStream, SubCommandParserRestoresFormatting)
{
    SubCommandParser parser("tool", "description");
    parser.add_subcommand("run", "run a task", [](ArgumentOptions&) { return 0; });

    std::ostringstream output;
    output.setf(std::ios::right, std::ios::adjustfield);
    output.fill('*');
    output.precision(3);
    const std::ios::fmtflags flags = output.flags();

    parser.print_help(output);

    EXPECT_EQ(output.flags(), flags);
    EXPECT_EQ(output.fill(), '*');
    EXPECT_EQ(output.precision(), 3);
}

TEST(ParseResult, ArgumentParserBooleanContract)
{
    ArgumentParser parser("tool", "description");
    parser.set_version("1.0.0");
    parser.add_positional<std::string>("input", "input", true);
    parser.add_help();
    parser.add_version();

    char program[] = "tool";
    char input[] = "input.txt";
    char help[] = "--help";
    char version[] = "--version";
    char* normal_argv[] = {program, input};
    char* help_argv[] = {program, help};
    char* version_argv[] = {program, version};

    EXPECT_TRUE(parser.parse_args(2, normal_argv));

    testing::internal::CaptureStdout();
    EXPECT_FALSE(parser.parse_args(2, help_argv));
    testing::internal::GetCapturedStdout();

    testing::internal::CaptureStdout();
    EXPECT_FALSE(parser.parse_args(2, version_argv));
    testing::internal::GetCapturedStdout();
}

TEST(ParseResult, SubCommandParserBooleanContract)
{
    SubCommandParser parser("tool", "description");
    parser.set_version("1.0.0");
    parser.add_version();
    parser.add_subcommand("run", "run a task", [](ArgumentOptions&) { return 0; });

    char program[] = "tool";
    char run[] = "run";
    char help[] = "--help";
    char version[] = "--version";
    char* normal_argv[] = {program, run};
    char* help_argv[] = {program, help};
    char* version_argv[] = {program, version};

    EXPECT_TRUE(parser.parse_args(2, normal_argv));

    testing::internal::CaptureStdout();
    EXPECT_FALSE(parser.parse_args(2, help_argv));
    testing::internal::GetCapturedStdout();

    testing::internal::CaptureStdout();
    EXPECT_FALSE(parser.parse_args(2, version_argv));
    testing::internal::GetCapturedStdout();
}
