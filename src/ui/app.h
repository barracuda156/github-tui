#pragma once

#include "../github_client.h"
#include <ftxui/component/component.hpp>
#include <memory>
#include <vector>

class App {
public:
    explicit App(std::shared_ptr<GitHubClient> client);

    void run();

private:
    std::shared_ptr<GitHubClient> client_;

    std::string owner_;
    std::string repo_;
    std::string repo_url_;
    std::optional<Repository> current_repo_;
    std::vector<TreeItem> tree_items_;
    std::vector<std::string> filtered_items_;
    int selected_index_ = 0;
    int focused_entry_ = 0;
    std::string current_path_;
    std::string file_content_;
    std::vector<std::string> file_lines_;
    int file_scroll_position_ = 0;
    std::string current_filename_;
    bool highlight_available_ = false;
    std::string status_message_;

    std::vector<Commit> commits_;
    std::vector<std::string> commit_display_items_;
    int selected_commit_index_ = 0;
    int focused_commit_entry_ = 0;
    std::string commits_for_path_;
    int commits_page_ = 1;
    std::optional<std::pair<Commit, std::vector<CommitFile>>> current_commit_detail_;
    std::vector<DiffFile> current_commit_diffs_;
    std::string current_commit_patch_;
    int commit_detail_scroll_position_ = 0;

    std::vector<Issue> issues_;
    std::vector<std::string> issue_display_items_;
    int selected_issue_index_ = 0;
    int focused_issue_entry_ = 0;
    int issues_page_ = 1;
    std::string issues_state_ = "all";
    std::optional<Issue> current_issue_;
    std::vector<Comment> current_comments_;
    int issue_scroll_position_ = 0;

    std::vector<PullRequest> pull_requests_;
    std::vector<std::string> pr_display_items_;
    int selected_pr_index_ = 0;
    int focused_pr_entry_ = 0;
    int pull_requests_page_ = 1;
    std::string prs_state_ = "all";
    std::optional<PullRequest> current_pr_;
    std::vector<CommitFile> current_pr_files_;
    std::vector<Commit> pr_commits_;
    std::vector<std::string> pr_commit_display_items_;
    int selected_pr_commit_index_ = 0;
    int focused_pr_commit_entry_ = 0;
    int pr_scroll_position_ = 0;

    std::vector<Notification> notifications_;
    std::vector<std::string> notification_display_items_;
    int selected_notification_index_ = 0;
    int focused_notification_entry_ = 0;
    int notifications_page_ = 1;
    std::string notifications_state_ = "all";

    std::string new_issue_title_;
    std::string new_issue_body_;
    std::string new_comment_body_;
    int comment_target_number_ = 0;
    bool comment_is_pr_ = false;

    enum class ViewMode {
        INPUT,
        TREE,
        FILE,
        COMMITS,
        COMMIT_DETAIL,
        ISSUES,
        ISSUE_DETAIL,
        PULL_REQUESTS,
        PR_DETAIL,
        PR_COMMITS,
        NOTIFICATIONS,
        CREATE_ISSUE,
        ADD_COMMENT
    };
    ViewMode view_mode_ = ViewMode::INPUT;
    ViewMode return_from_commit_ = ViewMode::COMMITS;
    ViewMode return_from_issue_detail_ = ViewMode::ISSUES;
    ViewMode return_from_pr_detail_ = ViewMode::PULL_REQUESTS;

    ftxui::Component make_main_component();

    void load_repository();
    void load_directory(const std::string& path);
    void navigate_into(const std::string& dir_name);
    void navigate_up();
    void load_file(const std::string& path);
    void set_status(const std::string& message);
    std::string format_size(size_t bytes) const;

    void load_commits(const std::string& path = "");
    void load_commit_detail(const std::string& sha);
    std::string format_commit_date(const std::string& iso_date) const;
    void save_file_locally();
    void save_commit_patch();

    void load_issues();
    void load_issue_detail(int number);
    void load_pull_requests();
    void load_pr_detail(int number);
    void load_notifications();
    void open_notification(const Notification& notif);
    void open_in_browser(const std::string& url);
    void copy_to_clipboard(const std::string& text);
    std::string get_github_url() const;

    bool check_highlight_available();
    std::string strip_ansi_codes(const std::string& text);
    std::string highlight_file(const std::string& content, const std::string& filename);
    ftxui::Element parse_ansi_line(const std::string& line_with_ansi);
    ftxui::Color ansi_code_to_color(int code);

    // Helper functions to reduce code duplication
    bool handle_scrolling(ftxui::Event& event, int& scroll_position, int max_scroll);
    bool check_double_click_or_enter(ftxui::Event& event);
    std::string capitalize_first(const std::string& str) const;
    std::string format_commit_display(const Commit& commit) const;
};
