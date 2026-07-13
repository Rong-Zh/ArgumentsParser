#include "SubCommandParser.h"
#include "ArgumentParser.h"

int main()
{
    ArgumentParser parser("tool", "tool");
    SubCommandParser commands("tool", "tool");
    parser.add_optional<int, Range<int>>("number", "number", 1, Range<int>(0, 10));
    ArgumentOptions& options =
        commands.add_subcommand("run", "run", [](ArgumentOptions&) { return 0; });
    options.add_optional<int, Range<int>>("number", "number", 1, Range<int>(0, 10));
    return parser.get_program_name() == commands.get_program_name() ? 0 : 1;
}
