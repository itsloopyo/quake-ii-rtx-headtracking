#include <iostream>

int RunQuakeMathTests();
int RunReticleTests();
int RunConfigTests();
int RunBuildProfileTests();

int main() {
    std::cout << "Quake II RTX Head Tracking Tests\n";
    std::cout << "===============================\n";

    // Sequenced, not summed in one expression: the operands of + have no
    // ordering, so which suite's output came first varied by compiler.
    int failures = RunQuakeMathTests();
    failures += RunReticleTests();
    failures += RunConfigTests();
    failures += RunBuildProfileTests();

    if (failures == 0) {
        std::cout << "All tests passed!\n";
        return 0;
    }
    std::cout << failures << " test(s) failed\n";
    return 1;
}
