#include <iostream>
#include <string>
#include <iomanip>

int main()
{
    double sum = 0.0;
    double sum_squares = 0.0;
    std::size_t count = 0;

    std::string line;

    while (std::getline(std::cin, line))
    {
        const std::size_t first_tab = line.find('\t');

        if (first_tab == std::string::npos)
        {
            continue;
        }

        const std::size_t second_tab = line.find('\t', first_tab + 1);

        if (second_tab == std::string::npos)
        {
            continue;
        }

        try
        {
            const double price = std::stod(
                line.substr(first_tab + 1, second_tab - first_tab - 1));

            const double price_square = std::stod(
                line.substr(second_tab + 1));

            sum += price;
            sum_squares += price_square;
            ++count;
        }
        catch (const std::exception&)
        {
            // Пропускаем некорректное значение.
        }
    }

    if (count == 0)
    {
        std::cerr << "No valid price values\n";
        return 1;
    }

    const double mean = sum / count;
    const double mean_square = sum_squares / count;
    const double variance = mean_square - mean * mean;

    std::cout << std::fixed << std::setprecision(6)
              << variance << "\n";

    return 0;
}