#include <httplib.h>

#include <cstdio>
#include <cxxopts.hpp>
#include <filesystem>
#include <fstream>
#if STORE_LEADERBOARD
#include <nlohmann/json.hpp>
#endif
#include <regex>
#include <sstream>

std::string read_file(std::filesystem::path path, bool strip=true) {
  std::ifstream t(path.string());
  std::stringstream buffer;
  buffer << t.rdbuf();
  std::string str = buffer.str();
  if (strip) {
    int begin = 0;
    while (begin < str.size() && str[begin] == '\n') { ++begin; }
    int end = str.size();
    while (end > 0 && str[end - 1] == '\n') { --end; }
    return str.substr(begin, end - begin);
  } else {
    return str;
  }
}

inline bool contains(const std::string &code, std::string substr) {
  return code.find(substr) != std::string::npos;
}
inline bool contains_regex(const std::string &code, std::string regex) {
  std::regex r(regex);
  return std::regex_search(code, r);
}

static std::vector<std::string> task_specific_bad_code_regex;

bool validate_code_input(const std::string &code) {
  // clang-format off
  static std::vector<std::string> bad_code_regex = {
      // spawn process:
      "system", "execl", "execlp", "execle", "execv", "execvp", "execvpe",
      "fork",
      // inline assembly:
      "\\basm",
      // overriding main:
      "\\bmain\\b", "argv", "argc", "\\b_main\\b", "\\bstart\\b",
      // abusive memory:
      "calloc", "malloc", "free", "\\bnew\\b", "\\bmmap\\b",
      // multithrading:
      "pthread", "async", "launch", "thread",
      // File IO
      "fstream", "fopen", "fputc", "filesystem", "directory_iterator", "dirent", "opendir", "readdir", "fread", "fwrite",
      // Stdin/stdout
      "printf", "puts", "fputs", "putc", "\\bcout\\b", "\\bcerr\\b", "\\bcin\\b",
  };
  static std::vector<std::string> bad_code_plain = {
      // Digraphs and preprocessor
      "<%", "%>", "<:", ":>", "%:", "%:%:", "#",
  };
  // clang-format on
  for (const std::string &regex : bad_code_regex) {
    if (contains_regex(code, regex)) {
      return false;
    }
  }
  for (const std::string &plain : bad_code_plain) {
    if (contains(code, plain)) {
      return false;
    }
  }
  return true;
}

bool validate_flags(const std::string &flags) {
  // clang-format off
  static std::vector<std::string> bad_flags = {
    ";", "&&", "||", "|", "&", ".", "/", "<", ">"
  };
  // clang-format on
  for (const std::string &str : bad_flags) {
    if (contains(flags, str)) {
      return false;
    }
  }
  return true;
}

struct submission_result {
  bool found{false};

  std::string task;
  std::string submission_id;
  std::string user_id;
  std::string code;
  std::string flags;
  std::string disassembly;
  std::string disassembly_with_source;
  std::string benchmark_output;

  bool compile_successful{false};
  bool correctness_test_passed{false};
  std::string author;

  int status{0};
  std::string compiler_output;
  double best_time{std::numeric_limits<double>::infinity()};
  double cycles_per_call{std::numeric_limits<double>::infinity()};
};

struct leaderboard_entry {
  std::string task;
  std::string user_id;
  std::string submission_id;
  double best_time;
  double cycles_per_call;
  std::string author;
};
#if STORE_LEADERBOARD
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(leaderboard_entry, task, user_id,
                                   submission_id, best_time, cycles_per_call);
#endif

leaderboard_entry make_leaderboard_entry(const submission_result &r) {
  leaderboard_entry e;
  e.task = r.task;
  e.best_time = r.best_time;
  e.user_id = r.user_id;
  e.submission_id = r.submission_id;
  e.cycles_per_call = r.cycles_per_call;
  e.author = r.author;
  return e;
}

int run_validated_submission(const std::string &task,
                             const std::string &user_id,
                             const std::string &submission_id,
                             const std::string &code, const std::string &flags,
                             const std::string &symbol,
                             const std::string &author, const std::string &ip) {
  std::printf("Running submission.\n");
  std::filesystem::path submission_dir = "submissions";
  submission_dir /= task;
  submission_dir /= submission_id;
  std::printf("   + mkdir: %s\n", submission_dir.c_str());
  std::filesystem::create_directories(submission_dir);

  {
    std::filesystem::path submission_header =
        submission_dir / "submitted_code.hpp";
    std::printf("   + write code: %s\n", submission_header.c_str());
    std::ofstream file(submission_header.string());
    file << code;
    file.close();
  }

  {
    std::filesystem::path flags_path = submission_dir / "flags.txt";
    std::printf("   + write flags: %s\n", flags_path.c_str());
    std::ofstream flags_file(flags_path.string());
    flags_file << flags;
    flags_file.close();
  }

  {
    std::filesystem::path userid_path = submission_dir / "user_id";
    std::printf("   + write user_id: %s\n", userid_path.c_str());
    std::ofstream userid_file(userid_path.string());
    userid_file << user_id;
    userid_file.close();
  }
  {
    std::filesystem::path author_path = submission_dir / "author";
    std::printf("   + write author: %s\n", author_path.c_str());
    std::ofstream author_file(author_path.string());
    author_file << author;
    author_file.close();
  }
  {
    std::filesystem::path ip_path = submission_dir / "ip";
    std::printf("   + write ip: %s\n", ip_path.c_str());
    std::ofstream ip_file(ip_path.string());
    ip_file << ip;
    ip_file.close();
  }

  std::printf("   + copy benchmark.cpp\n");
  std::filesystem::path benchmark_file = "tasks";
  benchmark_file /= task;
  benchmark_file /= "benchmark.cpp";
  std::filesystem::copy_file(benchmark_file, submission_dir / "benchmark.cpp",
                             std::filesystem::copy_options::overwrite_existing);

  std::string command = "/bin/bash ";
  command += std::filesystem::absolute("runtime/compile.sh").string();
  command += " ";
  command += submission_dir.string();
  command += " ";
  command += symbol; // Symbol to get the disassembly of.
  std::printf("Executing command:\n%s\n", command.c_str());
  int exit_code = std::system(command.c_str());
  int status = WEXITSTATUS(exit_code);
  std::printf("code: %d\n", status);

  return status;
}

submission_result load_submission_result(const std::string &task,
                                         const std::string &submission_id) {
  submission_result result;
  std::filesystem::path submission_dir = "submissions";
  submission_dir /= task;
  submission_dir /= submission_id;
  if (!std::filesystem::exists(submission_dir)) {
    result.found = false;
    return result;
  }

  result.found = true;

  result.code = read_file(submission_dir / "submitted_code.highlight.html");
  if (result.code.empty()) {
    result.code = read_file(submission_dir / "submitted_code.hpp");
  }
  result.flags = read_file(submission_dir / "flags.txt");
  result.user_id = read_file(submission_dir / "user_id");
  result.author = read_file(submission_dir / "author");
  result.submission_id = submission_id;
  result.task = task;
  result.compiler_output =
      read_file(submission_dir / "compile_stderr.log.html");
  result.status = std::atoi(read_file(submission_dir / "exit_code").c_str());
  result.benchmark_output = read_file(submission_dir / "benchmark_output");

  if (result.status != 1) {  // not failed
    result.compile_successful = true;
    result.disassembly = read_file(submission_dir / "disassembly.html");
    result.disassembly_with_source =
        read_file(submission_dir / "disassembly_with_source.html");

    if (result.status == 0) {
      result.correctness_test_passed = true;
      std::string content = read_file(submission_dir / "best_time.txt");
      std::stringstream ss(content);
      ss >> result.best_time;
      ss >> result.cycles_per_call;
    } else if (result.status == 2) {
      result.correctness_test_passed = false;
    }
  }

  return result;
}

std::string replace_all(std::string str, const std::string &from,
                        const std::string &to) {
  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos +=
        to.length();  // Handles case where 'to' is a substring of 'from'
  }
  return str;
}

std::string pre(const std::string &str) { return "<pre>" + str + "</pre>"; }
std::string green(const std::string &str) {
  return "<span style='color:green;'>" + str + "</span>";
}
std::string red(const std::string &str) {
  return "<span style='color:red;'>" + str + "</span>";
}

std::string format_time(float time) {
  char buf[100];
  // sprintf(buf, "%.3f \u00b5s", time * 1e6f);
  sprintf(buf, "%.5f ms", time * 1e3f);
  // {
  //   float ms = std::floor(time * 1e6f) * 1e-3;
  //   float us = time * 1e6f - ms * 1e3f;
  //   sprintf(buf, "%.3f ms + %.03f \u00b5s", ms, us);
  // }
  return std::string(buf);
}

std::string format_cycles_per_call(float cycles_per_call) {
  char buf[100];
  sprintf(buf, "%.3f cycles/call", cycles_per_call);
  return std::string(buf);
}

std::string format_author(const std::string &auth, bool text, bool icon) {
  std::string r;
  if (icon) {
    if (auth == "Human") {
      r += "👩";
    }
    if (auth == "ChatGPT") {
      r += "🤖";
    }
    if (auth == "HumanTeam") {
      r += "👩👩";
    }
    if (auth == "HybridTeam") {
      r += "🤖👩";
    }
    if (auth == "Teacher") {
      r += "🧑‍🏫";
    }
  }
  if (text) {
    if (icon) {
      r += " ";
    }
    r += auth;
  }
  return r;
}

std::string anonimify(std::string input, const std::string &task) {
  input += "__" + task + "__saltyAZErap";
  std::hash<std::string> hasher;
  size_t hash = hasher(input);
  char buf[100];
  sprintf(buf, "%8zX", hash);
  return std::string(buf);
}

std::string render_leaderboard(std::string task,
                               const std::vector<leaderboard_entry> &entries,
                               const std::string &user_id, bool public_mode) {
  std::string html = read_file("runtime/templates/leaderboard.html");
  html = replace_all(html, "${TASK}", task);
  std::string rows = "";
  std::set<std::string> users_on_leaderboard;
  int user_rank = -1;
  for (size_t i = 0; i < entries.size(); ++i) {
    const leaderboard_entry &e = entries[i];
    auto [it, done] = users_on_leaderboard.insert(e.user_id);
    std::string class_str = "";
    if (done) {
      class_str = "first-of-user";
      user_rank++;
    }
    if (user_id == e.user_id) {
      rows +=
          "<tr class='" + class_str + "' style='background-color: #caddb7;'>";
    } else {
      rows += "<tr class='" + class_str + "'>";
    }
    rows += "<td>" + std::to_string(i) + "</td>";
    if (done) {
      rows += "<td>" + std::to_string(user_rank) + "</td>";
    } else {
      rows += "<td></td>";
    }
    if (user_id == e.user_id || public_mode) {
      rows += "<td><a href='view_submission?id=" + e.submission_id + "'>" +
              e.submission_id + "</a></td>";
    } else {
      rows += "<td>" + e.submission_id + "</td>";
    }
    char buf[12];
    std::hash<std::string> hasher;
    size_t hash = hasher(e.user_id);
    std::sprintf(buf, "#%02zx%02zx%02zx", hash & 0x7f, (hash >> 8) & 0x7f,
                 (hash >> 16) & 0x7f);
    std::string color(buf);
    rows += "<td style='background-color: " + color + "; color: white;'>" +
            anonimify(e.user_id, task) + "</td>";
    rows += "<td>" + format_time(e.best_time) + "</td>";
    rows += "<td>" + format_cycles_per_call(e.cycles_per_call) + "</td>";
    rows += "<td>" + format_author(e.author, false, true) + "</td>";
    rows += "</tr>\n";
  }
  html = replace_all(html, "${LEADERBOARD_ROWS}", rows);
  return html;
}

std::string render_submission_result(const submission_result &result) {
  std::string html = read_file("runtime/templates/submission_result.html");
  // clang-format off
  html = replace_all(html, "${TASK}", result.task);
  html = replace_all(html, "${USER_ID}", anonimify(result.user_id, result.task));
  html = replace_all(html, "${SUBMISSION_ID}", result.submission_id);
  html = replace_all(html, "${COMPILER_FLAGS}", result.flags);
  html = replace_all(html, "${COMPILE_STATUS}", result.compile_successful ? green("Success") : red("Failed"));
  html = replace_all(html, "${CORRECTNESS_TEST}", result.correctness_test_passed ? green("Success") : red("Failed"));
  html = replace_all(html, "${BENCHMARK_BEST_TIME}", format_time(result.best_time));
  html = replace_all(html, "${BENCHMARK_CYCLES_PER_CALL}", format_cycles_per_call(result.cycles_per_call));
  html = replace_all(html, "${AI_GENERATED}", format_author(result.author, true, true));
  html = replace_all(html, "${INPUT_CODE}", result.code);
  html = replace_all(html, "${COMPILER_OUTPUT}", result.compiler_output);
  html = replace_all(html, "${DISASSEMBLY}", result.disassembly);
  html = replace_all(html, "${DISASSEMBLY_WITH_SOURCE}", result.disassembly_with_source);
  html = replace_all(html, "${BENCHMARK_OUTPUT}", result.benchmark_output);
  // clang-format on
  return html;
}

static int submission_id_counter = 0;
std::string generate_submission_id() {
  char buf[100];
  submission_id_counter++;
  int rand_val = std::rand() % 0xffff;
  std::sprintf(buf, "%04d-%04x", submission_id_counter, rand_val);
  return std::string(buf);
}

void sort_leaderboard(std::vector<leaderboard_entry> &leaderboard) {
  std::sort(leaderboard.begin(), leaderboard.end(),
            [](auto &a, auto &b) { return a.best_time < b.best_time; });
}

std::string generate_user_id() {
  char buf[100];
  submission_id_counter++;
  int32_t rand_val = std::rand();
  std::sprintf(buf, "%08x", rand_val);
  return std::string(buf);
}

std::string find_user_id_in_request(const httplib::Request &req) {
  static std::string header{"Cookie"};
  int count = req.get_header_value_count(header);
  for (int i = 0; i < count; ++i) {
    std::string cookie_str = req.get_header_value(header, i);
    int offset = 0;
    while (true) {
      int cookie_end = cookie_str.find("; ", offset);
      std::string cookie;
      if (cookie_end == std::string::npos) {
        cookie = cookie_str.substr(offset, cookie_end - offset);
      }
      int space_idx = cookie.find("=");
      if (space_idx != std::string::npos) {
        std::string cookie_name = cookie.substr(0, space_idx);
        std::string cookie_value = cookie.substr(space_idx + 1);
        if (cookie_name == "userId") {
          return cookie_value;
        }
      }

      if (cookie_end == std::string::npos) {
        break;
      } else {
        offset = cookie_end + 2;
      }
    }
  }
  return "";
}

int main(int argc, char **argv) {
  // clang-format off
  cxxopts::Options options("ClassroomPerf", "Classroom performance competition");
  options.add_options()
    ("task", "The task name for this competition.", cxxopts::value<std::string>())
    ("host", "Bind address for the server.", cxxopts::value<std::string>()->default_value("0.0.0.0"))
    ("port", "Bind port for the server.", cxxopts::value<int>()->default_value("5000"))
    ("P,public", "Run the server publicly.")
    ("R,regenerate-leaderboard", "Regenerate the leaderboard from the submission folder.")
    ;
  options.parse_positional({"task"});
  // clang-format on

  auto args = options.parse(argc, argv);
  if (args.count("help") || args.count("task") == 0) {
    std::cout << options.help() << std::endl;
    std::exit(0);
  }

  std::string task = args["task"].as<std::string>();
  bool public_mode = args["public"].count();

  std::filesystem::path task_folder("tasks/");
  task_folder /= task;
  if (!std::filesystem::is_directory(task_folder)) {
    std::printf("No task directory found: %s\n", task_folder.c_str());
    return 1;
  }

  std::filesystem::path bad_code_file = task_folder / "bad_code.regex";
  if (std::filesystem::exists(bad_code_file)) {
    task_specific_bad_code_regex.clear();
    std::ifstream f(bad_code_file.string());
    if (f.is_open()) {
      std::printf("Found bad code file with rules:\n");
      std::string line;
      while (std::getline(f, line)) {
        if (!line.empty()) {
          task_specific_bad_code_regex.push_back(line);
          std::printf("   '%s'\n", line.c_str());
        }
      }
    } else {
      std::printf("Could not load bad-code file for task %s: %s.\n", task.c_str(), bad_code_file.c_str());
    }
  } else {
    std::printf("No bad code file found for task %s.\n", task.c_str());
  }

  std::filesystem::path symbol_file = task_folder / "symbol";
  std::string symbol;
  if (std::filesystem::exists(symbol_file)) {
    symbol = read_file(symbol_file.string());
    std::printf("Symbol file indicates: %s.\n", symbol.c_str());
  } else {
    std::printf("No symbol file found for task %s.\n", task.c_str());
    return 1;
  }



  std::srand(std::time(0));
  std::vector<leaderboard_entry> leaderboard;

  // Create submissions dir
  std::filesystem::path submission_dir = "submissions";
  submission_dir /= task;
  std::filesystem::create_directories(submission_dir);

  // Load the leaderboard for this task
  std::filesystem::path leaderboard_dir = "leaderboard";
  leaderboard_dir /= task;
  std::filesystem::create_directories(leaderboard_dir);
  submission_id_counter = 0;
  if (args.count("regenerate-leaderboard") || !STORE_LEADERBOARD) {
    std::printf("Regenerating leaderboard...");

    for (auto it = std::filesystem::directory_iterator(
             submission_dir,
             std::filesystem::directory_options::skip_permission_denied);
         it != std::filesystem::directory_iterator(); ++it) {
      if (std::filesystem::is_directory(it->path())) {
        submission_id_counter++;
        submission_result result =
            load_submission_result(task, it->path().filename().string());
        if (result.compile_successful && result.correctness_test_passed) {
          leaderboard_entry e = make_leaderboard_entry(result);
          leaderboard.push_back(std::move(e));
        }
      }
    }
  } else {
#if STORE_LEADERBOARD
    for (auto it = std::filesystem::directory_iterator(
             leaderboard_dir,
             std::filesystem::directory_options::skip_permission_denied);
         it != std::filesystem::directory_iterator(); ++it) {
      if (it->path().extension().string() == ".json") {
        submission_id_counter++;
        std::printf("Loading leaderboard entry: %s\n", it->path().c_str());
        std::string lbes = read_file(it->path().string());
        nlohmann::json js = nlohmann::json::parse(lbes);
        leaderboard_entry entry;
        js.get_to(entry);

        leaderboard.push_back(std::move(entry));
      }
    }
#endif
  }
  std::printf("Loaded %zu leaderboard entries.\n", leaderboard.size());
  sort_leaderboard(leaderboard);

  httplib::Server svr;
  svr.set_mount_point("/", "./runtime/static/");
  svr.Get("/(leaderboard)?", [&](const httplib::Request &req,
                                 httplib::Response &res) {
    // std::string user_id = anonimify(req.remote_addr, task);
    std::string user_id = find_user_id_in_request(req);
    if (user_id == "") {
      res.set_header("Set-Cookie", "userId=" + generate_user_id());
    }
    res.set_content(render_leaderboard(task, leaderboard, user_id, public_mode),
                    "text/html");
    res.status = 200;
  });
  svr.Post("/submit", [&](const httplib::Request &req, httplib::Response &res) {
    std::string user_id = find_user_id_in_request(req);
    if (req.has_param("code") && req.has_param("flags") &&
        req.has_param("author") && user_id != "") {
      std::string code = req.get_param_value("code");
      std::string flags = req.get_param_value("flags");
      std::string author = req.get_param_value("author");

      if (!(author == "Human" || author == "ChatGPT" || author == "HumanTeam" ||
            author == "HybridTeam" || author == "Teacher")) {
        res.set_content("Invalid form submission.", "text/plain");
        res.status = 404;
        return;
      }

      bool valid_code = validate_code_input(code);
      if (!valid_code) {
        res.set_content("Code does not comply with the rules!", "text/plain");
        res.status = 404;
        return;
      }

      bool valid_flags = validate_flags(flags);
      if (!valid_flags) {
        res.set_content("Disallowed compiler flags.", "text/plain");
        res.status = 404;
        return;
      }

      std::string submission_id = generate_submission_id();

      int exit_code = run_validated_submission(
          task, user_id, submission_id, code, flags, symbol, author, req.remote_addr);

      if (exit_code == 0) {
        submission_result result = load_submission_result(task, submission_id);

        leaderboard_entry e = make_leaderboard_entry(result);

        // save the entry
#if STORE_LEADERBOARD
        std::filesystem::path lbep =
            leaderboard_dir / (e.submission_id + ".json");
        nlohmann::json js = e;
        std::ofstream lbef(lbep.string());
        lbef << js.dump(2);
        lbef.close();
#endif

        // add entry to leaderboard
        leaderboard.push_back(std::move(e));
        sort_leaderboard(leaderboard);
      }

      res.set_redirect("view_submission?id=" + submission_id);

    } else {
      res.set_content("Invalid form submission.", "text/plain");
      res.status = 404;
    }
  });
  svr.Get("/view_submission", [&](const httplib::Request &req,
                                  httplib::Response &res) {
    std::string user_id = find_user_id_in_request(req);
    std::string submission_id = req.get_param_value("id");
    auto it = std::find_if(leaderboard.begin(), leaderboard.end(),
                           [&submission_id](const auto &e) {
                             return e.submission_id == submission_id;
                           });
    submission_result result = load_submission_result(task, submission_id);
    if (!result.found) {
      res.set_content("Submission not found.", "text/plain");
      res.status = 404;
      return;
    }

    if (!public_mode) {
      if (result.user_id != user_id) {
        res.set_content("Not your submission.", "text/plain");
        res.status = 403;
        return;
      }
    }

    std::string html = render_submission_result(result);
    res.set_content(html, "text/html");
  });

  std::string host = args["host"].as<std::string>();
  int port = args["port"].as<int>();
  std::printf("Server started at %s:%d.\n", host.c_str(), port);
  svr.listen(host, port);

  return 0;
}
