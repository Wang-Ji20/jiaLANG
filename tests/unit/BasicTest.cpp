#include "Suites.h"
#include "jia/Basic/Diagnostic.h"

namespace jia::test {

const TestSuite basicSuite{
    "basic",
    {
        {"renders location, excerpt and caret",
         [](Tester &t) {
           const Diagnostic diag = makeError({2, 5}, "oops");
           t.expectEq(render(diag, "a.jia", "int x;\nint y z;\n"),
                      std::string("a.jia:2:5: error: oops\n"
                                  "    2 | int y z;\n"
                                  "      |     ^\n"));
         }},
        {"renders without location",
         [](Tester &t) {
           t.expectEq(render(makeError({}, "no input"), "jiac", ""),
                      std::string("jiac: error: no input\n"));
         }},
    }};

} // namespace jia::test
