#!/usr/bin/env python3
"""Validate CTest JUnit output against Eclipse's integration-test policy."""

from __future__ import annotations

import argparse
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


REQUIRED_INTEGRATION_TESTS = frozenset(
    {
        "typhon-protocol-integration-test",
        "typhon-shortcut-protocol-integration-test",
        "shell-unified-runtime-integration-test",
    }
)


def local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def fail(message: str) -> int:
    print(f"CTest JUnit validation failed: {message}", file=sys.stderr)
    return 1


def testcase_is_skipped(testcase: ET.Element) -> bool:
    if testcase.attrib.get("status", "").lower() in {"notrun", "skipped"}:
        return True
    return any(local_name(child.tag) == "skipped" for child in testcase)


def validate(mode: str, xml_path: Path) -> int:
    try:
        root = ET.parse(xml_path).getroot()
    except (OSError, ET.ParseError) as error:
        return fail(f"could not parse XML: {error}")

    if local_name(root.tag) not in {"testsuite", "testsuites"}:
        return fail("root element must be <testsuite> or <testsuites>")

    testcases = [element for element in root.iter() if local_name(element.tag) == "testcase"]
    if not testcases:
        return fail("zero testcases were reported")

    names: list[str] = []
    skipped_names: set[str] = set()
    for testcase in testcases:
        name = testcase.attrib.get("name", "").strip()
        if not name:
            return fail("a testcase has no name")
        if name in names:
            return fail(f"duplicate testcase identity: {name}")
        names.append(name)

        if testcase_is_skipped(testcase):
            skipped_names.add(name)

        child_failures = [child for child in testcase if local_name(child.tag) == "failure"]
        child_errors = [child for child in testcase if local_name(child.tag) == "error"]
        if child_failures:
            return fail(f"testcase reported failure: {name}")
        if child_errors:
            return fail(f"testcase reported error: {name}")

    for suite in root.iter():
        failures = suite.attrib.get("failures", "0")
        errors = suite.attrib.get("errors", "0")
        try:
            if int(failures) > 0:
                return fail("JUnit reports one or more failures")
            if int(errors) > 0:
                return fail("JUnit reports one or more errors")
        except ValueError:
            return fail("JUnit failure/error counts are not integers")

    present_names = set(names)
    missing = sorted(REQUIRED_INTEGRATION_TESTS - present_names)
    if missing:
        return fail("missing required integration test(s): " + ", ".join(missing))

    if mode == "typhon":
        if skipped_names:
            return fail("unexpected skipped testcase(s): " + ", ".join(sorted(skipped_names)))
    elif mode == "no-typhon":
        if skipped_names != REQUIRED_INTEGRATION_TESTS:
            missing_skips = sorted(REQUIRED_INTEGRATION_TESTS - skipped_names)
            extra_skips = sorted(skipped_names - REQUIRED_INTEGRATION_TESTS)
            details = []
            if missing_skips:
                details.append("missing expected skips: " + ", ".join(missing_skips))
            if extra_skips:
                details.append("unexpected skips: " + ", ".join(extra_skips))
            return fail("; ".join(details))
    else:
        return fail(f"unknown mode: {mode}")

    print(f"CTest JUnit valid: mode={mode}, tests={len(testcases)}, skips={len(skipped_names)}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("typhon", "no-typhon"))
    parser.add_argument("xml", type=Path)
    args = parser.parse_args(argv)
    return validate(args.mode, args.xml)


if __name__ == "__main__":
    raise SystemExit(main())
