#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Repository {
    std::string name;
    std::string full_name;
    std::string description;
    std::string default_branch;
    int stargazers_count;
    int forks_count;
    bool is_private;
};

struct TreeItem {
    std::string path;
    std::string type;
    std::string sha;
    size_t size;
};

struct CommitAuthor {
    std::string name;
    std::string email;
    std::string date;
};

struct Commit {
    std::string sha;
    std::string message;
    CommitAuthor author;
    CommitAuthor committer;
};

struct CommitFile {
    std::string filename;
    std::string status;
    int additions;
    int deletions;
    int changes;
};

struct DiffLine {
    enum Type { Keep, Add, Delete };
    Type type;
    std::string content;
};

struct DiffHunk {
    int left_start;
    int right_start;
    std::vector<DiffLine> lines;
};

struct DiffFile {
    std::string filename;
    std::vector<DiffHunk> hunks;
};

struct User {
    std::string login;
    std::string avatar_url;
};

struct Comment {
    int id;
    User user;
    std::string body;
    std::string created_at;
    std::string updated_at;
};

struct Label {
    std::string name;
    std::string color;
};

struct Issue {
    int number;
    std::string title;
    std::string body;
    std::string state;  // "open" or "closed"
    User user;
    std::string created_at;
    std::string updated_at;
    std::vector<Label> labels;
    int comments_count;
};

struct PullRequest {
    int number;
    std::string title;
    std::string body;
    std::string state;  // "open" or "closed"
    User user;
    std::string created_at;
    std::string updated_at;
    std::string head_ref;  // source branch
    std::string base_ref;  // target branch
    std::vector<Label> labels;
    int comments_count;
    bool merged;
    std::string merged_at;
};

struct NotificationRepository {
    std::string full_name;
    std::string owner;
    std::string name;
};

struct NotificationSubject {
    std::string title;
    std::string type;  // "Issue", "PullRequest", "Commit", "Release", etc.
    std::string url;   // API URL to the subject
};

struct Notification {
    std::string id;
    NotificationRepository repository;
    NotificationSubject subject;
    std::string reason;  // "subscribed", "mentioned", "review_requested", etc.
    bool unread;
    std::string updated_at;
};

class GitHubClient {
public:
    explicit GitHubClient(const std::string& token);

    std::optional<Repository> get_repository(const std::string& owner, const std::string& repo);
    std::optional<std::vector<TreeItem>> get_tree(const std::string& owner, const std::string& repo, const std::string& ref = "HEAD");
    std::optional<std::vector<TreeItem>> get_directory_contents(const std::string& owner, const std::string& repo, const std::string& path = "", const std::string& ref = "HEAD");
    std::optional<std::string> get_file_content(const std::string& owner, const std::string& repo, const std::string& path, const std::string& ref = "HEAD");

    // Commit history
    std::optional<std::vector<Commit>> get_commits(const std::string& owner, const std::string& repo, const std::string& path = "", const std::string& sha = "", int page = 1, int per_page = 30);
    std::optional<std::pair<Commit, std::vector<CommitFile>>> get_commit_details(const std::string& owner, const std::string& repo, const std::string& sha);
    std::optional<std::string> get_commit_patch(const std::string& owner, const std::string& repo, const std::string& sha);
    std::vector<DiffFile> parse_diff(const std::string& patch);

    // Issues
    std::optional<std::vector<Issue>> get_issues(const std::string& owner, const std::string& repo, const std::string& state = "open", int page = 1, int per_page = 30);
    std::optional<Issue> get_issue(const std::string& owner, const std::string& repo, int number);
    std::optional<std::vector<Comment>> get_issue_comments(const std::string& owner, const std::string& repo, int number);
    std::optional<Issue> create_issue(const std::string& owner, const std::string& repo, const std::string& title, const std::string& body);
    std::optional<Comment> create_issue_comment(const std::string& owner, const std::string& repo, int number, const std::string& body);
    bool update_issue_state(const std::string& owner, const std::string& repo, int number, const std::string& state);

    // Pull Requests
    std::optional<std::vector<PullRequest>> get_pull_requests(const std::string& owner, const std::string& repo, const std::string& state = "open", int page = 1, int per_page = 30);
    std::optional<PullRequest> get_pull_request(const std::string& owner, const std::string& repo, int number);
    std::optional<std::vector<Comment>> get_pull_request_comments(const std::string& owner, const std::string& repo, int number);
    std::optional<std::vector<CommitFile>> get_pull_request_files(const std::string& owner, const std::string& repo, int number);
    std::optional<std::vector<Commit>> get_pull_request_commits(const std::string& owner, const std::string& repo, int number);
    std::optional<Comment> create_pull_request_comment(const std::string& owner, const std::string& repo, int number, const std::string& body);
    bool update_pull_request_state(const std::string& owner, const std::string& repo, int number, const std::string& state);

    // Notifications
    std::optional<std::vector<Notification>> get_notifications(const std::string& filter = "all", int page = 1, int per_page = 30);
    bool mark_notification_read(const std::string& thread_id);

    std::string get_last_error() const { return last_error_; }

private:
    std::string token_;
    std::string last_error_;
    static constexpr const char* API_BASE = "https://api.github.com";

    std::optional<json> make_request(const std::string& endpoint);
    std::optional<json> make_post_request(const std::string& endpoint, const json& data);
    std::optional<json> make_patch_request(const std::string& endpoint, const json& data);
};
