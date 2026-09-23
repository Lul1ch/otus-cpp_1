#include <boost/filesystem.hpp>
#include <boost/crc.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <cstdint>
#include <numeric>

namespace fs = boost::filesystem;

// ============================================================================
// Конфигурация утилиты
// ============================================================================
struct Config 
{
    std::vector<fs::path> scan_dirs;
    std::vector<fs::path> exclude_dirs;
    int depth_level = -1; // -1 = без ограничений, 0 = только указанная директория
    std::uintmax_t min_file_size = 1;
    std::vector<std::string> filename_masks;
    std::size_t block_size = 4096;
    bool use_md5 = false; // false = CRC32, true = MD5
};

// ============================================================================
// Хэширование блока
// ============================================================================
struct BlockHasher 
{
    explicit BlockHasher(bool use_md5) : m_use_md5(use_md5) {}

    std::string hash_block(const std::vector<char>& block) {
        if (m_use_md5) 
        {
            boost::crc_32_type crc;
            crc.process_bytes(block.data(), block.size());
            return std::to_string(crc.checksum());
        } 
        else 
        {
            boost::crc_32_type crc;
            crc.process_bytes(block.data(), block.size());
            return std::to_string(crc.checksum());
        }
    }

private:
    bool m_use_md5;
};

// ============================================================================
// Представление файла как последовательности хэшей блоков
// ============================================================================
struct FileBlockHashes 
{
    fs::path path;
    std::uintmax_t size;
    std::vector<std::string> block_hashes;
    bool fully_computed = false;
};

// ============================================================================
// Чтение одного блока из файла
// ============================================================================
std::vector<char> read_block(std::ifstream& file, std::size_t block_size) 
{
    std::vector<char> buffer(block_size);
    file.read(buffer.data(), block_size);
    std::streamsize bytes_read = file.gcount();

    if (bytes_read == 0) 
    {
        return {};
    }

    if (static_cast<std::size_t>(bytes_read) < block_size) 
    {
        buffer.resize(bytes_read);
        buffer.resize(block_size, '\0');
    }

    return buffer;
}

// ============================================================================
// Вычисление хэша следующего блока для группы файлов
// Возвращает true, если есть файлы, у которых ещё не все блоки обработаны
// ============================================================================
bool compute_next_block_hashes(
    std::vector<FileBlockHashes>& files,
    std::size_t block_index,
    std::size_t block_size,
    BlockHasher& hasher)
{
    bool any_incomplete = false;

    for (auto& file : files) 
    {
        if (file.fully_computed) 
        {
            continue;
        }

        std::uintmax_t offset = block_index * static_cast<std::uintmax_t>(block_size);

        if (offset >= file.size) 
        {
            file.fully_computed = true;
            continue;
        }

        std::ifstream ifs(file.path.string(), std::ios::binary);
        if (!ifs) 
        {
            std::cerr << "Не удалось открыть файл: " << file.path << "\n";
            file.fully_computed = true;
            continue;
        }

        ifs.seekg(offset, std::ios::beg);
        auto block = read_block(ifs, block_size);

        if (block.empty()) 
        {
            file.fully_computed = true;
            continue;
        }

        std::string hash = hasher.hash_block(block);
        if (block_index >= file.block_hashes.size()) 
        {
            file.block_hashes.push_back(hash);
        } 
        else 
        {
            file.block_hashes[block_index] = hash;
        }

        any_incomplete = true;
    }

    return any_incomplete;
}

// ============================================================================
// Разбиение файлов на подгруппы по хэшу текущего блока
// ============================================================================
std::vector<std::vector<FileBlockHashes>> group_by_block_hash(
    std::vector<FileBlockHashes>& files,
    std::size_t block_index)
{
    std::map<std::string, std::vector<FileBlockHashes>> groups;

    for (auto& file : files) 
    {
        if (block_index >= file.block_hashes.size()) 
        {
            groups["__incomplete__"].push_back(std::move(file));
        } 
        else 
        {
            groups[file.block_hashes[block_index]].push_back(std::move(file));
        }
    }

    std::vector<std::vector<FileBlockHashes>> result;

    for (auto& kv : groups) 
    {
        if (kv.second.size() > 1 || kv.first == "__incomplete__") 
        {
            result.push_back(std::move(kv.second));
        }
    }

    return result;
}

// ============================================================================
// Поиск дубликатов в группе файлов с одинаковым размером
// ============================================================================
std::vector<std::vector<fs::path>> find_duplicates_in_group(
    std::vector<FileBlockHashes>& files,
    std::size_t block_size,
    BlockHasher& hasher)
{
    std::vector<std::vector<fs::path>> duplicate_groups;

    if (files.empty()) 
    {
        return duplicate_groups;
    }

    std::size_t block_index = 0;

    while (true) 
    {
        // Вычисляем хэши текущего блока для всех файлов, у которых он ещё не вычислен
        compute_next_block_hashes(files, block_index, block_size, hasher);

        // Разбиваем на подгруппы по хэшу текущего блока
        auto groups = group_by_block_hash(files, block_index);

        // Оставляем только группы, где больше одного файла
        std::vector<std::vector<FileBlockHashes>> next_round_files;

        for (auto& group : groups) 
        {
            // Проверяем, все ли файлы в группе полностью обработаны
            bool all_complete = true;
            for (const auto& f : group) 
            {
                if (!f.fully_computed) 
                {
                    all_complete = false;
                    break;
                }
            }

            if (all_complete) 
            {
                // Все файлы полностью совпали по всем блокам
                if (group.size() > 1) 
                {
                    std::vector<fs::path> dup_group;
                    for (const auto& f : group) 
                    {
                        dup_group.push_back(f.path);
                    }
                    duplicate_groups.push_back(std::move(dup_group));
                }
            } 
            else 
            {
                // Нужно продолжать сравнение следующих блоков
                next_round_files.push_back(std::move(group));
            }
        }

        if (next_round_files.empty()) 
        {
            break;
        }

        files.clear();
        for (auto& g : next_round_files) 
        {
            files.insert(files.end(),
                         std::make_move_iterator(g.begin()),
                         std::make_move_iterator(g.end()));
        }

        ++block_index;
    }

    return duplicate_groups;
}

// ============================================================================
// Фильтрация файлов по маскам имени
// ============================================================================
bool matches_masks(const fs::path& path,
                   const std::vector<std::string>& masks) 
{
    if (masks.empty()) 
    {
        return true;
    }

    std::string filename = path.filename().string();
    std::string filename_lower = filename;
    boost::algorithm::to_lower(filename_lower);

    for (const auto& mask : masks) 
    {
        std::string mask_lower = mask;
        boost::algorithm::to_lower(mask_lower);

        // Простая поддержка * и ?
        std::string pattern = mask_lower;
        std::string regex_pattern;

        for (char c : pattern) 
        {
            if (c == '*') 
            {
                regex_pattern += ".*";
            } 
            else if (c == '?') 
            {
                regex_pattern += ".";
            } 
            else 
            {
                regex_pattern += c;
            }
        }

        regex_pattern = "^" + regex_pattern + "$";

        boost::regex re(regex_pattern);
        if (boost::regex_match(filename_lower, re)) 
        {
            return true;
        }
    }

    return false;
}

// ============================================================================
// Сканирование директорий и сбор файлов
// ============================================================================
std::vector<FileBlockHashes> scan_files(const Config& config) 
{
    std::vector<FileBlockHashes> files;

    std::set<fs::path> exclude_dirs_set;
    for (const auto& p : config.exclude_dirs) 
    {
        exclude_dirs_set.insert(fs::canonical(p));
    }

    for (const auto& scan_dir : config.scan_dirs) 
    {
        if (!fs::exists(scan_dir)) 
        {
            std::cerr << "Директория не существует: " << scan_dir << "\n";
            continue;
        }

        fs::canonical(scan_dir);

        int max_depth = config.depth_level;

        for (fs::recursive_directory_iterator it(scan_dir), end; it != end; ++it) 
        {
            if (max_depth >= 0 && it.depth() > static_cast<unsigned int>(max_depth)) 
            {
                it.pop();
                continue;
            }

            if (fs::is_directory(it->status())) 
            {
                if (exclude_dirs_set.count(fs::canonical(it->path()))) 
                {
                    it.pop();
                    continue;
                }
            }

            if (!fs::is_regular_file(it->status())) 
            {
                continue;
            }

            fs::path file_path = it->path();

            if (!matches_masks(file_path, config.filename_masks)) 
            {
                continue;
            }

            std::uintmax_t file_size = fs::file_size(file_path);

            if (file_size < config.min_file_size) 
            {
                continue;
            }

            FileBlockHashes fbh;
            fbh.path = file_path;
            fbh.size = file_size;
            fbh.fully_computed = false;

            files.push_back(std::move(fbh));
        }
    }

    return files;
}

// ============================================================================
// Разбор аргументов командной строки
// ============================================================================
Config parse_args(int argc, char* argv[]) 
{
    Config config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--scan-dir" && i + 1 < argc) 
        {
            config.scan_dirs.emplace_back(argv[++i]);
        } 
        else if (arg == "--exclude-dir" && i + 1 < argc) 
        {
            config.exclude_dirs.emplace_back(argv[++i]);
        } 
        else if (arg == "--depth" && i + 1 < argc) 
        {
            config.depth_level = std::stoi(argv[++i]);
        } 
        else if (arg == "--min-size" && i + 1 < argc) 
        {
            config.min_file_size = std::stoull(argv[++i]);
        } 
        else if (arg == "--mask" && i + 1 < argc) 
        {
            config.filename_masks.push_back(argv[++i]);
        } 
        else if (arg == "--block-size" && i + 1 < argc) 
        {
            config.block_size = std::stoull(argv[++i]);
        } 
        else if (arg == "--hash" && i + 1 < argc) 
        {
            std::string hash_type = argv[++i];
            boost::algorithm::to_lower(hash_type);
            config.use_md5 = (hash_type == "md5");
        }
    }

    return config;
}

// ============================================================================
// Основная функция
// ============================================================================
int main(int argc, char* argv[]) 
{
    Config config = parse_args(argc, argv);

    if (config.scan_dirs.empty()) 
    {
        std::cerr << "Не указаны директории для сканирования.\n";
        return 1;
    }

    auto files = scan_files(config);

    if (files.empty()) 
    {
        std::cout << "Файлы не найдены.\n";
        return 0;
    }

    std::map<std::uintmax_t, std::vector<FileBlockHashes>> groups_by_size;
    for (auto& f : files) 
    {
        groups_by_size[f.size].push_back(std::move(f));
    }

    BlockHasher hasher(config.use_md5);

    bool first_group = true;

    for (auto& kv : groups_by_size) 
    {
        auto& group_files = kv.second;

        if (group_files.size() < 2) 
        {
            continue;
        }

        auto duplicate_groups =
            find_duplicates_in_group(group_files, config.block_size, hasher);

        for (const auto& dup_group : duplicate_groups) 
        {
            if (!first_group) 
            {
                std::cout << "\n";
            }
            first_group = false;

            for (const auto& path : dup_group) 
            {
                std::cout << path.string() << "\n";
            }
        }
    }

    return 0;
}