#include <ncurses.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <ftw.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <tuple>
#include <functional>
#include <unordered_map>

inline float deg2rad(float deg) 
{
  return (deg * M_PI) / 180.f;
}

std::string format_size_human(std::size_t bytes) 
{
  // cit. "640 TB ought to be enough for anybody"
  std::vector<std::string> magnitudes = {"KB", "MB", "GB", "TB"};
  auto it = magnitudes.begin();
  double fnum = bytes;
  std::string unit = "bytes";
  while (fnum >= 1024.0 && it != magnitudes.end()) {
    unit = *it;
    ++it;
    fnum /= 1024.0;
  }
  std::stringstream ss;
  ss.precision(2);
  ss << std::fixed << fnum << " " << unit;
  return ss.str();
}

std::string get_cwd() 
{
  char buff[PATH_MAX + 1];
  char *cwd = getcwd(buff, sizeof(buff) - 1);
  if (cwd != NULL)
    return std::string(cwd);
  else {
    printf("Cannot determine current working directory: %s\n", strerror(errno));
    exit(1);
  }
}

bool exists_path(std::string path) 
{
  struct stat st;
  if (stat(path.c_str(), &st) == 0)
    if ((st.st_mode & S_IFDIR) != 0)
      return true;
  return false;
}

namespace {

  static std::size_t running_size = 0;
  int visitor_sum_size(const char*, const struct stat *sb, int, struct FTW*)
  {
    running_size += sb->st_size;
    return 0;
  }

}

std::vector<std::tuple<std::string, std::size_t, bool, float>> get_files(std::string path)
{
  std::vector<std::tuple<std::string, std::size_t, bool, float>> files;

  if (access(path.c_str(), R_OK) != 0) {
    printf("Insufficient user privileges to access %s", path.c_str());
    exit(1);
  }

  DIR *dirp = opendir(path.c_str());
  if (dirp == NULL) {
    perror(NULL);
    exit(1);
  }

  struct dirent *dent;
  char temp[PATH_MAX];

  while ((dent = readdir(dirp)) != NULL) 
  {
    if ((strcmp(dent->d_name, ".") == 0) || (strcmp(dent->d_name, "..") == 0))
      continue;

    struct stat st;
    sprintf(temp, "%s/%s", path.c_str(), dent->d_name);
    if (lstat(temp, &st) != 0)
      continue;

    std::string filename = dent->d_name;
    bool is_dir = S_ISDIR(st.st_mode);

    running_size = 0;
    if (nftw(temp, &visitor_sum_size, 1, FTW_PHYS)) {
      printf("Error while accessing %s\n", temp);
      perror("ftw");
    }
    
    files.emplace_back(std::move(filename), running_size, is_dir, 0.f /* Will be populated later*/);
  }
  closedir(dirp);

  return files;
}

void scan_path(std::string path,
               std::vector<std::tuple<std::string, std::size_t, bool, float>>& files,
               std::unordered_map<std::string, std::size_t /* Index in files */>& hex_to_dir_map)
{
  clear();
  files.clear();
  hex_to_dir_map.clear();

  printw("Scanning, please wait..");
  refresh();

  files = get_files(path);

  clear();
  printw("Press ESC to exit");

  const int center_x = COLS / 2;
  const int center_y = LINES / 2;

  if (files.empty()) {
    mvwprintw(stdscr, center_y, center_x, "No files or directories detected in the specified path");
    return;
  }

  // hex_to_dir_map ids
  std::size_t hex_id = 0;

  const int ellipse_width = COLS / 5;
  const int ellipse_height = LINES / 5;

  const std::size_t total_files_size = std::accumulate(files.begin(), files.end(), std::size_t(0), [](std::size_t a, const auto& b) {
    size_t size = 0;
    std::tie(std::ignore, size, std::ignore, std::ignore) = b;
    return a + size;
  });
  for(auto& f : files) {
    size_t size;
    float percentage;
    std::tie(std::ignore, size, std::ignore, percentage) = f;
    std::get<3>(f) = (float)size / total_files_size; // Percentages in range [0;1]
  }

  // Sort files in ascending order
  std::sort(files.begin(), files.end(), [](const auto& e1, const auto& e2) {
    return std::get<1>(e1) < std::get<1>(e2);
  });

  auto to_hex_string = [](std::size_t val, std::ios_base& (*f)(std::ios_base&)) {
    std::ostringstream oss;
    oss << f << val;
    return oss.str();
  };

  // ncurses starting color
  int current_color = 1;

  attron(COLOR_PAIR(current_color));
  std::size_t element_index = 0;
  float previous_limit = 0.f;
  float percentage = std::get<3>(files[element_index]);
  float next_limit = 360.f * percentage;
  bool label_printed = false;
  for(float deg = 0; deg < 360.0f; deg += 1.0f)
  {
    if (deg >= next_limit) {

      ++element_index;
      if (element_index >= files.size())
        break; // All elements drawn
      previous_limit = next_limit;
      percentage = std::get<3>(files[element_index]);
      next_limit += 360.f * percentage;
      label_printed = false;
      current_color = (current_color + 1)  % 8;
      if (current_color == 0)
        ++current_color;
      attron(COLOR_PAIR(current_color));
    }

    const float rad = deg2rad(deg);
    float half_w = ellipse_width;
    float half_h = ellipse_height;
    float sin_th = sin(rad) * sin(rad);
    float cos_th = cos(rad) * cos(rad);
    const float radius = half_w * half_h / sqrt(half_w * half_w * sin_th + half_h * half_h * cos_th);

    for(float dr = 0.1f; dr < radius; dr += 0.1f)
    {
      float dx = dr * cos(rad);
      float dy = dr * sin(rad);
      mvaddch(center_y - dy, center_x + dx, ' ');
    }

    if (!label_printed && abs(deg - ((previous_limit + next_limit) / 2.f)) <= 1.0f) {

      label_printed = true;

      float label_dock_point = radius + 4.f;

      std::ostringstream ss;
      std::string filename;
      std::size_t size;
      bool is_dir = false;
      std::tie(filename, size, is_dir, std::ignore) = files[element_index];

      std::string hex_key;
      if (is_dir) {
        hex_key = to_hex_string(hex_id++, std::hex);
        hex_to_dir_map.emplace(hex_key, element_index);
      }

      std::string left_part, mid_part, right_part;

      mid_part = filename;
      if (filename.size() > 30)
        mid_part = filename.substr(0, 30) + "...";

      if (is_dir)
        ss << "{" << hex_key << "} [DIR] ";

      left_part = ss.str();

      ss.str("");
      ss << " (" << format_size_human(size) << ")";

      right_part = ss.str();

      float dx = label_dock_point * cos(rad);
      float dy = label_dock_point * sin(rad);

      attron(COLOR_PAIR(8));

      std::string final = left_part + mid_part + right_part;

      int x_point = center_x + dx;
      if (x_point > 0 && x_point < center_x) { // Left region
        if ((int)final.size() > x_point)
          final.resize(x_point);
        x_point = std::max(x_point - (int)final.size(), 0);
      } else if (x_point > 0 && x_point >= center_x) {
        if (x_point + (int)final.size() >= COLS)
          final.resize(COLS - x_point);
      } else if (x_point == 0 || x_point == COLS)
        final = ""; // No space

      mvwprintw(stdscr, center_y - dy, x_point, final.c_str());

      attron(COLOR_PAIR(current_color));
    }

  }
  attron(COLOR_PAIR(8));
}

int main(int argc, char *argv[])
{
  std::vector<std::string> path_history;
  std::string path;
  if (argc < 2) {
   path = get_cwd();   
  } else {
    path = argv[1];
    if (exists_path(path) == false) {
      printf("Invalid folder: '%s'\n", path.c_str());
      exit(1);
    }
  }

  // Enumerate files and directories
  // <relative name, size in bytes, is directory, folder size percentage>
  std::vector<std::tuple<std::string, std::size_t, bool, float>> files;
  std::unordered_map<std::string, std::size_t /* Index in files */> hex_to_dir_map; // Will be computed later

  initscr(); // Start curses mode. This also initializes COLS, LINES
  cbreak(); // Line buffering disabled, everything is passed to us
  curs_set(0); // Disable cursor
  if (has_colors() == false) {
    endwin();
    printf("Your terminal does not support color\n");
    exit(1);
  }
  start_color();
  keypad(stdscr, true); // Intercept keypad

  init_pair(1, COLOR_RED, COLOR_RED);
  init_pair(2, COLOR_GREEN, COLOR_GREEN);
  init_pair(3, COLOR_YELLOW, COLOR_YELLOW);
  init_pair(4, COLOR_BLUE, COLOR_BLUE);
  init_pair(5, COLOR_MAGENTA, COLOR_MAGENTA);
  init_pair(6, COLOR_CYAN, COLOR_CYAN);
  init_pair(7, COLOR_WHITE, COLOR_WHITE);
  init_pair(8, COLOR_WHITE, COLOR_BLACK); // Normal text

  path_history.push_back(path);
  scan_path(path, files, hex_to_dir_map);

  int input_cursor_x = 0, input_cursor_y = 0;

  auto print_commands = [&]() {

    mvwprintw(stdscr, 2, 0, "Current dir: ");
    wprintw(stdscr, path.c_str());

    if (!path_history.empty())
      mvwprintw(stdscr, LINES - 2, 0, "Use 'p' to come back to the parent folder");

    mvwprintw(stdscr, LINES - 3, 0, "Scan folder: ");
    getyx(stdscr, input_cursor_y, input_cursor_x);
  };

  print_commands();
  refresh();

  std::string entered_key;
  int ch;
  while((ch = getch()) != 0x1B /* Escape */)
  {
    switch(ch) {
      case 0xA /* Enter */: {
        if (entered_key.compare("p") == 0) {
          entered_key = "";
          path_history.pop_back(); // Pop current
          if (path_history.empty()) {
            // No parents
            endwin();
            printf("No other parents to scan\n");
            exit(0);
          }
          path = path_history.back();
          scan_path(path, files, hex_to_dir_map);
          print_commands();
          refresh();
          continue;
        }
        auto it = hex_to_dir_map.find(entered_key);
        if (it == hex_to_dir_map.end()) {
          entered_key = "";
          mvwprintw(stdscr, LINES - 3, 0, "Key not recognized. Try again: ");
          getyx(stdscr, input_cursor_y, input_cursor_x);
          wclrtoeol(stdscr);
          refresh();
        } else {
          entered_key = "";
          if (path.back() != '/')
            path += '/';
          path += std::get<0>(files[it->second]);
          path_history.push_back(path);
          scan_path(path, files, hex_to_dir_map);
          print_commands();
          refresh();
        }
        continue;
      } break;
      case 0x107 /* Backspace */: {
        if (!entered_key.empty())
          entered_key.resize(entered_key.size() - 1);
        wmove(stdscr, input_cursor_y, input_cursor_x);
        wclrtoeol(stdscr);
        continue; // Prevent insertion into the buffer
      }
    }

    entered_key += (char)ch;
  }

  endwin(); // End curses mode

  return 0;
}
