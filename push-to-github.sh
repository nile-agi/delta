#!/bin/bash

# Script to push delta repository to GitHub
# This will prompt for your GitHub credentials

echo "=========================================="
echo "Pushing delta to https://github.com/nile-agi/delta.git"
echo "=========================================="
echo ""

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: Not in the delta repository root directory"
    exit 1
fi

# Check remote
echo "Current remote:"
git remote -v
echo ""

# Check if we have commits
if ! git rev-parse --verify HEAD >/dev/null 2>&1; then
    echo "Error: No commits found. Please commit your changes first."
    exit 1
fi

# Check branch
CURRENT_BRANCH=$(git branch --show-current)
echo "Current branch: $CURRENT_BRANCH"
echo ""

# Try to push
echo "Attempting to push..."
echo ""
echo "NOTE: If prompted for credentials:"
echo "  - Username: Your GitHub username"
echo "  - Password: Use a Personal Access Token (NOT your GitHub password)"
echo "    Get one at: https://github.com/settings/tokens"
echo "    Select 'repo' scope"
echo ""

git push -u origin "$CURRENT_BRANCH"

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Successfully pushed to GitHub!"
    echo "Repository: https://github.com/nile-agi/delta"
else
    echo ""
    echo "❌ Push failed. Common issues:"
    echo "  1. Authentication required - use Personal Access Token"
    echo "  2. Repository doesn't exist or you don't have write access"
    echo "  3. Network issues"
    echo ""
    echo "To create a Personal Access Token:"
    echo "  1. Go to: https://github.com/settings/tokens"
    echo "  2. Generate new token (classic)"
    echo "  3. Select 'repo' scope"
    echo "  4. Copy the token and use it as your password"
fi

