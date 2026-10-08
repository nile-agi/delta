#!/usr/bin/env bash
#
# Copy published releases from the private source repository to the public release repository.
#
# For each tag: download every asset, create a draft with the same tag, title and prerelease flag
# (notes cleaned of links into the private repo), upload the assets, check each one's SHA-256
# against the original, and only then publish. Safe to rerun: a release already complete in the
# target is left alone, and a half-finished draft is resumed.
#
# Usage:
#   scripts/copy-releases.sh [--source owner/repo] [--target owner/repo] [--dry-run] TAG...
# Copy oldest first: the last release published becomes "latest".

set -euo pipefail

SOURCE="nile-agi/delta"
TARGET="nile-agi/delta-release"
DRY_RUN=0
TAGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --source) SOURCE="$2"; shift 2 ;;
        --target) TARGET="$2"; shift 2 ;;
        --dry-run) DRY_RUN=1; shift ;;
        -h|--help) sed -n '2,13p' "$0"; exit 0 ;;
        *) TAGS+=("$1"); shift ;;
    esac
done

if [[ ${#TAGS[@]} -eq 0 ]]; then
    echo "Name at least one tag, e.g. v1.0.10 v1.0.11 v1.0.13" >&2
    exit 2
fi

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# "name<TAB>sha256" for every asset of a release, sorted, from the digests GitHub records.
asset_digests() {
    gh api "repos/$1/releases/$2" --jq '.assets[] | "\(.name)\t\(.digest // "none")"' | sort
}

release_id() {
    gh api "repos/$1/releases" --paginate --jq ".[] | select(.tag_name == \"$2\") | .id" | head -n1
}

for TAG in "${TAGS[@]}"; do
    echo "== $TAG"
    SRC_ID=$(release_id "$SOURCE" "$TAG")
    if [[ -z "$SRC_ID" ]]; then
        echo "   no release $TAG in $SOURCE" >&2
        exit 1
    fi
    asset_digests "$SOURCE" "$SRC_ID" > "$WORK/source.txt"
    if grep -q $'\tnone$' "$WORK/source.txt"; then
        echo "   some source assets have no recorded digest; cannot verify the copy" >&2
        exit 1
    fi
    COUNT=$(wc -l < "$WORK/source.txt" | tr -d ' ')

    DST_ID=$(release_id "$TARGET" "$TAG")
    if [[ -n "$DST_ID" ]]; then
        asset_digests "$TARGET" "$DST_ID" > "$WORK/target.txt"
        DRAFT=$(gh api "repos/$TARGET/releases/$DST_ID" --jq .draft)
        if [[ "$DRAFT" == "false" ]] && diff -q "$WORK/source.txt" "$WORK/target.txt" >/dev/null; then
            echo "   already published in $TARGET with all $COUNT assets; skipping"
            continue
        fi
        if [[ "$DRAFT" == "false" ]]; then
            echo "   $TARGET has a published $TAG whose assets differ; fix it by hand" >&2
            exit 1
        fi
    fi

    if [[ $DRY_RUN -eq 1 ]]; then
        echo "   would copy $COUNT assets to $TARGET"
        continue
    fi

    gh api "repos/$SOURCE/releases/$SRC_ID" > "$WORK/release.json"
    TITLE=$(jq -r .name "$WORK/release.json")
    PRERELEASE=$(jq -r .prerelease "$WORK/release.json")
    PUBLISHED=$(jq -r '.published_at[0:10]' "$WORK/release.json")
    jq -r '.body // ""' "$WORK/release.json" > "$WORK/body.md"
    # shellcheck disable=SC2016  # the ${...} is a JavaScript template literal
    node -e '
        const { publicNotes } = require(process.argv[1]);
        const [owner, repo] = process.argv[2].split("/");
        const body = require("fs").readFileSync(process.argv[3], "utf8");
        process.stdout.write(`${publicNotes(body, { owner, repo })}\n\n_Originally published on ${process.argv[4]}._\n`);
    ' "$REPO_ROOT/scripts/release-notes.cjs" "$SOURCE" "$WORK/body.md" "$PUBLISHED" > "$WORK/notes.md"

    rm -rf "$WORK/assets" && mkdir "$WORK/assets"
    echo "   downloading $COUNT assets"
    gh release download "$TAG" -R "$SOURCE" -D "$WORK/assets"

    if [[ -z "$DST_ID" ]]; then
        echo "   creating draft in $TARGET"
        # Take the id from the create call itself: a new draft can take a moment to show up when
        # releases are listed, and every later step addresses the release by id.
        DST_ID=$(gh api "repos/$TARGET/releases" -X POST \
            -f tag_name="$TAG" -f target_commitish=main -f name="$TITLE" \
            -F body=@"$WORK/notes.md" -F draft=true -F prerelease="$PRERELEASE" --jq .id)
    fi

    echo "   uploading"
    gh release upload "$TAG" -R "$TARGET" --clobber "$WORK"/assets/*

    asset_digests "$TARGET" "$DST_ID" > "$WORK/target.txt"
    if ! diff -u "$WORK/source.txt" "$WORK/target.txt"; then
        echo "   assets in $TARGET do not match $SOURCE; left as a draft" >&2
        exit 1
    fi
    echo "   all $COUNT assets match by SHA-256; publishing"
    gh api "repos/$TARGET/releases/$DST_ID" -X PATCH -F draft=false >/dev/null
    rm -rf "$WORK/assets"
done
