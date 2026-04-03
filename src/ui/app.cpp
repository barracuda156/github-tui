#include "app.h"
#include "../config.h"
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <regex>
#include <chrono>

using namespace ftxui;

App::App(std::shared_ptr<GitHubClient> client) : client_(client) {
    highlight_available_ = check_highlight_available();
}

void App::set_status(const std::string& message) {
    status_message_ = message;
}

std::string App::format_size(size_t bytes) const {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    return std::to_string(bytes / (1024 * 1024)) + " MB";
}

bool App::check_highlight_available() {
    FILE* pipe = popen("which highlight 2>/dev/null", "r");
    if (!pipe) return false;

    char buffer[256];
    bool found = (fgets(buffer, sizeof(buffer), pipe) != nullptr);
    pclose(pipe);

    return found;
}

std::string App::strip_ansi_codes(const std::string& text) {
    // Remove ANSI escape sequences: ESC[...m
    std::regex ansi_regex("\033\\[[0-9;]*m");
    return std::regex_replace(text, ansi_regex, "");
}

Color App::ansi_code_to_color(int code) {
    switch (code) {
        case 30: return Color::Black;
        case 31: return Color::Red;
        case 32: return Color::Green;
        case 33: return Color::Yellow;
        case 34: return Color::Blue;
        case 35: return Color::Magenta;
        case 36: return Color::Cyan;
        case 37: return Color::White;
        case 90: return Color::GrayDark;
        case 91: return Color::RedLight;
        case 92: return Color::GreenLight;
        case 93: return Color::YellowLight;
        case 94: return Color::BlueLight;
        case 95: return Color::MagentaLight;
        case 96: return Color::CyanLight;
        case 97: return Color::GrayLight;
        default: return Color::Default;
    }
}

Element App::parse_ansi_line(const std::string& line_with_ansi) {
    Elements segments;

    // Extract line number from the beginning
    size_t space_pos = line_with_ansi.find_first_of(" ");
    if (space_pos != std::string::npos) {
        std::string line_num_part = line_with_ansi.substr(0, space_pos + 1);
        std::string content_part = line_with_ansi.substr(space_pos + 1);

        // Add line number in grey
        segments.push_back(text(line_num_part) | color(Color::GrayDark));

        // Now parse the rest of the line for ANSI codes
        std::string current_text;
        Color current_color = Color::Default;
        bool bold = false;

        size_t i = 0;
        while (i < content_part.length()) {
            if (content_part[i] == '\033' && i + 1 < content_part.length() && content_part[i + 1] == '[') {
                // Found ANSI escape sequence
                // Save current text segment if any
                if (!current_text.empty()) {
                    auto element = text(current_text);
                    if (current_color != Color::Default) {
                        element = element | color(current_color);
                    }
                    if (bold) {
                        element = element | ftxui::bold;
                    }
                    segments.push_back(element);
                    current_text.clear();
                }

                // Parse the escape sequence
                i += 2; // Skip ESC[
                std::string code_str;
                while (i < content_part.length() && content_part[i] != 'm') {
                    code_str += content_part[i];
                    i++;
                }
                i++; // Skip 'm'

                // Process codes (can be semicolon-separated like "1;31")
                if (code_str == "0" || code_str.empty()) {
                    // Reset
                    current_color = Color::Default;
                    bold = false;
                } else {
                    // Split by semicolon and process each code
                    std::istringstream code_stream(code_str);
                    std::string single_code;
                    while (std::getline(code_stream, single_code, ';')) {
                        try {
                            int code = std::stoi(single_code);
                            if (code == 0) {
                                current_color = Color::Default;
                                bold = false;
                            } else if (code == 1) {
                                bold = true;
                            } else if (code >= 30 && code <= 37) {
                                current_color = ansi_code_to_color(code);
                            } else if (code >= 90 && code <= 97) {
                                current_color = ansi_code_to_color(code);
                            }
                        } catch (...) {
                            // Ignore invalid codes
                        }
                    }
                }
            } else {
                current_text += content_part[i];
                i++;
            }
        }

        // Add remaining text
        if (!current_text.empty()) {
            auto element = text(current_text);
            if (current_color != Color::Default) {
                element = element | color(current_color);
            }
            if (bold) {
                element = element | ftxui::bold;
            }
            segments.push_back(element);
        }

        return hbox(segments);
    }

    // Fallback for lines without line numbers (shouldn't happen)
    std::string current_text;
    Color current_color = Color::Default;
    bool bold = false;

    size_t i = 0;
    while (i < line_with_ansi.length()) {
        if (line_with_ansi[i] == '\033' && i + 1 < line_with_ansi.length() && line_with_ansi[i + 1] == '[') {
            // Found ANSI escape sequence
            // Save current text segment if any
            if (!current_text.empty()) {
                auto element = text(current_text);
                if (current_color != Color::Default) {
                    element = element | color(current_color);
                }
                if (bold) {
                    element = element | ftxui::bold;
                }
                segments.push_back(element);
                current_text.clear();
            }

            // Parse the escape sequence
            i += 2; // Skip ESC[
            std::string code_str;
            while (i < line_with_ansi.length() && line_with_ansi[i] != 'm') {
                code_str += line_with_ansi[i];
                i++;
            }
            i++; // Skip 'm'

            // Process codes (can be semicolon-separated like "1;31")
            if (code_str == "0" || code_str.empty()) {
                // Reset
                current_color = Color::Default;
                bold = false;
            } else {
                // Split by semicolon and process each code
                std::istringstream code_stream(code_str);
                std::string single_code;
                while (std::getline(code_stream, single_code, ';')) {
                    try {
                        int code = std::stoi(single_code);
                        if (code == 0) {
                            current_color = Color::Default;
                            bold = false;
                        } else if (code == 1) {
                            bold = true;
                        } else if (code >= 30 && code <= 37) {
                            current_color = ansi_code_to_color(code);
                        } else if (code >= 90 && code <= 97) {
                            current_color = ansi_code_to_color(code);
                        }
                    } catch (...) {
                        // Ignore invalid codes
                    }
                }
            }
        } else {
            current_text += line_with_ansi[i];
            i++;
        }
    }

    // Add remaining text
    if (!current_text.empty()) {
        auto element = text(current_text);
        if (current_color != Color::Default) {
            element = element | color(current_color);
        }
        if (bold) {
            element = element | ftxui::bold;
        }
        segments.push_back(element);
    }

    return segments.empty() ? text("") : hbox(segments);
}

std::string App::highlight_file(const std::string& content, const std::string& filename) {
    if (!highlight_available_) {
        return content;
    }

    // Extract just the filename (no path) for the temp file
    std::string basename = filename;
    size_t last_slash = filename.find_last_of('/');
    if (last_slash != std::string::npos) {
        basename = filename.substr(last_slash + 1);
    }

    // Handle special files: Portfile (MacPorts) -> treat as .tcl
    if (basename == "Portfile") {
        basename = "Portfile.tcl";
    }

    // Handle CMakeLists.txt -> treat as .cmake
    if (basename == "CMakeLists.txt") {
        basename = "CMakeLists.cmake";
    }

    // Create temp file with safe name
    std::string temp_file = "/tmp/github-tui-" + std::to_string(getpid()) + "-" + basename;
    std::ofstream ofs(temp_file);
    if (!ofs) {
        return content;
    }
    ofs << content;
    ofs.close();

    // Quote the filename to handle spaces and special chars
    std::string cmd = "highlight --out-format=ansi --force '" + temp_file + "' 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        unlink(temp_file.c_str());
        return content;
    }

    std::string result;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    int status = pclose(pipe);
    unlink(temp_file.c_str());

    // If highlight failed or returned empty, use original content
    if (status != 0 || result.empty()) {
        return content;
    }

    return result;
}

void App::load_repository() {
    auto repo = client_->get_repository(owner_, repo_);
    if (!repo) {
        set_status("Error: " + client_->get_last_error());
        return;
    }

    current_repo_ = repo;
    current_path_ = "";
    load_directory("");
}

void App::load_directory(const std::string& path) {
    auto contents = client_->get_directory_contents(owner_, repo_, path, current_repo_->default_branch);
    if (!contents) {
        set_status("Error: " + client_->get_last_error());
        return;
    }

    current_path_ = path;
    tree_items_ = *contents;

    filtered_items_.clear();

    if (!current_path_.empty()) {
        filtered_items_.push_back("[..]  Parent Directory");
    }

    for (const auto& item : tree_items_) {
        std::string icon = (item.type == "dir") ? "[DIR]  " : "";
        std::string size_str = (item.type == "file") ? " (" + format_size(item.size) + ")" : "";
        filtered_items_.push_back(icon + item.path + size_str);
    }

    selected_index_ = 0;
    focused_entry_ = 0;
    view_mode_ = ViewMode::TREE;

    std::string display_path = current_path_.empty() ? "/" : "/" + current_path_;
    set_status(display_path + " - " + std::to_string(tree_items_.size()) + " items");
}

void App::navigate_into(const std::string& dir_name) {
    std::string new_path = current_path_.empty() ? dir_name : current_path_ + "/" + dir_name;
    load_directory(new_path);
}

void App::navigate_up() {
    if (current_path_.empty()) {
        return;
    }

    size_t last_slash = current_path_.find_last_of('/');
    if (last_slash == std::string::npos) {
        load_directory("");
    } else {
        load_directory(current_path_.substr(0, last_slash));
    }
}

std::string App::format_commit_date(const std::string& iso_date) const {
    // Convert ISO 8601 date to readable format
    // Input: 2024-01-15T10:30:00Z
    // Output: 2024-01-15 10:30
    if (iso_date.length() >= 16) {
        return iso_date.substr(0, 10) + " " + iso_date.substr(11, 5);
    }
    return iso_date;
}

void App::load_commits(const std::string& path) {
    // Reset page to 1 if viewing commits for a different path
    if (path != commits_for_path_) {
        commits_page_ = 1;
    }

    auto commits = client_->get_commits(owner_, repo_, path, current_repo_->default_branch, commits_page_, 30);
    if (!commits) {
        set_status("Error: " + client_->get_last_error());
        return;
    }

    commits_ = *commits;
    commits_for_path_ = path;
    commit_display_items_.clear();
    return_from_commit_ = ViewMode::COMMITS;

    for (const auto& commit : commits_) {
        commit_display_items_.push_back(format_commit_display(commit));
    }

    selected_commit_index_ = 0;
    focused_commit_entry_ = 0;
    view_mode_ = ViewMode::COMMITS;

    std::string status = "Commits";
    if (!path.empty()) {
        status += " for " + path;
    }
    status += " - " + std::to_string(commits_.size()) + " commits";
    set_status(status);
}

void App::load_commit_detail(const std::string& sha) {
    auto detail = client_->get_commit_details(owner_, repo_, sha);
    if (!detail) {
        set_status("Error: " + client_->get_last_error());
        return;
    }

    current_commit_detail_ = detail;

    // Fetch and parse the diff patch
    auto patch = client_->get_commit_patch(owner_, repo_, sha);
    if (patch) {
        current_commit_patch_ = *patch;
        current_commit_diffs_ = client_->parse_diff(*patch);
    } else {
        current_commit_patch_.clear();
        current_commit_diffs_.clear();
    }

    commit_detail_scroll_position_ = 0;
    view_mode_ = ViewMode::COMMIT_DETAIL;
    set_status("Commit " + sha.substr(0, 7));
}

void App::save_file_locally() {
    if (file_content_.empty() || current_filename_.empty()) {
        set_status("Error: No file loaded");
        return;
    }

    // Use current filename as default save location
    std::string save_path = current_filename_;

    // If the file already exists in current directory, add a prefix
    std::ifstream test_file(save_path);
    if (test_file.good()) {
        test_file.close();
        save_path = "github-" + save_path;
    }

    std::ofstream outfile(save_path);
    if (!outfile) {
        set_status("Error: Failed to save file to " + save_path);
        return;
    }

    outfile << file_content_;
    outfile.close();

    // Get full path
    char cwd[4096];
    std::string full_path = save_path;
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        full_path = std::string(cwd) + "/" + save_path;
    }

    set_status("Saved to " + full_path);
}

void App::save_commit_patch() {
    if (current_commit_patch_.empty() || !current_commit_detail_) {
        set_status("Error: No commit loaded");
        return;
    }

    const auto& [commit, files] = *current_commit_detail_;
    std::string save_path = "commit-" + commit.sha.substr(0, 7) + ".patch";

    // If the file already exists, add a number suffix
    std::ifstream test_file(save_path);
    if (test_file.good()) {
        test_file.close();
        int counter = 1;
        while (true) {
            save_path = "commit-" + commit.sha.substr(0, 7) + "-" + std::to_string(counter) + ".patch";
            std::ifstream test(save_path);
            if (!test.good()) {
                break;
            }
            test.close();
            counter++;
        }
    }

    std::ofstream outfile(save_path);
    if (!outfile) {
        set_status("Error: Failed to save patch to " + save_path);
        return;
    }

    outfile << current_commit_patch_;
    outfile.close();

    // Get full path
    char cwd[4096];
    std::string full_path = save_path;
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        full_path = std::string(cwd) + "/" + save_path;
    }

    set_status("Saved patch to " + full_path);
}

void App::load_issues() {
    auto issues = client_->get_issues(owner_, repo_, issues_state_, issues_page_, 30);
    if (!issues) {
        set_status("Error: " + client_->get_last_error());
        return;
    }

    issues_ = *issues;
    issue_display_items_.clear();

    for (const auto& issue : issues_) {
        std::string state_icon = (issue.state == "open") ? "[O]" : "[C]";
        std::string title = issue.title;
        // Truncate long titles to prevent horizontal scrolling issues
        if (title.length() > 60) {
            title = title.substr(0, 57) + "...";
        }
        std::string display = state_icon + " #" + std::to_string(issue.number) + " - " + title +
                             " (" + issue.user.login + ")";
        issue_display_items_.push_back(display);
    }

    selected_issue_index_ = 0;
    focused_issue_entry_ = 0;
    view_mode_ = ViewMode::ISSUES;

    // Format state for display
    std::string state_display = capitalize_first(issues_state_);
    set_status("Issues (" + state_display + ", Page " + std::to_string(issues_page_) + ") - " + std::to_string(issues_.size()) + " items");
}

void App::load_issue_detail(int number) {
    auto issue = client_->get_issue(owner_, repo_, number);
    if (!issue) {
        set_status("Error: " + client_->get_last_error());
        return;
    }

    auto comments = client_->get_issue_comments(owner_, repo_, number);
    if (!comments) {
        set_status("Error: " + client_->get_last_error());
        return;
    }

    current_issue_ = issue;
    current_comments_ = *comments;
    issue_scroll_position_ = 0;
    view_mode_ = ViewMode::ISSUE_DETAIL;
    set_status("Issue #" + std::to_string(number));
}

void App::load_pull_requests() {
    auto prs = client_->get_pull_requests(owner_, repo_, prs_state_, pull_requests_page_, 30);
    if (!prs) {
        set_status("Error: " + client_->get_last_error());
        return;
    }

    pull_requests_ = *prs;
    pr_display_items_.clear();

    for (const auto& pr : pull_requests_) {
        std::string state_icon;
        if (pr.merged) {
            state_icon = "[M]";
        } else if (pr.state == "open") {
            state_icon = "[O]";
        } else {
            state_icon = "[C]";
        }
        std::string title = pr.title;
        // Truncate long titles to prevent horizontal scrolling issues
        if (title.length() > 60) {
            title = title.substr(0, 57) + "...";
        }
        std::string display = state_icon + " #" + std::to_string(pr.number) + " - " + title +
                             " (" + pr.user.login + ")";
        pr_display_items_.push_back(display);
    }

    selected_pr_index_ = 0;
    focused_pr_entry_ = 0;
    view_mode_ = ViewMode::PULL_REQUESTS;

    // Format state for display
    std::string state_display = capitalize_first(prs_state_);
    set_status("Pull Requests (" + state_display + ", Page " + std::to_string(pull_requests_page_) + ") - " + std::to_string(pull_requests_.size()) + " items");
}

void App::load_pr_detail(int number) {
    auto pr = client_->get_pull_request(owner_, repo_, number);
    if (!pr) {
        set_status("Error: " + client_->get_last_error());
        return;
    }

    auto comments = client_->get_pull_request_comments(owner_, repo_, number);
    if (!comments) {
        set_status("Error: " + client_->get_last_error());
        return;
    }

    auto files = client_->get_pull_request_files(owner_, repo_, number);
    if (!files) {
        set_status("Error: " + client_->get_last_error());
        return;
    }

    current_pr_ = pr;
    current_comments_ = *comments;
    current_pr_files_ = *files;
    pr_scroll_position_ = 0;
    view_mode_ = ViewMode::PR_DETAIL;
    set_status("Pull Request #" + std::to_string(number));
}

void App::load_notifications() {
    auto notifications = client_->get_notifications(notifications_state_, notifications_page_, 30);
    if (!notifications) {
        set_status("Error: " + client_->get_last_error());
        return;
    }

    notifications_ = *notifications;
    notification_display_items_.clear();

    for (const auto& notif : notifications_) {
        std::string type_display = "[" + notif.subject.type + "]";
        std::string repo_title = notif.repository.full_name + ": " + notif.subject.title;

        // Truncate long repo:title to prevent horizontal scrolling
        if (repo_title.length() > 80) {
            repo_title = repo_title.substr(0, 77) + "...";
        }

        // Format timestamp (simple approach - just show the date part)
        std::string time_display = notif.updated_at.substr(0, 10);  // YYYY-MM-DD

        std::string display = type_display + " " + repo_title + " (" + notif.reason + ") - " + time_display;
        notification_display_items_.push_back(display);
    }

    selected_notification_index_ = 0;
    focused_notification_entry_ = 0;
    view_mode_ = ViewMode::NOTIFICATIONS;

    // Format state for display
    std::string state_display = capitalize_first(notifications_state_);
    set_status("Notifications (" + state_display + ", Page " + std::to_string(notifications_page_) + ") - " + std::to_string(notifications_.size()) + " items");
}

void App::open_notification(const Notification& notif) {
    // Parse subject URL: https://api.github.com/repos/{owner}/{repo}/{type}/{number}
    std::string url = notif.subject.url;
    if (url.empty()) {
        set_status("No URL available for this notification");
        return;
    }

    // Extract owner, repo, and number from URL
    // URL format: https://api.github.com/repos/owner/repo/issues/123
    // or: https://api.github.com/repos/owner/repo/pulls/456
    std::string prefix = "https://api.github.com/repos/";
    if (url.find(prefix) != 0) {
        set_status("Unsupported notification URL format");
        return;
    }

    std::string path = url.substr(prefix.length());
    // path is now: owner/repo/issues/123 or owner/repo/pulls/456

    size_t first_slash = path.find('/');
    size_t second_slash = path.find('/', first_slash + 1);
    size_t third_slash = path.find('/', second_slash + 1);
    size_t fourth_slash = path.find('/', third_slash + 1);

    if (first_slash == std::string::npos || second_slash == std::string::npos ||
        third_slash == std::string::npos) {
        set_status("Failed to parse notification URL");
        return;
    }

    std::string extracted_owner = path.substr(0, first_slash);
    std::string extracted_repo = path.substr(first_slash + 1, second_slash - first_slash - 1);
    std::string type = path.substr(second_slash + 1, third_slash - second_slash - 1);
    std::string number_str;

    if (fourth_slash != std::string::npos) {
        number_str = path.substr(third_slash + 1, fourth_slash - third_slash - 1);
    } else {
        number_str = path.substr(third_slash + 1);
    }

    int number;
    try {
        number = std::stoi(number_str);
    } catch (...) {
        set_status("Failed to parse notification number");
        return;
    }

    // Set the repository context
    owner_ = extracted_owner;
    repo_ = extracted_repo;

    // Load the repository info if not already loaded or if it's different
    if (!current_repo_ || current_repo_->full_name != (owner_ + "/" + repo_)) {
        auto repo = client_->get_repository(owner_, repo_);
        if (repo) {
            current_repo_ = repo;
        }
    }

    // Navigate based on type
    if (type == "issues" && notif.subject.type == "Issue") {
        return_from_issue_detail_ = ViewMode::NOTIFICATIONS;
        load_issue_detail(number);
    } else if (type == "pulls" && notif.subject.type == "PullRequest") {
        return_from_pr_detail_ = ViewMode::NOTIFICATIONS;
        load_pr_detail(number);
    } else {
        set_status("Unsupported notification type: " + notif.subject.type);
    }
}

std::string App::get_github_url() const {
    if (!current_repo_) return "";
    return "https://github.com/" + owner_ + "/" + repo_;
}

void App::open_in_browser(const std::string& url) {
    std::string cmd;
#ifdef __APPLE__
    cmd = "open '" + url + "'";
#else
    cmd = "xdg-open '" + url + "' >/dev/null 2>&1";
#endif
    int result = system(cmd.c_str());
    if (result == 0) {
        set_status("Opened in browser");
    } else {
        set_status("Failed to open browser");
    }
}

void App::copy_to_clipboard(const std::string& text) {
    std::string cmd;
#ifdef __APPLE__
    cmd = "echo '" + text + "' | pbcopy";
#else
    // Try xclip first, then xsel as fallback
    cmd = "echo '" + text + "' | xclip -selection clipboard 2>/dev/null || echo '" + text + "' | xsel --clipboard 2>/dev/null";
#endif
    int result = system(cmd.c_str());
    if (result == 0) {
        set_status("Copied URL to clipboard: " + text);
    } else {
        set_status("Failed to copy to clipboard (install xclip or xsel)");
    }
}

void App::load_file(const std::string& filename) {
    std::string full_path = current_path_.empty() ? filename : current_path_ + "/" + filename;
    auto content = client_->get_file_content(owner_, repo_, full_path, current_repo_->default_branch);
    if (!content) {
        set_status("Error: " + client_->get_last_error());
        return;
    }

    current_filename_ = filename;
    file_content_ = *content;

    // Try to highlight the file
    std::string highlighted = highlight_file(file_content_, filename);

    file_lines_.clear();
    file_scroll_position_ = 0;

    std::istringstream stream(highlighted);
    std::string line;
    int line_num = 1;

    // Calculate total number of lines for alignment
    size_t total_lines = std::count(highlighted.begin(), highlighted.end(), '\n') + 1;

    while (std::getline(stream, line)) {
        std::string line_number = std::to_string(line_num++);

        // Calculate spacing for alignment up to line 999
        std::string spacing;
        if (total_lines >= 100) {
            // Align for 3-digit line numbers
            if (line_number.length() == 1) {
                spacing = "   ";  // 3 spaces
            } else if (line_number.length() == 2) {
                spacing = "  ";   // 2 spaces
            } else {
                spacing = " ";    // 1 space
            }
        } else if (total_lines >= 10) {
            // Align for 2-digit line numbers
            if (line_number.length() == 1) {
                spacing = "  ";   // 2 spaces
            } else {
                spacing = " ";    // 1 space
            }
        } else {
            // Single digit line numbers
            spacing = " ";        // 1 space
        }

        // Store line with ANSI codes for later parsing
        file_lines_.push_back(line_number + spacing + line);
    }

    view_mode_ = ViewMode::FILE;
    std::string status = "Viewing: " + full_path;
    set_status(status);
}

Component App::make_main_component() {
    InputOption input_option;
    input_option.multiline = false;
    input_option.on_enter = [this] {
        if (!owner_.empty() && !repo_.empty()) {
            load_repository();
        }
    };
    input_option.transform = [](InputState state) {
        state.element = state.element | color(Color::Black) | bgcolor(Color::White);
        return state.element;
    };

    auto owner_input = Input(&owner_, "", input_option);
    auto repo_input = Input(&repo_, "", input_option);
    auto url_input = Input(&repo_url_, "", input_option);
    auto load_button = Button("Load Repository", [this] {
        // Parse URL if provided
        if (!repo_url_.empty()) {
            // Parse GitHub URL: https://github.com/owner/repo
            std::string url = repo_url_;
            size_t github_pos = url.find("github.com/");
            if (github_pos != std::string::npos) {
                std::string path = url.substr(github_pos + 11); // Skip "github.com/"
                size_t slash_pos = path.find('/');
                if (slash_pos != std::string::npos) {
                    owner_ = path.substr(0, slash_pos);
                    std::string repo_part = path.substr(slash_pos + 1);
                    // Remove trailing slashes or paths
                    size_t end_pos = repo_part.find('/');
                    if (end_pos != std::string::npos) {
                        repo_ = repo_part.substr(0, end_pos);
                    } else {
                        repo_ = repo_part;
                    }
                }
            }
        }

        if (!owner_.empty() && !repo_.empty()) {
            load_repository();
        }
    });

    auto input_container = Container::Vertical({
        owner_input,
        repo_input,
        url_input,
        load_button,
    });

    MenuOption menu_option;
    menu_option.focused_entry = &focused_entry_;
    menu_option.entries_option.transform = [this](const EntryState& state) {
        auto element = text(state.label);
        // Color [DIR] markers in blue
        if (state.label.find("[DIR]") == 0) {
            element = hbox({
                text("[DIR]") | color(Color::Blue),
                text(state.label.substr(5))
            });
        }
        if (state.focused) {
            element = element | inverted;
        }
        if (state.active) {
            element = element | bold;
        }
        return element;
    };
    auto tree_menu = Menu(&filtered_items_, &selected_index_, menu_option);

    MenuOption commit_menu_option;
    commit_menu_option.focused_entry = &focused_commit_entry_;
    commit_menu_option.entries_option.transform = [this](const EntryState& state) {
        auto element = text(state.label);

        // Format: "{sha} - {date} - {author} - {title}"
        // Color sha and date in grey
        size_t first_dash = state.label.find(" - ");
        if (first_dash != std::string::npos) {
            size_t second_dash = state.label.find(" - ", first_dash + 3);
            if (second_dash != std::string::npos) {
                std::string sha_date = state.label.substr(0, second_dash);
                std::string author_title = state.label.substr(second_dash);

                element = hbox({
                    text(sha_date) | color(Color::GrayDark),
                    text(author_title)
                });
            }
        }

        if (state.focused) {
            element = element | inverted;
        }
        if (state.active) {
            element = element | bold;
        }
        return element;
    };
    auto commits_menu = Menu(&commit_display_items_, &selected_commit_index_, commit_menu_option);

    MenuOption issue_menu_option;
    issue_menu_option.focused_entry = &focused_issue_entry_;
    issue_menu_option.entries_option.transform = [this](const EntryState& state) {
        auto element = text(state.label);
        // Color the status markers
        if (state.label.find("[O]") == 0) {
            element = hbox({
                text("[O]") | color(Color::GreenLight),
                text(state.label.substr(3))
            });
        } else if (state.label.find("[C]") == 0) {
            element = hbox({
                text("[C]") | color(Color::Red),
                text(state.label.substr(3))
            });
        }
        if (state.focused) {
            element = element | inverted;
        }
        if (state.active) {
            element = element | bold;
        }
        return element;
    };
    auto issues_menu = Menu(&issue_display_items_, &selected_issue_index_, issue_menu_option);

    MenuOption pr_menu_option;
    pr_menu_option.focused_entry = &focused_pr_entry_;
    pr_menu_option.entries_option.transform = [this](const EntryState& state) {
        auto element = text(state.label);
        // Color the status markers
        if (state.label.find("[O]") == 0) {
            element = hbox({
                text("[O]") | color(Color::GreenLight),
                text(state.label.substr(3))
            });
        } else if (state.label.find("[C]") == 0) {
            element = hbox({
                text("[C]") | color(Color::Red),
                text(state.label.substr(3))
            });
        } else if (state.label.find("[M]") == 0) {
            element = hbox({
                text("[M]") | color(Color::Magenta),
                text(state.label.substr(3))
            });
        }
        if (state.focused) {
            element = element | inverted;
        }
        if (state.active) {
            element = element | bold;
        }
        return element;
    };
    auto prs_menu = Menu(&pr_display_items_, &selected_pr_index_, pr_menu_option);

    MenuOption notification_menu_option;
    notification_menu_option.focused_entry = &focused_notification_entry_;
    notification_menu_option.entries_option.transform = [this](const EntryState& state) {
        // Find which notification this label belongs to
        int index = -1;
        for (size_t i = 0; i < notification_display_items_.size(); ++i) {
            if (notification_display_items_[i] == state.label) {
                index = i;
                break;
            }
        }

        if (index < 0 || index >= static_cast<int>(notifications_.size())) {
            return text(state.label);
        }

        const auto& notif = notifications_[index];
        bool supported = (notif.subject.type == "Issue" || notif.subject.type == "PullRequest");

        // Parse the display string format: "[Type] repo: title (reason) - timestamp"
        // Find the positions of key separators
        size_t paren_start = state.label.find(" (");
        size_t paren_end = state.label.find(") - ");

        if (paren_start == std::string::npos || paren_end == std::string::npos) {
            // Fallback if parsing fails
            std::string prefix = state.active ? "> " : "";
            auto element = hbox({
                text(prefix),
                text(state.label)
            });
            if (!supported) {
                element = element | color(Color::GrayDark);
            } else if (notif.unread) {
                element = element | bold;
            }
            if (state.focused) {
                element = element | inverted;
            }
            return element;
        }

        // Extract parts
        std::string main_part = state.label.substr(0, paren_start);  // "[Type] repo: title"
        std::string reason_part = state.label.substr(paren_start, paren_end - paren_start + 1);  // "(reason)"
        std::string timestamp_part = state.label.substr(paren_end + 1);  // " - timestamp"

        // Build element with color coding, add > prefix if selected
        std::string prefix = state.active ? "> " : "";
        auto element = hbox({
            text(prefix),
            text(main_part),
            text(reason_part) | dim,
            text(timestamp_part) | dim
        });

        if (!supported) {
            element = element | color(Color::GrayDark);
        } else if (notif.unread) {
            element = element | bold;
        }

        if (state.focused) {
            element = element | inverted;
        }

        return element;
    };
    auto notifications_menu = Menu(&notification_display_items_, &selected_notification_index_, notification_menu_option);

    MenuOption pr_commit_menu_option;
    pr_commit_menu_option.focused_entry = &focused_pr_commit_entry_;
    pr_commit_menu_option.entries_option.transform = [this](const EntryState& state) {
        auto element = text(state.label);

        // Format: "{sha} - {date} - {author} - {title}"
        // Color sha and date in grey
        size_t first_dash = state.label.find(" - ");
        if (first_dash != std::string::npos) {
            size_t second_dash = state.label.find(" - ", first_dash + 3);
            if (second_dash != std::string::npos) {
                std::string sha_date = state.label.substr(0, second_dash);
                std::string author_title = state.label.substr(second_dash);

                element = hbox({
                    text(sha_date) | color(Color::GrayDark),
                    text(author_title)
                });
            }
        }

        if (state.focused) {
            element = element | inverted;
        }
        if (state.active) {
            element = element | bold;
        }
        return element;
    };
    auto pr_commits_menu = Menu(&pr_commit_display_items_, &selected_pr_commit_index_, pr_commit_menu_option);

    // Create issue form
    InputOption issue_title_option;
    issue_title_option.multiline = false;
    issue_title_option.transform = [](InputState state) {
        state.element = state.element | color(Color::Black) | bgcolor(Color::White);
        return state.element;
    };

    InputOption issue_body_option;
    issue_body_option.multiline = true;
    issue_body_option.transform = [](InputState state) {
        state.element = state.element | color(Color::Black) | bgcolor(Color::White) | size(HEIGHT, GREATER_THAN, 5);
        return state.element;
    };

    auto issue_title_input = Input(&new_issue_title_, "Title", issue_title_option);
    auto issue_body_input = Input(&new_issue_body_, "Body", issue_body_option);
    auto create_issue_container = Container::Vertical({
        issue_title_input,
        issue_body_input,
    });

    // Add comment form
    InputOption comment_option;
    comment_option.multiline = true;
    comment_option.transform = [](InputState state) {
        state.element = state.element | color(Color::Black) | bgcolor(Color::White) | size(HEIGHT, GREATER_THAN, 5);
        return state.element;
    };
    auto comment_body_input = Input(&new_comment_body_, "Comment", comment_option);

    auto input_with_maybe = Maybe(input_container, [this] { return view_mode_ == ViewMode::INPUT; });
    auto tree_with_maybe = Maybe(tree_menu, [this] { return view_mode_ == ViewMode::TREE; });
    auto commits_with_maybe = Maybe(commits_menu, [this] { return view_mode_ == ViewMode::COMMITS; });
    auto issues_with_maybe = Maybe(issues_menu, [this] { return view_mode_ == ViewMode::ISSUES; });
    auto prs_with_maybe = Maybe(prs_menu, [this] { return view_mode_ == ViewMode::PULL_REQUESTS; });
    auto notifications_with_maybe = Maybe(notifications_menu, [this] { return view_mode_ == ViewMode::NOTIFICATIONS; });
    auto pr_commits_with_maybe = Maybe(pr_commits_menu, [this] { return view_mode_ == ViewMode::PR_COMMITS; });
    auto create_issue_with_maybe = Maybe(create_issue_container, [this] { return view_mode_ == ViewMode::CREATE_ISSUE; });
    auto add_comment_with_maybe = Maybe(comment_body_input, [this] { return view_mode_ == ViewMode::ADD_COMMENT; });

    auto main_container = Container::Vertical({
        input_with_maybe,
        tree_with_maybe,
        commits_with_maybe,
        issues_with_maybe,
        prs_with_maybe,
        notifications_with_maybe,
        pr_commits_with_maybe,
        create_issue_with_maybe,
        add_comment_with_maybe,
    });

    return CatchEvent(Renderer(main_container, [this, owner_input, repo_input, url_input, load_button, tree_menu, commits_menu, issues_menu, prs_menu, notifications_menu, pr_commits_menu, issue_title_input, issue_body_input, comment_body_input] {
        if (view_mode_ == ViewMode::INPUT) {
            return vbox({
                text("GitHub Repository Viewer") | bold | center,
                separator(),
                hbox({text("     Owner: ") | dim, owner_input->Render()}),
                text(""),
                hbox({text("Repository: ") | dim, repo_input->Render()}),
                separator(),
                hbox({text("       URL: ") | dim, url_input->Render()}),
                separator(),
                load_button->Render() | center,
                separator(),
                text(status_message_),
                filler(),
                text("Press Ctrl+C to exit") | dim | center,
            }) | border;
        } else if (view_mode_ == ViewMode::TREE) {
            std::string repo_info;
            if (current_repo_) {
                repo_info = current_repo_->full_name + " - " +
                           current_repo_->description + " [★" +
                           std::to_string(current_repo_->stargazers_count) + "]";
            }

            return vbox({
                text(repo_info) | bold | center,
                separator(),
                tree_menu->Render() | vscroll_indicator | frame | flex,
                separator(),
                text(status_message_) | color(Color::Yellow),
                text("↑/↓: Navigate | Enter: View | h: History | i: Issues | p: PRs | b: Browser | u: Copy URL | q/Esc: Up/Back") | dim | center,
            }) | border;
        } else if (view_mode_ == ViewMode::COMMITS) {
            std::string title = "Commit History";
            if (!commits_for_path_.empty()) {
                title += " - " + commits_for_path_;
            }

            std::string page_info = " (Page " + std::to_string(commits_page_) + ")";
            return vbox({
                text(title + page_info) | bold | center,
                separator(),
                commits_menu->Render() | vscroll_indicator | frame | flex,
                separator(),
                text(status_message_) | color(Color::Yellow),
                text("↑/↓: Navigate | Enter: View Details | n: Next Page | P: Prev Page | q/Esc: Back") | dim | center,
            }) | border;
        } else if (view_mode_ == ViewMode::COMMIT_DETAIL) {
            Elements all_lines;

            if (current_commit_detail_) {
                const auto& [commit, files] = *current_commit_detail_;

                all_lines.push_back(text("Commit: " + commit.sha) | bold);
                all_lines.push_back(text("Author: " + commit.author.name + " <" + commit.author.email + ">"));
                all_lines.push_back(text("Date: " + format_commit_date(commit.author.date)));
                all_lines.push_back(separator());

                // Split commit message into lines
                std::istringstream msg_stream(commit.message);
                std::string msg_line;
                while (std::getline(msg_stream, msg_line)) {
                    all_lines.push_back(text("  " + msg_line));
                }

                all_lines.push_back(separator());
                all_lines.push_back(text("Files changed: " + std::to_string(files.size())) | bold);
                all_lines.push_back(separator());

                for (const auto& file : files) {
                    std::string status_icon;
                    Color status_color = Color::Default;
                    if (file.status == "added") {
                        status_icon = "[+]";
                        status_color = Color::Green;
                    } else if (file.status == "modified") {
                        status_icon = "[M]";
                        status_color = Color::Yellow;
                    } else if (file.status == "removed") {
                        status_icon = "[-]";
                        status_color = Color::Red;
                    } else {
                        status_icon = "[?]";
                    }

                    std::string changes = " (+" + std::to_string(file.additions) +
                                        "/-" + std::to_string(file.deletions) + ")";

                    all_lines.push_back(
                        hbox({
                            text(status_icon) | color(status_color),
                            text(" " + file.filename + changes)
                        })
                    );
                }

                // Render diffs if available
                if (!current_commit_diffs_.empty()) {
                    all_lines.push_back(separator());
                    all_lines.push_back(text("Diff:") | bold);
                    all_lines.push_back(separator());

                    for (const auto& diff_file : current_commit_diffs_) {
                        all_lines.push_back(text("diff --git a/" + diff_file.filename + " b/" + diff_file.filename) | color(Color::Cyan));

                        for (const auto& hunk : diff_file.hunks) {
                            // Hunk header
                            std::string hunk_header = "@@ -" + std::to_string(hunk.left_start) +
                                                     " +" + std::to_string(hunk.right_start) + " @@";
                            all_lines.push_back(text(hunk_header) | color(Color::Cyan));

                            // Diff lines
                            for (const auto& line : hunk.lines) {
                                Element line_elem = text(line.content);

                                if (line.type == DiffLine::Add) {
                                    line_elem = line_elem |
                                        color(Color::RGB(200, 255, 200)) |
                                        bgcolor(Color::RGB(0, 100, 0));
                                } else if (line.type == DiffLine::Delete) {
                                    line_elem = line_elem |
                                        color(Color::RGB(255, 200, 200)) |
                                        bgcolor(Color::RGB(100, 0, 0));
                                } else {
                                    // Keep lines - normal color
                                    line_elem = line_elem | color(Color::Default);
                                }

                                all_lines.push_back(line_elem);
                            }
                        }

                        all_lines.push_back(text("")); // Blank line between files
                    }
                }
            }

            // Render only visible lines starting from scroll position
            Elements visible_lines;
            for (size_t i = commit_detail_scroll_position_; i < all_lines.size(); ++i) {
                visible_lines.push_back(all_lines[i]);
            }

            return vbox({
                text(status_message_) | bold | center,
                separator(),
                vbox(visible_lines) | flex,
                separator(),
                text("↑/↓/PgUp/PgDn: Scroll | s: Save Patch | b: Browser | u: Copy URL | q/Esc: Back") | dim | center,
            }) | border;
        } else if (view_mode_ == ViewMode::ISSUES) {
            std::string state_display = capitalize_first(issues_state_);
            std::string title = "Issues (" + state_display + ", Page " + std::to_string(issues_page_) + ")";
            return vbox({
                text(title) | bold | center,
                separator(),
                issues_menu->Render() | vscroll_indicator | frame | flex,
                separator(),
                text(status_message_) | color(Color::Yellow),
                text("↑/↓: Navigate | Enter: View | o: New Issue | f: Filter | n: Next Page | P: Prev Page | b: Browser | u: Copy URL | q/Esc: Back") | dim | center,
            }) | border;
        } else if (view_mode_ == ViewMode::ISSUE_DETAIL) {
            Elements all_lines;

            if (current_issue_) {
                const auto& issue = *current_issue_;
                std::string state_color_str = (issue.state == "open") ? "OPEN" : "CLOSED";
                Color state_color = (issue.state == "open") ? Color::Green : Color::Red;

                all_lines.push_back(hbox({
                    text("Issue #" + std::to_string(issue.number) + " "),
                    text("[" + state_color_str + "]") | color(state_color)
                }) | bold);
                all_lines.push_back(text("Title: " + issue.title) | bold);
                all_lines.push_back(text("Author: " + issue.user.login));
                all_lines.push_back(text("Created: " + format_commit_date(issue.created_at)));
                all_lines.push_back(separator());

                // Body
                std::istringstream body_stream(issue.body);
                std::string body_line;
                while (std::getline(body_stream, body_line)) {
                    all_lines.push_back(text("  " + body_line));
                }

                // Comments
                if (!current_comments_.empty()) {
                    all_lines.push_back(separator());
                    all_lines.push_back(text("Comments (" + std::to_string(current_comments_.size()) + "):") | bold);
                    all_lines.push_back(separator());

                    for (const auto& comment : current_comments_) {
                        all_lines.push_back(text(comment.user.login + " (" + format_commit_date(comment.created_at) + "):") | bold);
                        std::istringstream comment_stream(comment.body);
                        std::string comment_line;
                        while (std::getline(comment_stream, comment_line)) {
                            all_lines.push_back(text("  " + comment_line));
                        }
                        all_lines.push_back(text(""));
                    }
                }
            }

            // Render only visible lines starting from scroll position
            Elements visible_lines;
            for (size_t i = issue_scroll_position_; i < all_lines.size(); ++i) {
                visible_lines.push_back(all_lines[i]);
            }

            std::string help_text = "↑/↓/PgUp/PgDn: Scroll | c: Comment | b: Browser | u: Copy URL | ";
            if (current_issue_ && current_issue_->state == "open") {
                help_text += "x: Close | ";
            } else if (current_issue_) {
                help_text += "o: Reopen | ";
            }
            help_text += "q/Esc: Back";

            return vbox({
                text(status_message_) | bold | center,
                separator(),
                vbox(visible_lines) | flex,
                separator(),
                text(help_text) | dim | center,
            }) | border;
        } else if (view_mode_ == ViewMode::PULL_REQUESTS) {
            std::string state_display = capitalize_first(prs_state_);
            std::string title = "Pull Requests (" + state_display + ", Page " + std::to_string(pull_requests_page_) + ")";
            return vbox({
                text(title) | bold | center,
                separator(),
                prs_menu->Render() | vscroll_indicator | frame | flex,
                separator(),
                text(status_message_) | color(Color::Yellow),
                text("↑/↓: Navigate | Enter: View | f: Filter | n: Next Page | P: Prev Page | b: Browser | u: Copy URL | q/Esc: Back") | dim | center,
            }) | border;
        } else if (view_mode_ == ViewMode::NOTIFICATIONS) {
            std::string state_display = capitalize_first(notifications_state_);
            std::string title = "Notifications (" + state_display + ", Page " + std::to_string(notifications_page_) + ")";
            return vbox({
                text(title) | bold | center,
                separator(),
                notifications_menu->Render() | vscroll_indicator | frame | flex,
                separator(),
                text(status_message_) | color(Color::Yellow),
                text("↑/↓: Navigate | Enter: Open | f: Filter | n: Next Page | P: Prev Page | q/Esc: Back") | dim | center,
            }) | border;
        } else if (view_mode_ == ViewMode::PR_DETAIL) {
            Elements all_lines;

            if (current_pr_) {
                const auto& pr = *current_pr_;
                std::string state_text;
                Color state_color;
                if (pr.merged) {
                    state_text = "MERGED";
                    state_color = Color::Magenta;
                } else if (pr.state == "open") {
                    state_text = "OPEN";
                    state_color = Color::Green;
                } else {
                    state_text = "CLOSED";
                    state_color = Color::Red;
                }

                all_lines.push_back(hbox({
                    text("Pull Request #" + std::to_string(pr.number) + " "),
                    text("[" + state_text + "]") | color(state_color)
                }) | bold);
                all_lines.push_back(text("Title: " + pr.title) | bold);
                all_lines.push_back(text("Author: " + pr.user.login));
                all_lines.push_back(text("Branch: " + pr.head_ref + " → " + pr.base_ref));
                all_lines.push_back(text("Created: " + format_commit_date(pr.created_at)));
                all_lines.push_back(separator());

                // Body
                std::istringstream body_stream(pr.body);
                std::string body_line;
                while (std::getline(body_stream, body_line)) {
                    all_lines.push_back(text("  " + body_line));
                }

                // Files changed (summary)
                if (!current_pr_files_.empty()) {
                    all_lines.push_back(separator());
                    all_lines.push_back(text("Files changed (" + std::to_string(current_pr_files_.size()) + "):") | bold);
                    all_lines.push_back(separator());

                    for (const auto& file : current_pr_files_) {
                        std::string status_icon;
                        Color file_color = Color::Default;
                        if (file.status == "added") {
                            status_icon = "[+]";
                            file_color = Color::Green;
                        } else if (file.status == "modified") {
                            status_icon = "[M]";
                            file_color = Color::Yellow;
                        } else if (file.status == "removed") {
                            status_icon = "[-]";
                            file_color = Color::Red;
                        } else {
                            status_icon = "[?]";
                        }

                        std::string changes = " (+" + std::to_string(file.additions) +
                                            " / -" + std::to_string(file.deletions) + ")";

                        all_lines.push_back(hbox({
                            text(status_icon) | color(file_color),
                            text(" " + file.filename + changes)
                        }));
                    }
                }

                // Comments
                if (!current_comments_.empty()) {
                    all_lines.push_back(separator());
                    all_lines.push_back(text("Comments (" + std::to_string(current_comments_.size()) + "):") | bold);
                    all_lines.push_back(separator());

                    for (const auto& comment : current_comments_) {
                        all_lines.push_back(text(comment.user.login + " (" + format_commit_date(comment.created_at) + "):") | bold);
                        std::istringstream comment_stream(comment.body);
                        std::string comment_line;
                        while (std::getline(comment_stream, comment_line)) {
                            all_lines.push_back(text("  " + comment_line));
                        }
                        all_lines.push_back(text(""));
                    }
                }
            }

            // Render only visible lines starting from scroll position
            Elements visible_lines;
            for (size_t i = pr_scroll_position_; i < all_lines.size(); ++i) {
                visible_lines.push_back(all_lines[i]);
            }

            std::string help_text = "↑/↓/PgUp/PgDn: Scroll | v: View Commits | c: Comment | b: Browser | u: Copy URL | ";
            if (current_pr_ && current_pr_->state == "open" && !current_pr_->merged) {
                help_text += "x: Close | ";
            }
            help_text += "q/Esc: Back";

            return vbox({
                text(status_message_) | bold | center,
                separator(),
                vbox(visible_lines) | flex,
                separator(),
                text(help_text) | dim | center,
            }) | border;
        } else if (view_mode_ == ViewMode::PR_COMMITS) {
            std::string title = "Pull Request #";
            if (current_pr_) {
                title += std::to_string(current_pr_->number) + " - Commits";
            }

            return vbox({
                text(title) | bold | center,
                separator(),
                pr_commits_menu->Render() | vscroll_indicator | frame | flex,
                separator(),
                text(status_message_) | color(Color::Yellow),
                text("↑/↓: Navigate | Enter: View Details | b: Browser | u: Copy URL | q/Esc: Back") | dim | center,
            }) | border;
        } else if (view_mode_ == ViewMode::CREATE_ISSUE) {
            return vbox({
                text("Create New Issue") | bold | center,
                separator(),
                hbox({text("Title: ") | dim, issue_title_input->Render()}),
                hbox({text("Body:  ") | dim, issue_body_input->Render()}),
                separator(),
                text("Enter: Submit | Esc: Cancel") | dim | center,
                separator(),
                text(status_message_),
            }) | border;
        } else if (view_mode_ == ViewMode::ADD_COMMENT) {
            return vbox({
                text("Add Comment") | bold | center,
                separator(),
                hbox({text("Comment: ") | dim, comment_body_input->Render()}),
                separator(),
                text("Enter: Submit | Esc: Cancel") | dim | center,
                separator(),
                text(status_message_),
            }) | border;
        } else {
            // Render only visible lines starting from scroll position
            Elements visible_lines;

            // Skip lines before scroll position, render the rest
            for (size_t i = file_scroll_position_; i < file_lines_.size(); ++i) {
                visible_lines.push_back(parse_ansi_line(file_lines_[i]));
            }

            std::string scroll_info = "";
            if (!file_lines_.empty()) {
                scroll_info = " [Line " + std::to_string(file_scroll_position_ + 1) + "/" +
                              std::to_string(file_lines_.size()) + "]";
            }

            return vbox({
                text(status_message_ + scroll_info) | bold | center,
                separator(),
                vbox(visible_lines) | flex,
                separator(),
                text("↑/↓/PgUp/PgDn: Scroll | s: Save | h: History | b: Browser | u: Copy URL | q/Esc: Back") | dim | center,
            }) | border;
        }
    }), [this](Event event) {
        if (view_mode_ == ViewMode::TREE) {
            if (check_double_click_or_enter(event)) {
                int offset = current_path_.empty() ? 0 : 1;

                if (selected_index_ == 0 && offset == 1) {
                    navigate_up();
                } else if (selected_index_ >= offset && selected_index_ < static_cast<int>(tree_items_.size() + offset)) {
                    const auto& item = tree_items_[selected_index_ - offset];
                    if (item.type == "dir") {
                        navigate_into(item.path);
                    } else if (item.type == "file") {
                        load_file(item.path);
                    }
                }
                return true;
            }

            if (event == Event::Escape || event.character() == "q") {
                // Context-sensitive: go up one directory, or to INPUT if at root
                if (current_path_.empty()) {
                    view_mode_ = ViewMode::INPUT;
                    status_message_.clear();
                } else {
                    navigate_up();
                }
                return true;
            }

            if (event.character() == "h") {
                load_commits();
                return true;
            }

            if (event.character() == "i") {
                load_issues();
                return true;
            }

            if (event.character() == "p") {
                load_pull_requests();
                return true;
            }

            if (event.character() == "b") {
                std::string url = get_github_url();
                if (!current_path_.empty()) {
                    url += "/tree/" + current_repo_->default_branch + "/" + current_path_;
                }
                open_in_browser(url);
                return true;
            }

            if (event.character() == "u") {
                std::string url = get_github_url();
                if (!current_path_.empty()) {
                    url += "/tree/" + current_repo_->default_branch + "/" + current_path_;
                }
                copy_to_clipboard(url);
                return true;
            }
        } else if (view_mode_ == ViewMode::COMMITS) {
            if (check_double_click_or_enter(event)) {
                if (selected_commit_index_ >= 0 && selected_commit_index_ < static_cast<int>(commits_.size())) {
                    load_commit_detail(commits_[selected_commit_index_].sha);
                }
                return true;
            }

            if (event.character() == "n") {
                // Next page of commits
                commits_page_++;
                load_commits(commits_for_path_);
                // Check if we got any commits
                if (commits_.empty()) {
                    commits_page_--; // Go back to previous page
                    load_commits(commits_for_path_);
                    set_status("No more commits");
                } else {
                    set_status("Commits page " + std::to_string(commits_page_));
                }
                return true;
            }

            if (event.character() == "P") {
                // Previous page of commits
                if (commits_page_ > 1) {
                    commits_page_--;
                    load_commits(commits_for_path_);
                    set_status("Commits page " + std::to_string(commits_page_));
                }
                return true;
            }

            if (event == Event::Escape || event.character() == "q") {
                view_mode_ = ViewMode::TREE;
                commits_.clear();
                commit_display_items_.clear();
                commits_for_path_.clear();
                commits_page_ = 1;
                status_message_.clear();
                return true;
            }
        } else if (view_mode_ == ViewMode::COMMIT_DETAIL) {
            if (event == Event::Escape || event.character() == "q") {
                view_mode_ = return_from_commit_;
                current_commit_detail_.reset();
                current_commit_diffs_.clear();
                current_commit_patch_.clear();
                commit_detail_scroll_position_ = 0;
                status_message_.clear();
                return true;
            }

            if (event.character() == "b" && current_commit_detail_) {
                const auto& [commit, files] = *current_commit_detail_;
                open_in_browser(get_github_url() + "/commit/" + commit.sha);
                return true;
            }

            if (event.character() == "u" && current_commit_detail_) {
                const auto& [commit, files] = *current_commit_detail_;
                copy_to_clipboard(get_github_url() + "/commit/" + commit.sha);
                return true;
            }

            if (event.character() == "s") {
                save_commit_patch();
                return true;
            }

            // Handle scrolling
            const int estimated_visible_lines = 25;
            int total_lines = 4; // header lines
            if (current_commit_detail_) {
                const auto& [commit, files] = *current_commit_detail_;
                std::istringstream msg_stream(commit.message);
                std::string line;
                while (std::getline(msg_stream, line)) total_lines++;
                total_lines += 3 + files.size(); // files section

                // Count diff lines
                if (!current_commit_diffs_.empty()) {
                    total_lines += 3; // separator + "Diff:" + separator
                    for (const auto& diff_file : current_commit_diffs_) {
                        total_lines += 1; // diff --git line
                        for (const auto& hunk : diff_file.hunks) {
                            total_lines += 1; // hunk header
                            total_lines += hunk.lines.size();
                        }
                        total_lines += 1; // blank line between files
                    }
                }
            }
            int max_scroll = std::max(0, total_lines - estimated_visible_lines);
            if (handle_scrolling(event, commit_detail_scroll_position_, max_scroll)) {
                return true;
            }
        } else if (view_mode_ == ViewMode::ISSUES) {
            if (check_double_click_or_enter(event)) {
                if (selected_issue_index_ >= 0 && selected_issue_index_ < static_cast<int>(issues_.size())) {
                    load_issue_detail(issues_[selected_issue_index_].number);
                }
                return true;
            }

            if (event == Event::Escape || event.character() == "q") {
                view_mode_ = ViewMode::TREE;
                issues_.clear();
                issue_display_items_.clear();
                status_message_.clear();
                return true;
            }

            if (event.character() == "o") {
                new_issue_title_.clear();
                new_issue_body_.clear();
                view_mode_ = ViewMode::CREATE_ISSUE;
                return true;
            }

            if (event.character() == "n") {
                // Next page of issues
                issues_page_++;
                load_issues();
                // Check if we got any issues
                if (issues_.empty()) {
                    issues_page_--; // Go back to previous page
                    load_issues();
                    set_status("No more issues");
                } else {
                    set_status("Issues page " + std::to_string(issues_page_));
                }
                return true;
            }

            if (event.character() == "P") {
                // Previous page of issues
                if (issues_page_ > 1) {
                    issues_page_--;
                    load_issues();
                    set_status("Issues page " + std::to_string(issues_page_));
                }
                return true;
            }

            if (event.character() == "f") {
                // Toggle state filter: all → open → closed → all
                if (issues_state_ == "all") {
                    issues_state_ = "open";
                } else if (issues_state_ == "open") {
                    issues_state_ = "closed";
                } else {
                    issues_state_ = "all";
                }
                issues_page_ = 1;  // Reset to page 1 when changing filter
                load_issues();
                return true;
            }

            if (event.character() == "b") {
                open_in_browser(get_github_url() + "/issues");
                return true;
            }

            if (event.character() == "u") {
                copy_to_clipboard(get_github_url() + "/issues");
                return true;
            }
        } else if (view_mode_ == ViewMode::ISSUE_DETAIL) {
            if (event == Event::Escape || event.character() == "q") {
                view_mode_ = return_from_issue_detail_;
                current_issue_.reset();
                current_comments_.clear();
                status_message_.clear();
                // Clear repository context if returning to notifications
                if (return_from_issue_detail_ == ViewMode::NOTIFICATIONS) {
                    owner_.clear();
                    repo_.clear();
                    current_repo_.reset();
                }
                return_from_issue_detail_ = ViewMode::ISSUES;  // Reset to default
                return true;
            }

            if (event.character() == "c" && current_issue_) {
                new_comment_body_.clear();
                comment_target_number_ = current_issue_->number;
                comment_is_pr_ = false;
                view_mode_ = ViewMode::ADD_COMMENT;
                return true;
            }

            if (event.character() == "x" && current_issue_ && current_issue_->state == "open") {
                if (client_->update_issue_state(owner_, repo_, current_issue_->number, "closed")) {
                    load_issue_detail(current_issue_->number);
                    set_status("Issue closed");
                } else {
                    set_status("Error: " + client_->get_last_error());
                }
                return true;
            }

            if (event.character() == "o" && current_issue_ && current_issue_->state == "closed") {
                if (client_->update_issue_state(owner_, repo_, current_issue_->number, "open")) {
                    load_issue_detail(current_issue_->number);
                    set_status("Issue reopened");
                } else {
                    set_status("Error: " + client_->get_last_error());
                }
                return true;
            }

            if (event.character() == "b" && current_issue_) {
                open_in_browser(get_github_url() + "/issues/" + std::to_string(current_issue_->number));
                return true;
            }

            if (event.character() == "u" && current_issue_) {
                copy_to_clipboard(get_github_url() + "/issues/" + std::to_string(current_issue_->number));
                return true;
            }

            // Handle scrolling
            const int estimated_visible_lines = 25;
            // Calculate total lines (we need to count them)
            int total_lines = 5; // header lines
            if (current_issue_) {
                std::istringstream body_stream(current_issue_->body);
                std::string line;
                while (std::getline(body_stream, line)) total_lines++;

                total_lines += 1 + current_comments_.size() * 2; // separator + comments headers
                for (const auto& comment : current_comments_) {
                    std::istringstream comment_stream(comment.body);
                    while (std::getline(comment_stream, line)) total_lines++;
                }
            }
            int max_scroll = std::max(0, total_lines - estimated_visible_lines);
            if (handle_scrolling(event, issue_scroll_position_, max_scroll)) {
                return true;
            }
        } else if (view_mode_ == ViewMode::PULL_REQUESTS) {
            if (check_double_click_or_enter(event)) {
                if (selected_pr_index_ >= 0 && selected_pr_index_ < static_cast<int>(pull_requests_.size())) {
                    load_pr_detail(pull_requests_[selected_pr_index_].number);
                }
                return true;
            }

            if (event == Event::Escape || event.character() == "q") {
                view_mode_ = ViewMode::TREE;
                pull_requests_.clear();
                pr_display_items_.clear();
                status_message_.clear();
                return true;
            }

            if (event.character() == "n") {
                // Next page of pull requests
                pull_requests_page_++;
                load_pull_requests();
                // Check if we got any pull requests
                if (pull_requests_.empty()) {
                    pull_requests_page_--; // Go back to previous page
                    load_pull_requests();
                    set_status("No more pull requests");
                } else {
                    set_status("Pull Requests page " + std::to_string(pull_requests_page_));
                }
                return true;
            }

            if (event.character() == "P") {
                // Previous page of pull requests
                if (pull_requests_page_ > 1) {
                    pull_requests_page_--;
                    load_pull_requests();
                    set_status("Pull Requests page " + std::to_string(pull_requests_page_));
                }
                return true;
            }

            if (event.character() == "f") {
                // Toggle state filter: all → open → closed → all
                if (prs_state_ == "all") {
                    prs_state_ = "open";
                } else if (prs_state_ == "open") {
                    prs_state_ = "closed";
                } else {
                    prs_state_ = "all";
                }
                pull_requests_page_ = 1;  // Reset to page 1 when changing filter
                load_pull_requests();
                return true;
            }

            if (event.character() == "b") {
                open_in_browser(get_github_url() + "/pulls");
                return true;
            }

            if (event.character() == "u") {
                copy_to_clipboard(get_github_url() + "/pulls");
                return true;
            }
        } else if (view_mode_ == ViewMode::NOTIFICATIONS) {
            if (check_double_click_or_enter(event)) {
                if (selected_notification_index_ >= 0 && selected_notification_index_ < static_cast<int>(notifications_.size())) {
                    const auto& notif = notifications_[selected_notification_index_];
                    // Check if notification type is supported
                    if (notif.subject.type == "Issue" || notif.subject.type == "PullRequest") {
                        // Mark as read and open
                        client_->mark_notification_read(notif.id);
                        notifications_[selected_notification_index_].unread = false;
                        open_notification(notif);
                    } else {
                        set_status("Unsupported notification type: " + notif.subject.type);
                    }
                }
                return true;
            }

            if (event == Event::Escape || event.character() == "q") {
                view_mode_ = ViewMode::INPUT;
                notifications_.clear();
                notification_display_items_.clear();
                status_message_.clear();
                return true;
            }

            if (event.character() == "n") {
                // Next page of notifications
                notifications_page_++;
                load_notifications();
                // Check if we got any notifications
                if (notifications_.empty()) {
                    notifications_page_--; // Go back to previous page
                    load_notifications();
                    set_status("No more notifications");
                } else {
                    set_status("Notifications page " + std::to_string(notifications_page_));
                }
                return true;
            }

            if (event.character() == "P") {
                // Previous page of notifications
                if (notifications_page_ > 1) {
                    notifications_page_--;
                    load_notifications();
                    set_status("Notifications page " + std::to_string(notifications_page_));
                }
                return true;
            }

            if (event.character() == "f") {
                // Toggle state filter: all → unread → participating → all
                if (notifications_state_ == "all") {
                    notifications_state_ = "unread";
                } else if (notifications_state_ == "unread") {
                    notifications_state_ = "participating";
                } else {
                    notifications_state_ = "all";
                }
                notifications_page_ = 1;  // Reset to page 1 when changing filter
                load_notifications();
                return true;
            }
        } else if (view_mode_ == ViewMode::PR_DETAIL) {
            if (event == Event::Escape || event.character() == "q") {
                view_mode_ = return_from_pr_detail_;
                current_pr_.reset();
                current_comments_.clear();
                current_pr_files_.clear();
                status_message_.clear();
                // Clear repository context if returning to notifications
                if (return_from_pr_detail_ == ViewMode::NOTIFICATIONS) {
                    owner_.clear();
                    repo_.clear();
                    current_repo_.reset();
                }
                return_from_pr_detail_ = ViewMode::PULL_REQUESTS;  // Reset to default
                return true;
            }

            if (event.character() == "c" && current_pr_) {
                new_comment_body_.clear();
                comment_target_number_ = current_pr_->number;
                comment_is_pr_ = true;
                view_mode_ = ViewMode::ADD_COMMENT;
                return true;
            }

            if (event.character() == "x" && current_pr_ && current_pr_->state == "open" && !current_pr_->merged) {
                if (client_->update_pull_request_state(owner_, repo_, current_pr_->number, "closed")) {
                    load_pr_detail(current_pr_->number);
                    set_status("Pull request closed");
                } else {
                    set_status("Error: " + client_->get_last_error());
                }
                return true;
            }

            if (event.character() == "b" && current_pr_) {
                open_in_browser(get_github_url() + "/pull/" + std::to_string(current_pr_->number));
                return true;
            }

            if (event.character() == "u" && current_pr_) {
                copy_to_clipboard(get_github_url() + "/pull/" + std::to_string(current_pr_->number));
                return true;
            }

            if (event.character() == "v" && current_pr_) {
                // Load commits for this PR
                auto commits = client_->get_pull_request_commits(owner_, repo_, current_pr_->number);
                if (commits) {
                    pr_commits_ = *commits;
                    pr_commit_display_items_.clear();

                    for (const auto& commit : pr_commits_) {
                        pr_commit_display_items_.push_back(format_commit_display(commit));
                    }

                    selected_pr_commit_index_ = 0;
                    focused_pr_commit_entry_ = 0;
                    return_from_commit_ = ViewMode::PR_COMMITS;
                    view_mode_ = ViewMode::PR_COMMITS;
                    set_status("PR #" + std::to_string(current_pr_->number) + " - " + std::to_string(pr_commits_.size()) + " commits");
                } else {
                    set_status("Error: " + client_->get_last_error());
                }
                return true;
            }

            // Handle scrolling
            const int estimated_visible_lines = 25;
            // Calculate total lines
            int total_lines = 5; // header lines
            if (current_pr_) {
                std::istringstream body_stream(current_pr_->body);
                std::string line;
                while (std::getline(body_stream, line)) total_lines++;

                total_lines += 1 + current_comments_.size() * 2; // comments section
                for (const auto& comment : current_comments_) {
                    std::istringstream comment_stream(comment.body);
                    while (std::getline(comment_stream, line)) total_lines++;
                }
            }
            int max_scroll = std::max(0, total_lines - estimated_visible_lines);
            if (handle_scrolling(event, pr_scroll_position_, max_scroll)) {
                return true;
            }
        } else if (view_mode_ == ViewMode::PR_COMMITS) {
            if (check_double_click_or_enter(event)) {
                if (selected_pr_commit_index_ >= 0 && selected_pr_commit_index_ < static_cast<int>(pr_commits_.size())) {
                    load_commit_detail(pr_commits_[selected_pr_commit_index_].sha);
                }
                return true;
            }

            if (event == Event::Escape || event.character() == "q") {
                view_mode_ = ViewMode::PR_DETAIL;
                pr_commits_.clear();
                pr_commit_display_items_.clear();
                status_message_.clear();
                return true;
            }

            if (event.character() == "b" && current_pr_) {
                open_in_browser(get_github_url() + "/pull/" + std::to_string(current_pr_->number) + "/commits");
                return true;
            }

            if (event.character() == "u" && current_pr_) {
                copy_to_clipboard(get_github_url() + "/pull/" + std::to_string(current_pr_->number) + "/commits");
                return true;
            }
        } else if (view_mode_ == ViewMode::CREATE_ISSUE) {
            if (event == Event::Escape) {
                view_mode_ = ViewMode::ISSUES;
                new_issue_title_.clear();
                new_issue_body_.clear();
                status_message_.clear();
                return true;
            }

            // Check for Ctrl+Enter (Return with ctrl modifier)
            if (event == Event::Return) {
                if (new_issue_title_.empty()) {
                    set_status("Error: Title cannot be empty");
                    return true;
                }

                auto issue = client_->create_issue(owner_, repo_, new_issue_title_, new_issue_body_);
                if (issue) {
                    set_status("Issue #" + std::to_string(issue->number) + " created");
                    load_issues();
                } else {
                    set_status("Error: " + client_->get_last_error());
                }
                return true;
            }
        } else if (view_mode_ == ViewMode::ADD_COMMENT) {
            if (event == Event::Escape) {
                if (comment_is_pr_) {
                    view_mode_ = ViewMode::PR_DETAIL;
                } else {
                    view_mode_ = ViewMode::ISSUE_DETAIL;
                }
                new_comment_body_.clear();
                status_message_.clear();
                return true;
            }

            // Check for Return (submit comment)
            if (event == Event::Return) {
                if (new_comment_body_.empty()) {
                    set_status("Error: Comment cannot be empty");
                    return true;
                }

                std::optional<Comment> comment;
                if (comment_is_pr_) {
                    comment = client_->create_pull_request_comment(owner_, repo_, comment_target_number_, new_comment_body_);
                } else {
                    comment = client_->create_issue_comment(owner_, repo_, comment_target_number_, new_comment_body_);
                }

                if (comment) {
                    set_status("Comment added");
                    if (comment_is_pr_) {
                        load_pr_detail(comment_target_number_);
                    } else {
                        load_issue_detail(comment_target_number_);
                    }
                } else {
                    set_status("Error: " + client_->get_last_error());
                }
                return true;
            }
        } else if (view_mode_ == ViewMode::FILE) {
            if (event == Event::Escape || event.character() == "q") {
                view_mode_ = ViewMode::TREE;
                file_content_.clear();
                file_lines_.clear();
                file_scroll_position_ = 0;
                status_message_.clear();
                return true;
            }

            if (event.character() == "h") {
                // Show commit history for this file
                std::string full_path = current_path_.empty() ? current_filename_ : current_path_ + "/" + current_filename_;
                load_commits(full_path);
                return true;
            }

            if (event.character() == "s") {
                save_file_locally();
                return true;
            }

            if (event.character() == "b") {
                std::string full_path = current_path_.empty() ? current_filename_ : current_path_ + "/" + current_filename_;
                open_in_browser(get_github_url() + "/blob/" + current_repo_->default_branch + "/" + full_path);
                return true;
            }

            if (event.character() == "u") {
                std::string full_path = current_path_.empty() ? current_filename_ : current_path_ + "/" + current_filename_;
                copy_to_clipboard(get_github_url() + "/blob/" + current_repo_->default_branch + "/" + full_path);
                return true;
            }

            // Handle keyboard and mouse wheel scrolling
            // Estimate visible lines in window (typical terminal height minus UI elements)
            const int estimated_visible_lines = 25;
            int max_scroll = std::max(0, static_cast<int>(file_lines_.size()) - estimated_visible_lines);

            if (handle_scrolling(event, file_scroll_position_, max_scroll)) {
                return true;
            }

            return false;
        }

        // Global: Capital N switches to notifications from anywhere
        if (event.character() == "N") {
            load_notifications();
            return true;
        }

        // Global: Capital Q returns to initial screen from anywhere
        if (event.character() == "Q") {
            view_mode_ = ViewMode::INPUT;
            status_message_.clear();
            current_path_.clear();
            // Clear any loaded data
            file_content_.clear();
            file_lines_.clear();
            current_commit_detail_.reset();
            current_commit_diffs_.clear();
            current_commit_patch_.clear();
            current_issue_.reset();
            current_comments_.clear();
            current_pr_.reset();
            current_pr_files_.clear();
            commits_.clear();
            commits_page_ = 1;
            issues_.clear();
            issues_page_ = 1;
            pull_requests_.clear();
            pull_requests_page_ = 1;
            notifications_.clear();
            notifications_page_ = 1;
            pr_commits_.clear();
            return true;
        }

        return false;
    });
}

bool App::handle_scrolling(Event& event, int& scroll_position, int max_scroll) {
    if (event == Event::ArrowUp) {
        scroll_position = std::max(0, scroll_position - 1);
        return true;
    } else if (event == Event::ArrowDown) {
        scroll_position = std::min(max_scroll, scroll_position + 1);
        return true;
    } else if (event == Event::PageUp) {
        scroll_position = std::max(0, scroll_position - 20);
        return true;
    } else if (event == Event::PageDown) {
        scroll_position = std::min(max_scroll, scroll_position + 20);
        return true;
    } else if (event == Event::Home) {
        scroll_position = 0;
        return true;
    } else if (event == Event::End) {
        scroll_position = max_scroll;
        return true;
    }

    // Mouse wheel scrolling
    if (event.is_mouse()) {
        if (event.mouse().button == Mouse::WheelUp) {
            scroll_position = std::max(0, scroll_position - 3);
            return true;
        } else if (event.mouse().button == Mouse::WheelDown) {
            scroll_position = std::min(max_scroll, scroll_position + 3);
            return true;
        }
    }

    return false;
}

bool App::check_double_click_or_enter(Event& event) {
    if (event == Event::Return) {
        return true;
    }

    if (event.is_mouse() && event.mouse().button == Mouse::Left &&
        event.mouse().motion == Mouse::Pressed) {
        static auto last_click_time = std::chrono::steady_clock::now();
        static int last_click_y = -1;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_click_time).count();

        if (elapsed < 500 && last_click_y == event.mouse().y) {
            last_click_time = now;
            last_click_y = event.mouse().y;
            return true;
        }

        last_click_time = now;
        last_click_y = event.mouse().y;
    }

    return false;
}

std::string App::capitalize_first(const std::string& str) const {
    if (str.empty()) return str;
    std::string result = str;
    result[0] = std::toupper(result[0]);
    return result;
}

std::string App::format_commit_display(const Commit& commit) const {
    // Get first line of commit message
    std::string first_line = commit.message;
    size_t newline_pos = first_line.find('\n');
    if (newline_pos != std::string::npos) {
        first_line = first_line.substr(0, newline_pos);
    }

    // Truncate author name and message to prevent horizontal scrolling
    std::string author = commit.author.name;
    if (author.length() > 20) {
        author = author.substr(0, 17) + "...";
    }

    if (first_line.length() > 50) {
        first_line = first_line.substr(0, 47) + "...";
    }

    return commit.sha.substr(0, 7) + " - " +
           format_commit_date(commit.author.date) + " - " +
           author + " - " + first_line;
}

void App::run() {
    // Check config for starting page
    auto& config = Config::instance();
    auto starting_page = config.get_starting_page();

    if (starting_page.has_value()) {
        const std::string& page = starting_page.value();

        if (page == "notifications") {
            // Start with notifications view
            load_notifications();
        } else if (page.find("https://github.com/") == 0) {
            // Parse GitHub URL and load repository
            repo_url_ = page;
            load_repository();
        }
        // else: invalid value, stay in INPUT view (default)
    }
    // else: no starting_page configured, stay in INPUT view (default)

    auto screen = ScreenInteractive::Fullscreen();
    screen.Loop(make_main_component());
}
