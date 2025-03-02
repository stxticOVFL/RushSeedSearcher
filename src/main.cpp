#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if USE_TRACY
#if defined(__clang__) || defined(__GNUC__)
#define TracyFunction __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define TracyFunction __FUNCSIG__
#endif

#include <tracy/Tracy.hpp>
#endif


#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef min // sigh
#undef max
#endif


class random_dotnet {
  public:
    random_dotnet() { seed_array = new int[56]; };
    random_dotnet(int seed) {
        seed_array = new int[56];
        set_seed(seed);
    }

    ~random_dotnet() { delete[] seed_array; }

    inline void set_seed(int seed) {
        constexpr auto lim = std::numeric_limits<int>();
        seed_array[55] = seed = MSEED - seed;
        int num3 = 1;

        int index = 0;
        for (int i = 0; i < 54; ++i) {
            index += 21;
            if (index >= 55)
                index -= 55;
            seed_array[index] = num3;
            num3 = seed - num3;
            if (num3 < 0)
                num3 += lim.max();
            seed = seed_array[index];
        }

        for (int j = 0; j < 4; ++j) {
            int offset = 31;
            for (int k = 0; k < 55; ++k) {
                if (++offset == 56)
                    offset = 1;

                if ((seed_array[k + 1] -= seed_array[offset]) < 0)
                    seed_array[k + 1] += lim.max();
            }
        }
        inext = 0;
        inextp = 21;
    }

    inline double next_double() { return (double)next() / MBIG; }

    inline int next() {
        ++inext;
        if (++inextp == 56)
            inextp = 1;
        else if (inext == 56)
            inext = 1;
        int num3 = seed_array[inext] - seed_array[inextp];
        if (num3 == MBIG)
            num3--;
        else if (num3 < 0)
            num3 += std::numeric_limits<int>().max();
        return seed_array[inext] = num3;
    }

    inline int next(int min, int max) { return (int)(next_double() * (max - min)) + min; }
    inline int next(int max) { return (int)(next_double() * max); }

  private:
    static constexpr int MBIG = 2147483647;
    static constexpr int MSEED = 161803398;
    static constexpr int MZ = 0;

    int* seed_array;
    int inext;
    int inextp;
};

struct rush_info {
    rush_info() = default;
    rush_info(int c, int o) : count(c), offset(o) {};

    int count;
    int offset;
};

// clang-format off
static std::map<std::string, rush_info> rushes = {
    { "white",  rush_info(96, 0) },
    { "mikey",  rush_info(96, 0) },
    { "yellow", rush_info(8, 96) },
    { "violet", rush_info(8, 96 + 8) },
    { "red",    rush_info(8, 96 + 8 + 8) },

    { "rebirth",                    rush_info(10, 0) },
    { "killer-inside",              rush_info(10, 10) },
    { "only-shallow",               rush_info(10, 20) },
    { "the-old-city",               rush_info(3, 30) },
    { "the-burn-that-cures",        rush_info(10, 33) },
    { "covenant",                  rush_info(10, 43) },
    { "reckoning",                 rush_info(10, 53) },
    { "benediction",               rush_info(10, 63) },
    { "apocrypha",                 rush_info(10, 73) },
    { "the-third-temple",          rush_info(3, 83) },
    { "thousand-pound-butterfly",  rush_info(10, 85) },
    { "hand-of-god",               rush_info(2, 96 + 8 + 8 + 8) }
};
// clang-format on


bool found = false;
int jobs;
int size;
int* targetD;
bool ordered;
bool stop_at_first;
#if NDEBUG
constexpr bool verbose = false;
#else
bool verbose;
#endif
std::mutex cout_mutex;
std::chrono::time_point<std::chrono::steady_clock> start;
int level_count;

void get_levels(int* output, int* pool, random_dotnet& random) {
#if USE_TRACY
    ZoneScopedN("Get Levels");
#endif
    int target_count = size / sizeof(int);

    int last = 0;
    memset(output, 0xFF, sizeof(int) * level_count);
    for (int i = 0; i < level_count; ++i) {
        if (output[i] == 0xFFFFFFFF)
            output[i] = i;

        int r = random.next(level_count);
        pool[i] = r;
        if (r < target_count) {
            for (int j = last; j < i + 1; ++j) {
                int rand = pool[j];
                int num = output[j];
                int randv = output[rand];
                output[j] = randv == 0xFFFFFFFF ? rand : randv;
                output[rand] = num;
            }
            last = i + 1;
        }
    }

    return;
}

void level_thread(int offset) {
    int stoff = offset + 1;

#if USE_TRACY
    tracy::SetThreadName(std::format("Level Thread {}", stoff).c_str());
#endif

#if _WIN32
    SetThreadPriority(GetCurrentThread(), 1);
    // SetThreadAffinityMask(GetCurrentThread(), 1 << (stoff + 1));
#endif

    int* levels = new int[level_count];
    int* randpool = new int[level_count];
    int* targetM = new int[size / sizeof(int)];
    memcpy(targetM, targetD, size); // own memory
    random_dotnet rand(offset);

    if (verbose) {
        std::lock_guard<std::mutex> lockC(cout_mutex);
        std::cout << "Thread " << stoff << " started." << std::endl;
    }

    while (true) {
#if USE_TRACY
        ZoneScopedN("Get & Compare");
#endif

        get_levels(levels, randpool, rand);
        if (ordered) {
            if (!memcmp(levels, targetM, size)) {
                std::lock_guard<std::mutex> lockC(cout_mutex);

                if (found) // early check
                    break;
                std::cout << "** Found: " << offset << " ("
                          << (std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::steady_clock::now() - start)
                                     .count())
                        / 1000000.0
                          << "s, thread " << stoff << ") **" << std::endl;

                if (stop_at_first) {
                    found = true;
                    break;
                }
            }
        }
        else {
            // var startSet = new HashSet<int>(randomizedIndex.Take(searchDepth));
            // if (startSet.IsSupersetOf(targetSet)) {
            //     break;
            // }
        }

        offset += jobs;

        if (offset < 0 || found)
            break;
        rand.set_seed(offset);

        if (offset % 20000000 == 0) {
            std::lock_guard<std::mutex> lockC(cout_mutex);
            std::cout << "Current seed: " << offset << " (thread " << stoff << ")" << std::endl;
        }
    }

    if (verbose) {
        std::lock_guard<std::mutex> lockC(cout_mutex);
        std::cout << "Thread " << stoff << " finished." << std::endl;
    }

    delete[] levels;
    delete[] targetM;
}

int display_help() { return 1; }

inline int stoi(const std::string& str) { return std::stoi(str); }
inline bool stob(const std::string& str) {
    bool res;
    std::istringstream(str) >> res;
    return res;
}

void signal_handler(int signal) {
    found = true; // force stop it
    std::lock_guard<std::mutex> lockC(cout_mutex);
    std::cout << "\nForce stopping." << std::endl;
}

// https://stackoverflow.com/a/868894
class input_parser {
  public:
    input_parser(int& argc, char** argv) {
        for (int i = 1; i < argc; ++i)
            tokens.push_back(std::string(argv[i]));
    }

    template <typename T>
    bool get_option(const std::string& option, T* ref, T (*pass)(const std::string&)) const {
        std::vector<std::string>::const_iterator itr;
        itr = std::find(tokens.begin(), tokens.end(), option);

        std::string val("");
        if (itr != tokens.end() && ++itr != tokens.end())
            val = *itr;
        else
            return false;

        try {
            *ref = pass(val);
        } catch (...) {
            return false;
        }
        return true;
    }

    std::string get_option(const std::string& option) const {
        std::vector<std::string>::const_iterator itr;
        itr = std::find(tokens.begin(), tokens.end(), option);

        std::string val("");
        if (itr != tokens.end() && ++itr != tokens.end())
            val = *itr;

        return val;
    }
    int option_index(const std::string& option) const {
        auto found = std::find(this->tokens.begin(), this->tokens.end(), option);
        if (found == tokens.end())
            return -1;
        return std::distance(tokens.begin(), found);
    }

    std::vector<std::string> tokens;
};

int main(int argc, char** argv) {
    input_parser input(argc, argv);
#if !NDEBUG
    verbose = input.option_index("-v") != -1;
#endif
    if (input.option_index("-h") != -1) {
        display_help();
        return 0;
    }

    auto rush = input.tokens[0];
    std::for_each(rush.begin(), rush.end(), tolower);
    if (!rushes.contains(rush))
        return display_help();
    auto rushinfo = rushes[rush];
    level_count = rushinfo.count;

    int seed;
    if (input.get_option("-g", &seed, stoi)) {
        // do the whole export shenaigan
        std::cout << "-g is not currently supported." << std::endl;
        return 2;
    }
    else if (input.option_index("-g") != -1)
        return display_help();

    ordered = input.option_index("-u") == -1;
    stop_at_first = input.option_index("-f") == -1
        ? (input.option_index("-F") != -1 ? false : ordered)
        : true;

    std::string list = input.tokens[1];
    // views fuckin HATE me rn so fuck it im just gonna forloop this shit it's fine
    size_t pos = 0;
    std::vector<int> target;
    try {
        while ((pos = list.find(',')) != std::string::npos) {
            target.push_back(std::stoi(list.substr(0, pos)) - 1);
            list.erase(0, pos + 1);
        }
        if (!list.empty())
            target.push_back(std::stoi(list) - 1);
    } catch (...) {
        return display_help();
    }

    size = target.size() * sizeof(int);
    targetD = target.data();

    jobs = 4;
    if (input.option_index("-j") != -1 && !input.get_option("-j", &jobs, stoi))
        return display_help();

    std::signal(SIGINT, signal_handler);

    std::vector<std::thread> jobThreads;
    jobThreads.reserve(jobs);

#if _WIN32
    SetPriorityClass(GetCurrentProcess(),
        input.option_index("--realtime") != -1 ? REALTIME_PRIORITY_CLASS : HIGH_PRIORITY_CLASS);
#endif
    start = std::chrono::steady_clock::now();
    for (int i = 0; i < jobs; ++i)
        jobThreads.emplace_back(level_thread, i);

    for (auto& t : jobThreads)
        t.join();

    std::cout << "Done in "
              << (std::chrono::duration_cast<std::chrono::microseconds>(
                     std::chrono::steady_clock::now() - start)
                         .count())
            / 1000000.0
              << "s." << std::endl;
    return 0;
}