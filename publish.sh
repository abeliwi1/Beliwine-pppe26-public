#!/bin/sh
# publish.sh -- make pppe26-public match main's files exactly, without main's history.
#
# Each run adds one commit to the public repo whose tree is main's tree, with the
# previous public commit as its parent.  Main's own commits never leave this repo,
# so files deleted from main's history (solutions, rubrics, autograders) stay private.
#
#   ./publish.sh ["commit message"]
set -e

git fetch -q public

# Refuse to publish grading material that has landed in main's current files.
if git ls-tree -r --name-only main | grep -i -E 'rubric|solution|reference|autograder|gradescope'; then
    echo "publish.sh: refusing to publish -- the files above look like grading material" >&2
    exit 1
fi

tree=$(git rev-parse 'main^{tree}')
if [ "$tree" = "$(git rev-parse 'public/main^{tree}')" ]; then
    echo "publish.sh: pppe26-public already matches main"
    exit 0
fi

msg=${1:-"Sync with main $(git rev-parse --short main)"}
commit=$(git commit-tree "$tree" -p public/main -m "$msg")
git diff --stat public/main "$commit"
git push public "$commit:refs/heads/main"
