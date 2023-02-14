#include <httplib.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
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
  std::string id, task;
  std::string code;
  std::string flags;
  std::string disassembly;
  std::string disassembly_with_source;

  bool compile_successful{false};
  bool correctness_test_passed{false};

  int status{0};
  std::string compiler_output;
  double best_time{std::numeric_limits<double>::infinity()};
};

submission_result run_validated_submission(const std::string &task,
                                           const std::string &id,
                                           const std::string &code,
                                           const std::string &flags) {
  std::printf("Running submission.\n");
  std::filesystem::path submission_dir = "submissions";
  submission_dir /= task;
  submission_dir /= id;
  std::printf("   + mkdir: %s\n", submission_dir.c_str());
  std::filesystem::create_directories(submission_dir);

  std::filesystem::path submission_header =
      submission_dir / "submitted_code.hpp";
  std::printf("   + write code: %s\n", submission_header.c_str());
  std::ofstream file(submission_header.string());
  file << code;
  file.close();

  std::filesystem::path flags_path = submission_dir / "flags.txt";
  std::printf("   + write flags: %s\n", flags_path.c_str());
  std::ofstream flags_file(flags_path.string());
  flags_file << flags;
  flags_file.close();

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

  submission_result result;
  result.code = code;
  result.flags = flags;
  result.id = id;
  result.task = task;
  result.compiler_output =
      read_file(submission_dir / "compile_stderr.log.html");
  result.status = status;

  if (status != 1) {  // not failed
    result.compile_successful = true;
    result.disassembly = read_file(submission_dir / "disassembly.html");
    result.disassembly_with_source =
        read_file(submission_dir / "disassembly_with_source.html");

    if (status == 0) {
      result.correctness_test_passed = true;
      std::string content = read_file(submission_dir / "best_time.txt");
      result.best_time = std::atof(content.c_str());
    } else if (status == 2) {
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

int main(int argc, char **argv) {
  httplib::Server svr;
  if (argc != 2) {
    std::printf("run with arg: <taskname>");
    return 1;
  }
  std::string task = argv[1];

  svr.set_mount_point("/", "./runtime/static/");
  svr.Get("/leaderboard",
          [&](const httplib::Request &req, httplib::Response &res) {
            std::printf("Get\n");
            res.status = 200;
            res.body = "hello";
          });
  svr.Post("/submit", [&](const httplib::Request &req, httplib::Response &res) {
    std::printf("Post\n");
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

      submission_result result =
          run_validated_submission(task, "id0", code, flags);

      std::string html = read_file("runtime/templates/submission_result.html");
      html = replace_all(html, "${SUBMISSION_ID}", result.id);
      html = replace_all(html, "${COMPILER_FLAGS}", result.flags);
      html = replace_all(html, "${COMPILE_STATUS}",
                         result.compile_successful ? green("Success") : red("Failed"));
      html = replace_all(html, "${CORRECTNESS_TEST}",
                         result.correctness_test_passed ? green("Success") : red("Failed"));
      html = replace_all(html, "${BENCHMARK_BEST_TIME}",
                         format_time(result.best_time));
      html = replace_all(html, "${COMPILER_OUTPUT}", result.compiler_output);
      html = replace_all(html, "${DISASSEMBLY}", result.disassembly);
      html = replace_all(html, "${DISASSEMBLY_WITH_SOURCE}",
                         result.disassembly_with_source);

      res.set_content(html, "text/html");

    } else {
      res.set_content("Invalid form submission.", "text/plain");
      res.status = 404;
    }
  });

  std::printf("Server started at localhost:5000.\n");
  svr.listen("localhost", 5000);

  return 0;
}
