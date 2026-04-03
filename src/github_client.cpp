#include "github_client.h"
#include <cpr/cpr.h>
#include <sstream>
#include <algorithm>
#include <iostream>

// Helper functions to reduce code duplication
static cpr::Header build_headers(const std::string& token, bool include_content_type = false) {
    cpr::Header headers{
        {"Accept", "application/vnd.github.v3+json"},
        {"User-Agent", "github-tui"}
    };

    if (include_content_type) {
        headers["Content-Type"] = "application/json";
    }

    if (!token.empty()) {
        headers["Authorization"] = "token " + token;
    }

    return headers;
}

static bool handle_error_response(const cpr::Response& response, std::string& last_error) {
    std::ostringstream oss;
    oss << "HTTP " << response.status_code;
    if (!response.text.empty()) {
        try {
            auto error_json = json::parse(response.text);
            if (error_json.contains("message")) {
                oss << ": " << error_json["message"].get<std::string>();
            }
        } catch (...) {
            oss << ": " << response.text;
        }
    }
    last_error = oss.str();
    return false;
}

GitHubClient::GitHubClient(const std::string& token) : token_(token) {}

std::optional<json> GitHubClient::make_request(const std::string& endpoint) {
    auto response = cpr::Get(
        cpr::Url{std::string(API_BASE) + endpoint},
        build_headers(token_)
    );

    if (response.status_code != 200) {
        handle_error_response(response, last_error_);
        return std::nullopt;
    }

    try {
        return json::parse(response.text);
    } catch (const json::exception& e) {
        last_error_ = std::string("JSON parse error: ") + e.what();
        return std::nullopt;
    }
}

std::optional<json> GitHubClient::make_post_request(const std::string& endpoint, const json& data) {
    auto response = cpr::Post(
        cpr::Url{std::string(API_BASE) + endpoint},
        build_headers(token_, true),
        cpr::Body{data.dump()}
    );

    if (response.status_code != 200 && response.status_code != 201) {
        handle_error_response(response, last_error_);
        return std::nullopt;
    }

    try {
        return json::parse(response.text);
    } catch (const json::exception& e) {
        last_error_ = std::string("JSON parse error: ") + e.what();
        return std::nullopt;
    }
}

std::optional<json> GitHubClient::make_patch_request(const std::string& endpoint, const json& data) {
    auto response = cpr::Patch(
        cpr::Url{std::string(API_BASE) + endpoint},
        build_headers(token_, true),
        cpr::Body{data.dump()}
    );

    if (response.status_code != 200) {
        handle_error_response(response, last_error_);
        return std::nullopt;
    }

    try {
        return json::parse(response.text);
    } catch (const json::exception& e) {
        last_error_ = std::string("JSON parse error: ") + e.what();
        return std::nullopt;
    }
}

std::optional<Repository> GitHubClient::get_repository(const std::string& owner, const std::string& repo) {
    auto result = make_request("/repos/" + owner + "/" + repo);
    if (!result) {
        return std::nullopt;
    }

    Repository repository;
    repository.name = (*result)["name"].get<std::string>();
    repository.full_name = (*result)["full_name"].get<std::string>();
    repository.description = (*result)["description"].is_null() ? "" : (*result)["description"].get<std::string>();
    repository.default_branch = (*result)["default_branch"].get<std::string>();
    repository.stargazers_count = (*result)["stargazers_count"].get<int>();
    repository.forks_count = (*result)["forks_count"].get<int>();
    repository.is_private = (*result)["private"].get<bool>();

    return repository;
}

std::optional<std::vector<TreeItem>> GitHubClient::get_tree(const std::string& owner, const std::string& repo, const std::string& ref) {
    auto result = make_request("/repos/" + owner + "/" + repo + "/git/trees/" + ref + "?recursive=1");
    if (!result) {
        return std::nullopt;
    }

    std::vector<TreeItem> items;
    for (const auto& item : (*result)["tree"]) {
        TreeItem tree_item;
        tree_item.path = item["path"].get<std::string>();
        tree_item.type = item["type"].get<std::string>();
        tree_item.sha = item["sha"].get<std::string>();
        tree_item.size = item.contains("size") ? item["size"].get<size_t>() : 0;
        items.push_back(tree_item);
    }

    return items;
}

std::optional<std::vector<TreeItem>> GitHubClient::get_directory_contents(const std::string& owner, const std::string& repo, const std::string& path, const std::string& ref) {
    std::string endpoint = "/repos/" + owner + "/" + repo + "/contents";
    if (!path.empty()) {
        endpoint += "/" + path;
    }
    endpoint += "?ref=" + ref;

    auto result = make_request(endpoint);
    if (!result) {
        return std::nullopt;
    }

    if (!result->is_array()) {
        last_error_ = "Path is not a directory";
        return std::nullopt;
    }

    std::vector<TreeItem> items;
    for (const auto& item : *result) {
        TreeItem tree_item;
        tree_item.path = item["name"].get<std::string>();
        tree_item.type = item["type"].get<std::string>();
        tree_item.sha = item["sha"].get<std::string>();
        tree_item.size = item.contains("size") ? item["size"].get<size_t>() : 0;
        items.push_back(tree_item);
    }

    std::sort(items.begin(), items.end(), [](const TreeItem& a, const TreeItem& b) {
        bool a_is_dir = (a.type == "dir");
        bool b_is_dir = (b.type == "dir");
        if (a_is_dir != b_is_dir) return a_is_dir > b_is_dir;
        return a.path < b.path;
    });

    return items;
}

std::optional<std::string> GitHubClient::get_file_content(const std::string& owner, const std::string& repo, const std::string& path, const std::string& ref) {
    auto result = make_request("/repos/" + owner + "/" + repo + "/contents/" + path + "?ref=" + ref);
    if (!result) {
        return std::nullopt;
    }

    if ((*result)["type"].get<std::string>() != "file") {
        last_error_ = "Not a file";
        return std::nullopt;
    }

    std::string encoding = (*result)["encoding"].get<std::string>();
    if (encoding != "base64") {
        last_error_ = "Unsupported encoding: " + encoding;
        return std::nullopt;
    }

    std::string content = (*result)["content"].get<std::string>();

    // Decode base64
    std::string decoded;
    const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string cleaned;
    for (char c : content) {
        if (c != '\n' && c != '\r' && c != ' ') {
            cleaned += c;
        }
    }

    for (size_t i = 0; i < cleaned.length(); i += 4) {
        uint32_t val = 0;
        int padding = 0;

        for (int j = 0; j < 4; ++j) {
            val <<= 6;
            if (i + j < cleaned.length()) {
                char c = cleaned[i + j];
                if (c == '=') {
                    padding++;
                } else {
                    size_t pos = base64_chars.find(c);
                    if (pos != std::string::npos) {
                        val |= pos;
                    }
                }
            }
        }

        for (int j = 2; j >= padding; --j) {
            decoded += static_cast<char>((val >> (j * 8)) & 0xFF);
        }
    }

    return decoded;
}

std::optional<std::vector<Commit>> GitHubClient::get_commits(const std::string& owner, const std::string& repo, const std::string& path, const std::string& sha, int page, int per_page) {
    std::string endpoint = "/repos/" + owner + "/" + repo + "/commits";
    std::string query;

    if (!sha.empty()) {
        query += (query.empty() ? "?" : "&") + std::string("sha=") + sha;
    }
    if (!path.empty()) {
        query += (query.empty() ? "?" : "&") + std::string("path=") + path;
    }

    // Add pagination parameters
    query += (query.empty() ? "?" : "&") + std::string("page=") + std::to_string(page);
    query += "&per_page=" + std::to_string(per_page);

    auto result = make_request(endpoint + query);
    if (!result) {
        return std::nullopt;
    }

    if (!result->is_array()) {
        last_error_ = "Expected array of commits";
        return std::nullopt;
    }

    std::vector<Commit> commits;
    for (const auto& item : *result) {
        Commit commit;
        commit.sha = item["sha"].get<std::string>();

        const auto& commit_obj = item["commit"];
        commit.message = commit_obj["message"].get<std::string>();

        const auto& author_obj = commit_obj["author"];
        commit.author.name = author_obj["name"].get<std::string>();
        commit.author.email = author_obj["email"].get<std::string>();
        commit.author.date = author_obj["date"].get<std::string>();

        const auto& committer_obj = commit_obj["committer"];
        commit.committer.name = committer_obj["name"].get<std::string>();
        commit.committer.email = committer_obj["email"].get<std::string>();
        commit.committer.date = committer_obj["date"].get<std::string>();

        commits.push_back(commit);
    }

    return commits;
}

std::optional<std::pair<Commit, std::vector<CommitFile>>> GitHubClient::get_commit_details(const std::string& owner, const std::string& repo, const std::string& sha) {
    auto result = make_request("/repos/" + owner + "/" + repo + "/commits/" + sha);
    if (!result) {
        return std::nullopt;
    }

    Commit commit;
    commit.sha = (*result)["sha"].get<std::string>();

    const auto& commit_obj = (*result)["commit"];
    commit.message = commit_obj["message"].get<std::string>();

    const auto& author_obj = commit_obj["author"];
    commit.author.name = author_obj["name"].get<std::string>();
    commit.author.email = author_obj["email"].get<std::string>();
    commit.author.date = author_obj["date"].get<std::string>();

    const auto& committer_obj = commit_obj["committer"];
    commit.committer.name = committer_obj["name"].get<std::string>();
    commit.committer.email = committer_obj["email"].get<std::string>();
    commit.committer.date = committer_obj["date"].get<std::string>();

    std::vector<CommitFile> files;
    if (result->contains("files")) {
        for (const auto& file_obj : (*result)["files"]) {
            CommitFile file;
            file.filename = file_obj["filename"].get<std::string>();
            file.status = file_obj["status"].get<std::string>();
            file.additions = file_obj["additions"].get<int>();
            file.deletions = file_obj["deletions"].get<int>();
            file.changes = file_obj["changes"].get<int>();
            files.push_back(file);
        }
    }

    return std::make_pair(commit, files);
}

std::optional<std::vector<Issue>> GitHubClient::get_issues(const std::string& owner, const std::string& repo, const std::string& state, int page, int per_page) {
    std::string endpoint = "/repos/" + owner + "/" + repo + "/issues?state=" + state +
                           "&page=" + std::to_string(page) + "&per_page=" + std::to_string(per_page);
    auto result = make_request(endpoint);
    if (!result) {
        return std::nullopt;
    }

    if (!result->is_array()) {
        last_error_ = "Expected array of issues";
        return std::nullopt;
    }

    std::vector<Issue> issues;
    for (const auto& item : *result) {
        // Skip pull requests (they appear in issues endpoint too)
        if (item.contains("pull_request")) {
            continue;
        }

        Issue issue;
        issue.number = item["number"].get<int>();
        issue.title = item["title"].get<std::string>();
        issue.body = item["body"].is_null() ? "" : item["body"].get<std::string>();
        issue.state = item["state"].get<std::string>();
        issue.created_at = item["created_at"].get<std::string>();
        issue.updated_at = item["updated_at"].get<std::string>();
        issue.comments_count = item["comments"].get<int>();

        issue.user.login = item["user"]["login"].get<std::string>();
        issue.user.avatar_url = item["user"]["avatar_url"].get<std::string>();

        if (item.contains("labels") && item["labels"].is_array()) {
            for (const auto& label_obj : item["labels"]) {
                Label label;
                label.name = label_obj["name"].get<std::string>();
                label.color = label_obj["color"].get<std::string>();
                issue.labels.push_back(label);
            }
        }

        issues.push_back(issue);
    }

    return issues;
}

std::optional<Issue> GitHubClient::get_issue(const std::string& owner, const std::string& repo, int number) {
    auto result = make_request("/repos/" + owner + "/" + repo + "/issues/" + std::to_string(number));
    if (!result) {
        return std::nullopt;
    }

    Issue issue;
    issue.number = (*result)["number"].get<int>();
    issue.title = (*result)["title"].get<std::string>();
    issue.body = (*result)["body"].is_null() ? "" : (*result)["body"].get<std::string>();
    issue.state = (*result)["state"].get<std::string>();
    issue.created_at = (*result)["created_at"].get<std::string>();
    issue.updated_at = (*result)["updated_at"].get<std::string>();
    issue.comments_count = (*result)["comments"].get<int>();

    issue.user.login = (*result)["user"]["login"].get<std::string>();
    issue.user.avatar_url = (*result)["user"]["avatar_url"].get<std::string>();

    if (result->contains("labels") && (*result)["labels"].is_array()) {
        for (const auto& label_obj : (*result)["labels"]) {
            Label label;
            label.name = label_obj["name"].get<std::string>();
            label.color = label_obj["color"].get<std::string>();
            issue.labels.push_back(label);
        }
    }

    return issue;
}

std::optional<std::vector<Comment>> GitHubClient::get_issue_comments(const std::string& owner, const std::string& repo, int number) {
    auto result = make_request("/repos/" + owner + "/" + repo + "/issues/" + std::to_string(number) + "/comments");
    if (!result) {
        return std::nullopt;
    }

    if (!result->is_array()) {
        last_error_ = "Expected array of comments";
        return std::nullopt;
    }

    std::vector<Comment> comments;
    for (const auto& item : *result) {
        Comment comment;
        comment.id = item["id"].get<int>();
        comment.body = item["body"].get<std::string>();
        comment.created_at = item["created_at"].get<std::string>();
        comment.updated_at = item["updated_at"].get<std::string>();
        comment.user.login = item["user"]["login"].get<std::string>();
        comment.user.avatar_url = item["user"]["avatar_url"].get<std::string>();
        comments.push_back(comment);
    }

    return comments;
}

std::optional<Issue> GitHubClient::create_issue(const std::string& owner, const std::string& repo, const std::string& title, const std::string& body) {
    json data = {
        {"title", title},
        {"body", body}
    };

    auto result = make_post_request("/repos/" + owner + "/" + repo + "/issues", data);
    if (!result) {
        return std::nullopt;
    }

    Issue issue;
    issue.number = (*result)["number"].get<int>();
    issue.title = (*result)["title"].get<std::string>();
    issue.body = (*result)["body"].is_null() ? "" : (*result)["body"].get<std::string>();
    issue.state = (*result)["state"].get<std::string>();
    issue.created_at = (*result)["created_at"].get<std::string>();
    issue.updated_at = (*result)["updated_at"].get<std::string>();
    issue.comments_count = (*result)["comments"].get<int>();

    issue.user.login = (*result)["user"]["login"].get<std::string>();
    issue.user.avatar_url = (*result)["user"]["avatar_url"].get<std::string>();

    return issue;
}

std::optional<Comment> GitHubClient::create_issue_comment(const std::string& owner, const std::string& repo, int number, const std::string& body) {
    json data = {
        {"body", body}
    };

    auto result = make_post_request("/repos/" + owner + "/" + repo + "/issues/" + std::to_string(number) + "/comments", data);
    if (!result) {
        return std::nullopt;
    }

    Comment comment;
    comment.id = (*result)["id"].get<int>();
    comment.body = (*result)["body"].get<std::string>();
    comment.created_at = (*result)["created_at"].get<std::string>();
    comment.updated_at = (*result)["updated_at"].get<std::string>();
    comment.user.login = (*result)["user"]["login"].get<std::string>();
    comment.user.avatar_url = (*result)["user"]["avatar_url"].get<std::string>();

    return comment;
}

bool GitHubClient::update_issue_state(const std::string& owner, const std::string& repo, int number, const std::string& state) {
    json data = {
        {"state", state}
    };

    auto result = make_patch_request("/repos/" + owner + "/" + repo + "/issues/" + std::to_string(number), data);
    return result.has_value();
}

std::optional<std::vector<PullRequest>> GitHubClient::get_pull_requests(const std::string& owner, const std::string& repo, const std::string& state, int page, int per_page) {
    std::string endpoint = "/repos/" + owner + "/" + repo + "/pulls?state=" + state +
                           "&page=" + std::to_string(page) + "&per_page=" + std::to_string(per_page);
    auto result = make_request(endpoint);
    if (!result) {
        return std::nullopt;
    }

    if (!result->is_array()) {
        last_error_ = "Expected array of pull requests";
        return std::nullopt;
    }

    std::vector<PullRequest> prs;
    for (const auto& item : *result) {
        try {
            PullRequest pr;
            pr.number = item["number"].get<int>();
            pr.title = item["title"].get<std::string>();
            pr.body = item["body"].is_null() ? "" : item["body"].get<std::string>();
            pr.state = item["state"].get<std::string>();
            pr.created_at = item["created_at"].get<std::string>();
            pr.updated_at = item["updated_at"].get<std::string>();

            // Handle comments field (might be missing)
            if (item.contains("comments")) {
                pr.comments_count = item["comments"].get<int>();
            } else {
                pr.comments_count = 0;
            }

            // Handle merged field (might be missing in list view)
            if (item.contains("merged") && !item["merged"].is_null()) {
                pr.merged = item["merged"].get<bool>();
            } else {
                pr.merged = false;
            }

            if (item.contains("merged_at") && !item["merged_at"].is_null()) {
                pr.merged_at = item["merged_at"].get<std::string>();
            } else {
                pr.merged_at = "";
            }

            // Handle potentially null head/base refs (deleted forks)
            if (item.contains("head") && !item["head"].is_null() && item["head"].contains("ref")) {
                pr.head_ref = item["head"]["ref"].get<std::string>();
            } else {
                pr.head_ref = "unknown";
            }

            if (item.contains("base") && !item["base"].is_null() && item["base"].contains("ref")) {
                pr.base_ref = item["base"]["ref"].get<std::string>();
            } else {
                pr.base_ref = "unknown";
            }

            pr.user.login = item["user"]["login"].get<std::string>();
            pr.user.avatar_url = item["user"]["avatar_url"].get<std::string>();

            if (item.contains("labels") && item["labels"].is_array()) {
                for (const auto& label_obj : item["labels"]) {
                    Label label;
                    label.name = label_obj["name"].get<std::string>();
                    label.color = label_obj["color"].get<std::string>();
                    pr.labels.push_back(label);
                }
            }

            prs.push_back(pr);
        } catch (const json::exception& e) {
            // Log error but continue with other PRs
            std::cerr << "Error parsing PR: " << e.what() << std::endl;
            continue;
        } catch (...) {
            // Skip this PR if there's an unknown error
            continue;
        }
    }

    return prs;
}

std::optional<PullRequest> GitHubClient::get_pull_request(const std::string& owner, const std::string& repo, int number) {
    auto result = make_request("/repos/" + owner + "/" + repo + "/pulls/" + std::to_string(number));
    if (!result) {
        return std::nullopt;
    }

    try {
        PullRequest pr;
    pr.number = (*result)["number"].get<int>();
    pr.title = (*result)["title"].get<std::string>();
    pr.body = (*result)["body"].is_null() ? "" : (*result)["body"].get<std::string>();
    pr.state = (*result)["state"].get<std::string>();
    pr.created_at = (*result)["created_at"].get<std::string>();
    pr.updated_at = (*result)["updated_at"].get<std::string>();
    pr.comments_count = (*result)["comments"].get<int>();
    pr.merged = (*result)["merged"].get<bool>();
    pr.merged_at = (*result)["merged_at"].is_null() ? "" : (*result)["merged_at"].get<std::string>();

    // Handle potentially null head/base refs (deleted forks)
    if (result->contains("head") && !(*result)["head"].is_null() && (*result)["head"].contains("ref")) {
        pr.head_ref = (*result)["head"]["ref"].get<std::string>();
    } else {
        pr.head_ref = "unknown";
    }

    if (result->contains("base") && !(*result)["base"].is_null() && (*result)["base"].contains("ref")) {
        pr.base_ref = (*result)["base"]["ref"].get<std::string>();
    } else {
        pr.base_ref = "unknown";
    }

    pr.user.login = (*result)["user"]["login"].get<std::string>();
    pr.user.avatar_url = (*result)["user"]["avatar_url"].get<std::string>();

        if (result->contains("labels") && (*result)["labels"].is_array()) {
            for (const auto& label_obj : (*result)["labels"]) {
                Label label;
                label.name = label_obj["name"].get<std::string>();
                label.color = label_obj["color"].get<std::string>();
                pr.labels.push_back(label);
            }
        }

        return pr;
    } catch (const json::exception& e) {
        last_error_ = std::string("Error parsing PR: ") + e.what();
        return std::nullopt;
    } catch (...) {
        last_error_ = "Unknown error parsing PR";
        return std::nullopt;
    }
}

std::optional<std::vector<Comment>> GitHubClient::get_pull_request_comments(const std::string& owner, const std::string& repo, int number) {
    auto result = make_request("/repos/" + owner + "/" + repo + "/issues/" + std::to_string(number) + "/comments");
    if (!result) {
        return std::nullopt;
    }

    if (!result->is_array()) {
        last_error_ = "Expected array of comments";
        return std::nullopt;
    }

    std::vector<Comment> comments;
    for (const auto& item : *result) {
        Comment comment;
        comment.id = item["id"].get<int>();
        comment.body = item["body"].get<std::string>();
        comment.created_at = item["created_at"].get<std::string>();
        comment.updated_at = item["updated_at"].get<std::string>();
        comment.user.login = item["user"]["login"].get<std::string>();
        comment.user.avatar_url = item["user"]["avatar_url"].get<std::string>();
        comments.push_back(comment);
    }

    return comments;
}

std::optional<std::vector<CommitFile>> GitHubClient::get_pull_request_files(const std::string& owner, const std::string& repo, int number) {
    auto result = make_request("/repos/" + owner + "/" + repo + "/pulls/" + std::to_string(number) + "/files");
    if (!result) {
        return std::nullopt;
    }

    if (!result->is_array()) {
        last_error_ = "Expected array of files";
        return std::nullopt;
    }

    std::vector<CommitFile> files;
    for (const auto& item : *result) {
        CommitFile file;
        file.filename = item["filename"].get<std::string>();
        file.status = item["status"].get<std::string>();
        file.additions = item["additions"].get<int>();
        file.deletions = item["deletions"].get<int>();
        file.changes = item["changes"].get<int>();
        files.push_back(file);
    }

    return files;
}

std::optional<Comment> GitHubClient::create_pull_request_comment(const std::string& owner, const std::string& repo, int number, const std::string& body) {
    json data = {
        {"body", body}
    };

    auto result = make_post_request("/repos/" + owner + "/" + repo + "/issues/" + std::to_string(number) + "/comments", data);
    if (!result) {
        return std::nullopt;
    }

    Comment comment;
    comment.id = (*result)["id"].get<int>();
    comment.body = (*result)["body"].get<std::string>();
    comment.created_at = (*result)["created_at"].get<std::string>();
    comment.updated_at = (*result)["updated_at"].get<std::string>();
    comment.user.login = (*result)["user"]["login"].get<std::string>();
    comment.user.avatar_url = (*result)["user"]["avatar_url"].get<std::string>();

    return comment;
}

std::optional<std::vector<Commit>> GitHubClient::get_pull_request_commits(const std::string& owner, const std::string& repo, int number) {
    auto result = make_request("/repos/" + owner + "/" + repo + "/pulls/" + std::to_string(number) + "/commits");
    if (!result) {
        return std::nullopt;
    }

    if (!result->is_array()) {
        last_error_ = "Expected array of commits";
        return std::nullopt;
    }

    std::vector<Commit> commits;
    for (const auto& item : *result) {
        Commit commit;
        commit.sha = item["sha"].get<std::string>();

        const auto& commit_obj = item["commit"];
        commit.message = commit_obj["message"].get<std::string>();

        const auto& author_obj = commit_obj["author"];
        commit.author.name = author_obj["name"].get<std::string>();
        commit.author.email = author_obj["email"].get<std::string>();
        commit.author.date = author_obj["date"].get<std::string>();

        const auto& committer_obj = commit_obj["committer"];
        commit.committer.name = committer_obj["name"].get<std::string>();
        commit.committer.email = committer_obj["email"].get<std::string>();
        commit.committer.date = committer_obj["date"].get<std::string>();

        commits.push_back(commit);
    }

    return commits;
}

bool GitHubClient::update_pull_request_state(const std::string& owner, const std::string& repo, int number, const std::string& state) {
    json data = {
        {"state", state}
    };

    auto result = make_patch_request("/repos/" + owner + "/" + repo + "/pulls/" + std::to_string(number), data);
    return result.has_value();
}

std::optional<std::string> GitHubClient::get_commit_patch(const std::string& owner, const std::string& repo, const std::string& sha) {
    cpr::Header headers{
        {"Accept", "application/vnd.github.v3.diff"},
        {"User-Agent", "github-tui"}
    };

    if (!token_.empty()) {
        headers["Authorization"] = "token " + token_;
    }

    auto response = cpr::Get(
        cpr::Url{std::string(API_BASE) + "/repos/" + owner + "/" + repo + "/commits/" + sha},
        headers
    );

    if (response.status_code != 200) {
        std::ostringstream oss;
        oss << "HTTP " << response.status_code;
        if (!response.text.empty()) {
            oss << ": " << response.text;
        }
        last_error_ = oss.str();
        return std::nullopt;
    }

    return response.text;
}

std::vector<DiffFile> GitHubClient::parse_diff(const std::string& patch) {
    std::vector<DiffFile> diff_files;

    if (patch.empty()) {
        return diff_files;
    }

    std::istringstream stream(patch);
    std::string line;
    DiffFile* current_file = nullptr;
    bool in_hunk = false;
    DiffHunk current_hunk;

    while (std::getline(stream, line)) {
        // New file
        if (line.find("diff --git") == 0) {
            // Save current hunk if exists
            if (in_hunk && current_file) {
                current_file->hunks.push_back(current_hunk);
                in_hunk = false;
            }

            diff_files.push_back(DiffFile());
            current_file = &diff_files.back();

            // Extract filename from "diff --git a/file b/file"
            size_t b_pos = line.rfind(" b/");
            if (b_pos != std::string::npos) {
                current_file->filename = line.substr(b_pos + 3);
            }
        }
        // File metadata (skip these)
        else if (line.find("index ") == 0 ||
                 line.find("--- ") == 0 ||
                 line.find("+++ ") == 0 ||
                 line.find("new file mode") == 0 ||
                 line.find("deleted file mode") == 0) {
            continue;
        }
        // Hunk header: @@ -start,count +start,count @@
        else if (line.find("@@") == 0 && current_file) {
            // Save previous hunk if exists
            if (in_hunk) {
                current_file->hunks.push_back(current_hunk);
            }

            // Start new hunk
            current_hunk = DiffHunk();
            current_hunk.lines.clear();
            in_hunk = true;

            // Parse @@ -(\d+)(?:,\d+)? \+(\d+)(?:,\d+)? @@
            size_t minus_pos = line.find('-');
            size_t plus_pos = line.find('+', minus_pos);
            size_t end_pos = line.find("@@", 2);

            if (minus_pos != std::string::npos && plus_pos != std::string::npos) {
                std::string left_part = line.substr(minus_pos + 1, plus_pos - minus_pos - 2);
                std::string right_part = line.substr(plus_pos + 1, end_pos - plus_pos - 2);

                // Extract just the start line number (before comma if present)
                size_t comma = left_part.find(',');
                if (comma != std::string::npos) {
                    left_part = left_part.substr(0, comma);
                }
                comma = right_part.find(',');
                if (comma != std::string::npos) {
                    right_part = right_part.substr(0, comma);
                }

                try {
                    current_hunk.left_start = std::stoi(left_part);
                    current_hunk.right_start = std::stoi(right_part);
                } catch (...) {
                    current_hunk.left_start = 0;
                    current_hunk.right_start = 0;
                }
            }
        }
        // Diff content lines
        else if (in_hunk && !line.empty()) {
            DiffLine diff_line;

            if (line[0] == '+') {
                diff_line.type = DiffLine::Add;
                diff_line.content = line;
            } else if (line[0] == '-') {
                diff_line.type = DiffLine::Delete;
                diff_line.content = line;
            } else if (line[0] == ' ') {
                diff_line.type = DiffLine::Keep;
                diff_line.content = line;
            } else {
                // Other metadata or binary file markers
                diff_line.type = DiffLine::Keep;
                diff_line.content = line;
            }

            current_hunk.lines.push_back(diff_line);
        }
    }

    // Add the last hunk if exists
    if (in_hunk && current_file) {
        current_file->hunks.push_back(current_hunk);
    }

    return diff_files;
}

std::optional<std::vector<Notification>> GitHubClient::get_notifications(const std::string& filter, int page, int per_page) {
    std::string endpoint = "/notifications";
    std::string query = "?page=" + std::to_string(page) + "&per_page=" + std::to_string(per_page);

    if (filter == "all") {
        query += "&all=true";
    } else if (filter == "participating") {
        query += "&participating=true";
    }
    // "unread" is the default, no extra parameter needed

    auto result = make_request(endpoint + query);
    if (!result) {
        return std::nullopt;
    }

    if (!result->is_array()) {
        last_error_ = "Expected array of notifications";
        return std::nullopt;
    }

    std::vector<Notification> notifications;
    for (const auto& item : *result) {
        Notification notif;
        notif.id = item["id"].get<std::string>();
        notif.unread = item["unread"].get<bool>();
        notif.reason = item["reason"].get<std::string>();
        notif.updated_at = item["updated_at"].get<std::string>();

        notif.repository.full_name = item["repository"]["full_name"].get<std::string>();
        notif.repository.owner = item["repository"]["owner"]["login"].get<std::string>();
        notif.repository.name = item["repository"]["name"].get<std::string>();

        notif.subject.title = item["subject"]["title"].get<std::string>();
        notif.subject.type = item["subject"]["type"].get<std::string>();
        notif.subject.url = item["subject"]["url"].is_null() ? "" : item["subject"]["url"].get<std::string>();

        notifications.push_back(notif);
    }

    return notifications;
}

bool GitHubClient::mark_notification_read(const std::string& thread_id) {
    json data = {};  // Empty body
    auto result = make_patch_request("/notifications/threads/" + thread_id, data);
    return result.has_value();
}
