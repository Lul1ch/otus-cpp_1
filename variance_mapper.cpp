#include <iostream>
#include <string>
#include <vector>
#include <sstream>

namespace
{

std::vector<std::string> splitCsvLine(const std::string& line)
{
    std::vector<std::string> fields;
    std::string field;
    bool inside_quotes = false;

    for (char c : line)
    {
        if (c == '"')
        {
            inside_quotes = !inside_quotes;
        }
        else if (c == ',' && !inside_quotes)
        {
            fields.push_back(field);
            field.clear();
        }
        else
        {
            field += c;
        }
    }

    fields.push_back(field);
    return fields;
}

} 

int main()
{
    std::string line;
    bool is_header = true;

    while (std::getline(std::cin, line))
    {
        if (is_header)
        {
            is_header = false;
            continue;
        }

        const auto fields = splitCsvLine(line);

        if (fields.size() <= 9)
        {
            continue;
        }

        try
        {
            const double price = std::stod(fields[9]);
            std::cout << "price\t" << price << "\t"
                      << (price * price) << "\n";
        }
        catch (const std::exception&)
        {
            // Пропускаем некорректную строку.
        }
    }

    return 0;
}