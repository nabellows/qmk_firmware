#!/usr/bin/env bash
set -Eeuo pipefail

ROOT=$(git rev-parse --show-toplevel)
cd "$ROOT"

CURRENT_BRANCH=$(git branch --show-current)

SUCCESS=false
WT=

cleanup() {
    if [[ -z "${WT:-}" ]]; then
        return
    fi

    if $SUCCESS; then
        git worktree remove --force "$WT" >/dev/null 2>&1 || true
    else
        cat <<EOF

Sync failed.

The temporary worktree has been preserved:

    $WT

You can inspect it with:

    cd "$WT"
    git status

and either resolve the merge there or simply delete it.
EOF
    fi
}

trap cleanup EXIT

fetch_default_branch() {
    local remote=$1

    local branch
    branch=$(
        git ls-remote --symref "$remote" HEAD |
        awk '/^ref:/ {sub("^refs/heads/","",$2); print $2}'
    )

    git fetch --quiet "$remote" \
        "+refs/heads/$branch:refs/remotes/$remote/$branch"

    printf '%s\n' "$branch"
}

echo "Fetching remotes..."

UPSTREAM_BRANCH=$(fetch_default_branch upstream)
SRGB_BRANCH=$(fetch_default_branch signalrgb)

WT=$(mktemp -d)

echo "Creating worktree..."
git worktree add --quiet "$WT" HEAD

pushd "$WT" >/dev/null

echo "Merging upstream/$UPSTREAM_BRANCH..."

git merge \
    --no-ff \
    --no-commit \
    "upstream/$UPSTREAM_BRANCH"

echo "Importing SignalRGB..."

SRGB_DIR=quantum
SRGB_FILE_BASE="$SRGB_DIR/signalrgb"
SRGB_FIlES=(
    "$SRGB_FILE_BASE.c"
    "$SRGB_FILE_BASE.h"
    "$SRGB_FILE_BASE.h"
    #quantum/rgb_matrix/animations/signalrgb_anim.h
    "$SRGB_DIR/rgb_matrix/animations/signalrgb_anim.h"
    )

git restore \
    --source="signalrgb/$SRGB_BRANCH" \
    -- \
    "${SRGB_FIlES[@]}"

# Patch SignalRGB silliness
sed -i \
    's/\<via_command_kb\>/via_command_signalrgb/g' \
    "$SRGB_FILE_BASE.c"

#
# Only create a commit if anything actually changed.
#
if ! git diff --cached --quiet || ! git diff --quiet || [[ -n "$(git status --porcelain)" ]]; then
    git add "${SRGB_FIlES[@]}"
    git commit -m "Sync upstream and SignalRGB"
fi

NEW_COMMIT=$(git rev-parse HEAD)

git tag -f sync "$NEW_COMMIT"

popd >/dev/null

#
# If nothing changed at all, we're done.
#
if [[ "$NEW_COMMIT" == "$(git rev-parse HEAD)" ]]; then
    echo
    echo "Already up to date."
    SUCCESS=true
    exit 0
fi

#
# Don't touch a dirty working tree.
#
if ! git diff --quiet || ! git diff --cached --quiet; then
    cat <<EOF

Sync succeeded, but your working tree has local changes.

Prepared commit:

    tag 'sync':
        $NEW_COMMIT

Review it:

    git diff HEAD sync ($NEW_COMMIT)

When ready, update your branch manually:

    git merge [--ff-only] sync ($NEW_COMMIT)

EOF

    SUCCESS=true
    exit 0
fi

echo
echo "Fast-forwarding current branch..."

git merge --ff-only sync

SUCCESS=true

echo
echo "Done."
