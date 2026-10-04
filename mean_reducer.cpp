#include <iostream>
#include <string>
#include <iomanip>

int main()
{
    double sum = 0.0;
    std::size_t count = 0;

    std::string line;

    while (std::getline(std::cin, line))
    {
        const std::size_t tab_pos = line.find('\t');

        if (tab_pos == std::string::npos)
        {
            continue;
        }

        try
        {
            sum += std::stod(line.substr(tab_pos + 1));
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

    std::cout << std::fixed << std::setprecision(6)
              << (sum / count) << "\n";

    return 0;
}