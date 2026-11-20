"""Print release notes for a tag.

The notes are the matching section of CHANGELOG.md (or CHANGELOG) when that
section exists. Otherwise they are the annotated tag message, which for a
lightweight tag is the commit message the tag points at.
"""
from pathlib import Path
import re
import subprocess
import sys

TRAILERS = re.compile(
    r"^(Co-authored-by|Signed-off-by|Acked-by|Reviewed-by|Tested-by): ",
    re.IGNORECASE,
)


def version_of(tag):
    if tag.startswith("v") and len(tag) > 1:
        return tag[1:]
    return tag


def heading_is_version(title, version):
    title = title.strip()
    escaped = re.escape(version)
    linked = rf"^\[v?{escaped}\](?:\([^)]*\))?(?:\s|$)"
    plain = rf"^v?{escaped}(?:\s|$|:)"
    return re.match(linked, title) is not None or re.match(plain, title) is not None


def changelog_section(text, version):
    lines = text.splitlines()
    start = None
    level = None
    for index, line in enumerate(lines):
        match = re.match(r"^(#{1,6})\s+(.*?)\s*$", line)
        if match and heading_is_version(match.group(2), version):
            start = index + 1
            level = len(match.group(1))
            break
    if start is None:
        return None
    end = len(lines)
    for index in range(start, len(lines)):
        match = re.match(r"^(#{1,6})\s+", lines[index])
        if match and len(match.group(1)) <= level:
            end = index
            break
    body = strip_link_refs("\n".join(lines[start:end]))
    return body or None


def strip_link_refs(body):
    """Drop a Keep a Changelog footer of `[version]: url` lines."""
    lines = body.splitlines()
    while lines and re.match(r"^\[[^\]]+\]:\s+\S+\s*$", lines[-1].strip()):
        lines.pop()
        while lines and lines[-1].strip() == "":
            lines.pop()
    return "\n".join(lines).strip()


def read_changelog(root, version):
    for name in ("CHANGELOG.md", "CHANGELOG"):
        path = root / name
        if path.is_file():
            section = changelog_section(path.read_text(encoding="utf-8-sig"), version)
            if section:
                return section
    return None


def strip_trailers(message):
    lines = message.splitlines()
    end = len(lines)
    while end > 0 and lines[end - 1].strip() == "":
        end -= 1
    trailer_start = end
    while trailer_start > 0 and TRAILERS.match(lines[trailer_start - 1].strip()):
        trailer_start -= 1
    if trailer_start < end and (trailer_start == 0 or lines[trailer_start - 1].strip() == ""):
        lines = lines[:trailer_start]
    return "\n".join(lines).strip()


def tag_message(tag):
    result = subprocess.run(
        ["git", "for-each-ref", f"refs/tags/{tag}", "--format=%(contents)"],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)
    return strip_trailers(result.stdout)


def notes_for(tag, root=Path(".")):
    section = read_changelog(root, version_of(tag))
    if section:
        return section
    return tag_message(tag)


def main():
    if len(sys.argv) != 2 or not sys.argv[1].strip():
        sys.stderr.write("usage: release_notes.py <tag>\n")
        raise SystemExit(2)
    text = notes_for(sys.argv[1].strip())
    if text:
        sys.stdout.write(text)
        if not text.endswith("\n"):
            sys.stdout.write("\n")


if __name__ == "__main__":
    main()
