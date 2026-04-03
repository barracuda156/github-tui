# GitHub TUI

A terminal user interface for browsing GitHub repositories. Supports authentification,
browsing, commit history, issues, PRs, notifications and saving diffs from commits. Not all
features are supported currently.

Note, this is just a convenience tool to use on legacy platforms without Go and Rust.
Avoid it unless you know what you are doing. Implementation is done by Claude Code; it may
contain bugs and be insecure. There are no reasons to use this if you have better options
for your platform. I am just fed up of JS garbage on GitHub website which makes it unusable
in older browsers, and I have no time to attempt writing such a tool manually.
If you still decide to use this tool, it is at your own risk.

## Usage

### Quick Start (No Authentication)

For browsing public repositories:

```bash
github-tui
```

**Note**: Unauthenticated requests are limited to 60/hour.

### With Authentication (Recommended)

For private repositories and higher rate limits (5000/hour):

1. Generate a GitHub Personal Access Token at https://github.com/settings/tokens
   - Required scopes: `repo` (for private repos) or `public_repo` (for public only)

2. Save your token:
```bash
github-tui --token YOUR_TOKEN_HERE
```

3. Run the application:
```bash
./github-tui
```

### Using the Application

Enter owner and repository name, then press "Load Repository"

## Navigation

- **Input View**: Tab to navigate fields, Enter to load repository
- **Tree View**: ↑/↓ to navigate, Enter to view file, q/Esc to return
- **File View**: ↑/↓ to scroll, q/Esc to return to tree
- **Global**: Ctrl+C to exit

## Configuration

Token is stored in `~/.config/github-tui/config.toml` with mode 0600.
