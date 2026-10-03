#include "TestFramework.h"

int main()
{
    auto& cases = keyflow::test::registry();

    int failedCount = 0;
    for (auto& testCase : cases) {
        keyflow::test::failed() = false;
        keyflow::test::aborted() = false;

        testCase.run();

        if (keyflow::test::failed()) {
            std::cout << "FAIL  " << testCase.name << "\n";
            ++failedCount;
        } else {
            std::cout << "ok    " << testCase.name << "\n";
        }
    }

    std::cout << "\n" << cases.size() << " cases, " << failedCount << " failed\n";
    return failedCount == 0 ? 0 : 1;
}
