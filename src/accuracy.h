#pragma once

#include <cstddef>
#include <istream>

#include "classifier.h"

namespace fashion_mnist {

struct EvaluationResult {
    std::size_t total = 0;
    std::size_t correct = 0;
    double accuracy() const {
        return total == 0 ? 0.0 : static_cast<double>(correct) / static_cast<double>(total);
    }
};

// Читает CSV из потока, предсказывает класс каждого объекта и считает accuracy.
EvaluationResult evaluate(const IClassifier& classifier, std::istream& csv);

}  // namespace fashion_mnist
