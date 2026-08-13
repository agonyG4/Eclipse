#!/usr/bin/env python3
"""Validate the constrained security policy used by Eclipse CI."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ACTION_REFERENCE = re.compile(
    r"^(?:[A-Za-z0-9_.-]+/)+[A-Za-z0-9_.-]+@[0-9a-fA-F]{40}$"
)
MAPPING_KEY = re.compile(r"^([A-Za-z_][A-Za-z0-9_-]*)\s*:\s*(.*)$")
USES_LINE = re.compile(r"^(?:-\s*)?uses\s*:\s*(.*)$")


def strip_comment(line: str) -> str:
    """Remove a YAML-style comment while retaining # characters in quotes."""

    single_quoted = False
    double_quoted = False
    escaped = False
    for index, character in enumerate(line):
        if double_quoted and escaped:
            escaped = False
            continue
        if double_quoted and character == "\\":
            escaped = True
            continue
        if not double_quoted and character == "'":
            single_quoted = not single_quoted
            continue
        if not single_quoted and character == '"':
            double_quoted = not double_quoted
            continue
        if character == "#" and not single_quoted and not double_quoted:
            if index == 0 or line[index - 1].isspace():
                return line[:index].rstrip()
    return line.rstrip()


def active_lines(document: str) -> list[tuple[int, int, str]]:
    result: list[tuple[int, int, str]] = []
    for line_number, raw_line in enumerate(document.splitlines(), start=1):
        line = strip_comment(raw_line)
        if not line.strip():
            continue
        indentation = len(line) - len(line.lstrip(" "))
        result.append((line_number, indentation, line.strip()))
    return result


def unquote(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in {'"', "'"}:
        return value[1:-1]
    return value


def top_level_key(content: str) -> tuple[str, str] | None:
    match = MAPPING_KEY.match(content)
    if match is None:
        return None
    return match.group(1), match.group(2).strip()


def violation(line_number: int | None, message: str) -> str:
    location = f"line {line_number}: " if line_number is not None else ""
    return location + message


def validate(document: str) -> list[str]:
    lines = active_lines(document)
    errors: list[str] = []

    top_permissions = [
        (line_number, value)
        for line_number, indentation, content in lines
        if indentation == 0
        for key, value in [top_level_key(content) or ("", "")]
        if key == "permissions"
    ]
    if len(top_permissions) != 1:
        errors.append(
            violation(
                None,
                "workflow must contain exactly one top-level permissions mapping",
            )
        )
    else:
        permissions_line, inline_value = top_permissions[0]
        if inline_value:
            errors.append(
                violation(
                    permissions_line,
                    "top-level permissions must be a mapping containing only contents: read",
                )
            )
        else:
            permission_entries: list[tuple[int, int, str, str]] = []
            permissions_index = next(
                index
                for index, (line_number, indentation, content) in enumerate(lines)
                if line_number == permissions_line
                and indentation == 0
                and (top_level_key(content) or ("", ""))[0] == "permissions"
            )
            for line_number, indentation, content in lines[permissions_index + 1 :]:
                if indentation == 0:
                    break
                match = MAPPING_KEY.match(content)
                if match is None:
                    errors.append(
                        violation(
                            line_number,
                            "top-level permissions contains malformed mapping content",
                        )
                    )
                    continue
                permission_entries.append(
                    (line_number, indentation, match.group(1), match.group(2).strip())
                )
            if len(permission_entries) != 1:
                errors.append(
                    violation(
                        permissions_line,
                        "top-level permissions must contain exactly contents: read",
                    )
                )
            else:
                entry_line, _, key, value = permission_entries[0]
                if key != "contents" or unquote(value) != "read":
                    errors.append(
                        violation(
                            entry_line,
                            "top-level permissions must contain exactly contents: read",
                        )
                    )
                if not value:
                    errors.append(
                        violation(entry_line, "contents permission must have value read")
                    )

    for line_number, indentation, content in lines:
        key_value = top_level_key(content)
        if indentation > 0 and key_value is not None and key_value[0] == "permissions":
            errors.append(violation(line_number, "job-level permissions overrides are forbidden"))

    on_index: int | None = None
    on_inline_value = ""
    for index, (_, indentation, content) in enumerate(lines):
        if indentation == 0:
            key_value = top_level_key(content)
            if key_value is not None and key_value[0] == "on":
                on_index = index
                on_inline_value = key_value[1]
                break
    if on_index is not None:
        event_lines: list[tuple[int, str]] = []
        if on_inline_value:
            event_lines.append((lines[on_index][0], on_inline_value))
        for line_number, indentation, content in lines[on_index + 1 :]:
            if indentation == 0:
                break
            event_lines.append((line_number, content))
        for line_number, content in event_lines:
            if re.search(r"(?:^|[\s,\[])[\"']?pull_request_target[\"']?(?:$|[\s,:\]}])", content):
                errors.append(violation(line_number, "pull_request_target event is forbidden"))

    for line_number, _, content in lines:
        uses_match = USES_LINE.match(content)
        if uses_match is not None:
            reference = unquote(uses_match.group(1).split()[0] if uses_match.group(1) else "")
            if reference.startswith("./"):
                continue
            if not ACTION_REFERENCE.fullmatch(reference):
                errors.append(
                    violation(
                        line_number,
                        "external action references must use a full 40-character immutable SHA",
                    )
                )
        if re.search(r"\bsecrets\.", content):
            errors.append(violation(line_number, "secret references are forbidden in this workflow"))

    return errors


def main(argv: list[str] | None = None) -> int:
    arguments = sys.argv[1:] if argv is None else argv
    if len(arguments) != 1:
        print("usage: check-workflow-policy.py WORKFLOW", file=sys.stderr)
        return 2
    workflow_path = Path(arguments[0])
    try:
        document = workflow_path.read_text(encoding="utf-8")
    except OSError as error:
        print(f"workflow policy violation: cannot read {workflow_path}: {error}", file=sys.stderr)
        return 1
    errors = validate(document)
    if errors:
        print("Workflow policy validation failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    print(f"Workflow policy valid: {workflow_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
