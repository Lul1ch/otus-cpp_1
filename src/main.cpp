#include <fstream>
#include <iostream>

#include "accuracy.h"
#include "catboost_classifier.h"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <test.csv> <model.cbm>\n";
        return 1;
    }

    try {
        std::ifstream csv(argv[1]);
        if (!csv) {
            std::cerr << "Cannot open test file: " << argv[1] << '\n';
            return 1;
        }

        const fashion_mnist::CatBoostClassifier model(argv[2]);
        const auto result = fashion_mnist::evaluate(model, csv);

        if (result.total == 0) {
            std::cerr << "Test file is empty\n";
            return 1;
        }
        std::cout << result.accuracy() << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
