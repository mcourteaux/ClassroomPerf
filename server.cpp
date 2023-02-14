#include <httplib.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>

std::string read_file(std::filesystem::path path) {
  std::ifstream t(path.string());
  std::stringstream buffer;
  buffer << t.rdbuf();
  return buffer.str();
}

inline bool contains(const std::string &code, std::string substr) {
  return code.find(substr) != std::string::npos;
}
inline bool contains_regex(const std::string &code, std::string regex) {
  std::regex r(regex);
  return std::regex_search(code, r);
}
bool validate_code_input(const std::string &code) {
  // clang-format off
  static std::vector<std::string> bad_code_regex = {
      // spawn process:
      "system", "execl", "execlp", "execle", "execv", "execvp", "execvpe",
      "fork",
      // overriding main:
      "\\bmain\\b", "argv", "argc",
      // abusive memory:
      "calloc", "malloc", "free", "\\bnew\\b", "\\bmmap\\b",
      // multithrading:
      "pthread", "async", "launch", "thread",
      // File IO
      "fstream", "fopen", "fputc", "filesystem", "directory_iterator", "dirent", "opendir", "readdir", "fread", "fwrite",
      // Stdin/stdout
      "printf", "puts", "fputs", "putc", "\\bcout\\b", "\\bcerr\\b", "\\bcin\\b",
      // Competition rules
      "cmath", "math.h", "std::atan", "\\batan\\b", "\\batanf\\b", "\\batanl\\b",
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

  int status{0};
  std::string compiler_output;
  double best_time{std::numeric_limits<double>::infinity()};
};

struct leaderboard_entry {
  std::string task;
  std::string user_id;
  std::string submission_id;
  double best_time;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(leaderboard_entry, task, user_id,
                                   submission_id, best_time);

int run_validated_submission(const std::string &task,
                             const std::string &user_id,
                             const std::string &submission_id,
                             const std::string &code,
                             const std::string &flags) {
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

  std::printf("   + copy benchmark.cpp\n");
  std::filesystem::copy_file("benchmark.cpp", submission_dir / "benchmark.cpp",
                             std::filesystem::copy_options::overwrite_existing);

  std::string command = "/bin/bash ";
  command += std::filesystem::absolute("runtime/compile.sh").string();
  command += " ";
  command += submission_dir.string();
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
  result.submission_id = submission_id;
  result.task = task;
  result.compiler_output = read_file(submission_dir / "compile_stderr.log.html");
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
      result.best_time = std::atof(content.c_str());
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
  sprintf(buf, "%.3fms", time * 1000.0f);
  return std::string(buf);
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
                               const std::string &user_id) {
  std::string html = read_file("runtime/templates/leaderboard.html");
  html = replace_all(html, "${TASK}", task);
  std::string rows = "";
  for (size_t i = 0; i < entries.size(); ++i) {
    const leaderboard_entry &e = entries[i];
    if (user_id == e.user_id) {
      rows += "<tr style='background-color: #caddb7;'>";
    } else {
      rows += "<tr>";
    }
    rows += "<td>" + std::to_string(i) + "</td>";
    if (user_id == e.user_id) {
      rows += "<td><a href='view_submission?id=" + e.submission_id + "'>" +
              e.submission_id + "</a></td>";
    } else {
      rows += "<td>" + e.submission_id + "</td>";
    }
    rows += "<td>" + e.user_id + "</td>";
    rows += "<td>" + format_time(e.best_time) + "</td>";
    rows += "</tr>\n";
  }
  html = replace_all(html, "${LEADERBOARD_ROWS}", rows);
  return html;
}

std::string render_submission_result(const submission_result &result) {
  std::string html = read_file("runtime/templates/submission_result.html");
  html = replace_all(html, "${TASK}", result.task);
  html = replace_all(html, "${USER_ID}", result.user_id);
  html = replace_all(html, "${SUBMISSION_ID}", result.submission_id);
  html = replace_all(html, "${COMPILER_FLAGS}", result.flags);
  html =
      replace_all(html, "${COMPILE_STATUS}",
                  result.compile_successful ? green("Success") : red("Failed"));
  html = replace_all(
      html, "${CORRECTNESS_TEST}",
      result.correctness_test_passed ? green("Success") : red("Failed"));
  html = replace_all(html, "${BENCHMARK_BEST_TIME}",
                     format_time(result.best_time));
  html = replace_all(html, "${INPUT_CODE}", result.code);
  html = replace_all(html, "${COMPILER_OUTPUT}", result.compiler_output);
  html = replace_all(html, "${DISASSEMBLY}", result.disassembly);
  html = replace_all(html, "${DISASSEMBLY_WITH_SOURCE}",
                     result.disassembly_with_source);
  html = replace_all(html, "${BENCHMARK_OUTPUT}",
                     result.benchmark_output);
  return html;
}

static int submission_id_counter = 0;
std::string generate_submission_id() {
  char buf[100];
  submission_id_counter++;
  int rand_val = std::rand() % 0xffff;
  std::sprintf(buf, "%04x-%04x", rand_val, submission_id_counter);
  return std::string(buf);
}

void sort_leaderboard(std::vector<leaderboard_entry> &leaderboard) {
  std::sort(leaderboard.begin(), leaderboard.end(),
            [](auto &a, auto &b) { return a.best_time < b.best_time; });
}

int main(int argc, char **argv) {
  httplib::Server svr;
  if (argc != 2) {
    std::printf("run with arg: <taskname>");
    return 1;
  }
  std::string task = argv[1];

  std::srand(std::time(0));
  std::vector<leaderboard_entry> leaderboard;

  // Load the leaderboard for this task
  std::filesystem::path leaderboard_dir = "leaderboard";
  leaderboard_dir /= task;
  std::filesystem::create_directories(leaderboard_dir);
  submission_id_counter = 0;
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
  std::printf("Loaded %zu leaderboard entries.\n", leaderboard.size());
  sort_leaderboard(leaderboard);

  svr.set_mount_point("/", "./runtime/static/");
  svr.Get("/(leaderboard)?",
          [&](const httplib::Request &req, httplib::Response &res) {
            std::string user_id = anonimify(req.remote_addr, task);
            res.set_content(render_leaderboard(task, leaderboard, user_id),
                            "text/html");
            res.status = 200;
          });
  svr.Post("/submit", [&](const httplib::Request &req, httplib::Response &res) {
    if (req.has_param("code") && req.has_param("flags")) {
      std::string code = req.get_param_value("code");
      std::string flags = req.get_param_value("flags");
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

      std::string user_id = anonimify(req.remote_addr, task);
      std::string submission_id = generate_submission_id();

      int exit_code = run_validated_submission(task, user_id, submission_id, code, flags);

      if (exit_code == 0) {
        submission_result result = load_submission_result(task, submission_id);

        // add entry to leaderboard
        leaderboard_entry e;
        e.task = task;
        e.best_time = result.best_time;
        e.user_id = result.user_id;
        e.submission_id = result.submission_id;

        // save the entry
        std::filesystem::path lbep =
            leaderboard_dir / (e.submission_id + ".json");
        nlohmann::json js = e;
        std::ofstream lbef(lbep.string());
        lbef << js.dump(2);
        lbef.close();

        leaderboard.push_back(std::move(e));
        sort_leaderboard(leaderboard);
      }

      res.set_redirect("view_submission?id=" + submission_id);

    } else {
      res.set_content("Invalid form submission.", "text/plain");
      res.status = 404;
    }
  });
  svr.Get("/view_submission",
          [&](const httplib::Request &req, httplib::Response &res) {
            std::string user_id = anonimify(req.remote_addr, task);
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

            if (result.user_id != user_id) {
              res.set_content("Not your submission.", "text/plain");
              res.status = 403;
              return;
            }

            std::string html = render_submission_result(result);
            res.set_content(html, "text/html");
          });

  std::string host = "0.0.0.0";
  int port = 5000;
  std::printf("Server started at %s:%d.\n", host.c_str(), port);
  svr.listen(host, port);

  return 0;
}
