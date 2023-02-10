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
  static std::vector<std::string> bad_code = {
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
  // clang-format on
  for (const std::string &regex : bad_code) {
    if (contains_regex(code, regex)) {
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
  int status = std::system(command.c_str());

  std::printf("code: %d\n", status);

  submission_result result;
  result.code = code;
  result.flags = flags;
  result.id = id;
  result.task = task;
  result.compiler_output = read_file(submission_dir / "compile_stderr.log");
  result.status = status;

  if (status == 0) {
    std::string content = read_file(submission_dir / "best_time.txt");
    result.best_time = std::atof(content.c_str());
  }

  return result;
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

      std::string resp;
      resp += "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\n";
      resp += "<html>\n<head>\n <meta charset=\"UTF-8\"> </head><body><pre>";
      resp += "Id: " + result.id + "\n";
      resp += "Compiler flags: " + result.flags + "\n";
      resp += "Compiler output:";
      if (result.compiler_output.empty()) {
        resp += " (No output)";
      } else {
        resp += "\n\n";
        resp += result.compiler_output;
        resp += "\n\n";
      }
      resp += "\n";
      if (result.status == 1) {
        resp += "Did not compile.\n";
      } else {
        resp +=
            "Correctness test passed: " + std::to_string(result.status == 0);
        resp += "\n";
        resp += "Best time: " + std::to_string(result.best_time);
        resp += "\n";
      }

      resp += "</pre></body></html>";

      res.set_content(resp, "text/html");

    } else {
      res.set_content("Invalid form submission.", "text/plain");
      res.status = 404;
    }
  });

  std::printf("Server started at localhost:5000.\n");
  svr.listen("localhost", 5000);

  return 0;
}
