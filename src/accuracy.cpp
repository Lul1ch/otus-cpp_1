#include "accuracy.h"

#include <stdexcept>
#include <string>

#include "dataset.h"

namespace fashion_mnist {

EvaluationResult evaluate(const IClassifier& classifier, std::istream& csv) {
    EvaluationResult result;
    CsvDatasetReader reader(csv);
    try {
        Sample sample_example;

        while (auto sample = reader.next(sample_example)) {
            ++result.total;
            if (classifier.predict(sample->pixels) == sample->label) {
                ++result.correct;
            }
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("line " + std::to_string(reader.lineNumber()) + ": " + e.what());
    }
    return result;
}

}  // namespace fashion_mnist
