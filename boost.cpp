#include <boost/filesystem.hpp>
#include <boost/crc.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <boost/uuid/detail/md5.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/program_options.hpp>

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
namespace po = boost::program_options;

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

    std::string hashBlock(const std::vector<char>& block) {
        if (m_use_md5) 
        {
            boost::uuids::detail::md5 md5;
            md5.process_bytes(block.data(), block.size());
            boost::uuids::detail::md5::digest_type digest;
            md5.get_digest(digest);
            return boost::algorithm::hex(std::string(reinterpret_cast<const char*>(&digest), sizeof(digest)));
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
    std::ifstream ifs; 
    std::uintmax_t size;
    std::vector<std::string> block_hashes;
    bool fully_computed = false;
};

// ============================================================================
// Чтение одного блока из файла
// ============================================================================
std::vector<char> readBlock(std::ifstream& file, std::size_t block_size) 
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
bool computeNextBlockHashes(
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
        if(!file.ifs.is_open())
        {
            file.ifs.open(file.path.string(), std::ios::binary);
        }

        if (!file.ifs) 
        {
            std::cerr << "Не удалось открыть файл: " << file.path << "\n";
            file.fully_computed = true;
            continue;
        }

        file.ifs.seekg(offset, std::ios::beg);
        auto block = readBlock(file.ifs, block_size);

        if (block.empty()) 
        {
            file.fully_computed = true;
            continue;
        }

        std::string hash = hasher.hashBlock(block);
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
std::vector<std::vector<FileBlockHashes>> groupByBlockHash(
    std::vector<FileBlockHashes>& files,
    std::size_t block_index)
{
    std::map<std::string, std::vector<FileBlockHashes>> groups;
    std::vector<FileBlockHashes> pending_files;

    for (auto& file : files) 
    {
        if (file.fully_computed) 
        {
            // Файл завершён, группируем по последнему хэшу
            if (!file.block_hashes.empty()) 
            {
                groups[file.block_hashes.back()].push_back(std::move(file));
            }
            else 
            {
                pending_files.push_back(std::move(file));
            }
        }
        else if (block_index < file.block_hashes.size()) 
        {
            // Есть хэш для текущего блока
            groups[file.block_hashes[block_index]].push_back(std::move(file));
        }
        else 
        {
            // Файл ещё не завершён и нет хэша для текущего блока
            pending_files.push_back(std::move(file));
        }
    }

    std::vector<std::vector<FileBlockHashes>> result;

    // Добавляем группы с совпадающими хэшами
    for (auto& kv : groups) 
    {
        if (kv.second.size() > 1) 
        {
            result.push_back(std::move(kv.second));
        }
    }

    // Добавляем незавершённые файлы как отдельную группу
    if (pending_files.size() > 1) 
    {
        result.push_back(std::move(pending_files));
    }

    return result;
}

// ============================================================================
// Поиск дубликатов в группе файлов с одинаковым размером
// ============================================================================
std::vector<std::vector<fs::path>> findDuplicatesInGroup(
    std::vector<FileBlockHashes>& files,
    std::size_t block_size,
    BlockHasher& hasher)
{
    std::vector<std::vector<fs::path>> duplicate_groups;

    if (files.empty()) 
    {
        return duplicate_groups;
    }

    // Каждая группа — файлы, совпавшие по всем предыдущим блокам
    std::vector<std::vector<FileBlockHashes>> current_groups;
    current_groups.push_back(std::move(files));

    std::size_t block_index = 0;

    while (!current_groups.empty()) 
    {
        std::vector<std::vector<FileBlockHashes>> next_groups;

        // Обрабатываем каждую группу отдельно
        for (auto& group : current_groups) 
        {
            if (group.empty()) 
            {
                continue;
            }

            // Проверяем, все ли файлы в группе завершены
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
                // Все файлы завершены — это группа дубликатов
                if (group.size() > 1) 
                {
                    std::vector<fs::path> dup_group;
                    for (const auto& f : group) 
                    {
                        dup_group.push_back(f.path);
                    }
                    duplicate_groups.push_back(std::move(dup_group));
                }
                // Иначе группа из 1 файла — не дубликат
                continue;
            }

            // Вычисляем хэши текущего блока для этой группы
            computeNextBlockHashes(group, block_index, block_size, hasher);

            // Разбиваем группу на подгруппы по хэшу текущего блока
            auto subgroups = groupByBlockHash(group, block_index);

            // Добавляем подгруппы в следующий раунд
            for (auto& subgroup : subgroups) 
            {
                if (subgroup.size() > 1) 
                {
                    next_groups.push_back(std::move(subgroup));
                }
            }
        }

        // Переходим к следующей группе
        current_groups = std::move(next_groups);
        ++block_index;
    }

    return duplicate_groups;
}

// ============================================================================
// Фильтрация файлов по маскам имени
// ============================================================================
bool matchesMasks(const fs::path& path,
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
std::vector<FileBlockHashes> scanFiles(const Config& config) 
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
            if (max_depth >= 0 && it.depth() > max_depth) 
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

            if (!matchesMasks(file_path, config.filename_masks)) 
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
Config parseArgs(int argc, char* argv[]) 
{
    Config config;
    
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Produce help message")
        ("scan-dir", po::value<std::vector<std::string>>(), 
         "Directory to scan (can be specified multiple times)")
        ("exclude-dir", po::value<std::vector<std::string>>(), 
         "Directory to exclude (can be specified multiple times)")
        ("depth", po::value<int>()->default_value(-1), 
         "Scan depth level (-1 = unlimited, 0 = only specified directory)")
        ("min-size", po::value<std::uintmax_t>()->default_value(1), 
         "Minimum file size")
        ("mask", po::value<std::vector<std::string>>(), 
         "Filename mask (can be specified multiple times)")
        ("block-size", po::value<std::size_t>()->default_value(4096), 
         "Block size for reading files")
        ("hash", po::value<std::string>()->default_value("crc32"), 
         "Hash algorithm (crc32 or md5)");
    
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    
    // Обработка help
    if (vm.count("help")) 
    {
        std::cout << desc << "\n";
        std::exit(0);
    }
    
    // Заполнение config из variables_map
    if (vm.count("scan-dir")) 
    {
        const auto& dirs = vm["scan-dir"].as<std::vector<std::string>>();
        for (const auto& dir : dirs) 
        {
            config.scan_dirs.emplace_back(dir);
        }
    }
    
    if (vm.count("exclude-dir")) 
    {
        const auto& dirs = vm["exclude-dir"].as<std::vector<std::string>>();
        for (const auto& dir : dirs) 
        {
            config.exclude_dirs.emplace_back(dir);
        }
    }
    
    if (vm.count("depth")) 
    {
        config.depth_level = vm["depth"].as<int>();
    }
    
    if (vm.count("min-size")) 
    {
        config.min_file_size = vm["min-size"].as<std::uintmax_t>();
    }
    
    if (vm.count("mask")) 
    {
        config.filename_masks = vm["mask"].as<std::vector<std::string>>();
    }
    
    if (vm.count("block-size")) 
    {
        config.block_size = vm["block-size"].as<std::size_t>();
    }
    
    if (vm.count("hash")) 
    {
        std::string hash_type = vm["hash"].as<std::string>();
        boost::algorithm::to_lower(hash_type);
        config.use_md5 = (hash_type == "md5");
    }
    
    return config;
}

// ============================================================================
// Основная функция
// ============================================================================
int main(int argc, char* argv[]) 
{
    Config config = parseArgs(argc, argv);

    if (config.scan_dirs.empty()) 
    {
        std::cerr << "Не указаны директории для сканирования.\n";
        return 1;
    }

    auto files = scanFiles(config);

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
            findDuplicatesInGroup(group_files, config.block_size, hasher);

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