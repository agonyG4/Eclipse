#!/usr/bin/env python3
"""Unit tests for the CTest JUnit policy validator."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "check-ctest-junit.py"
REQUIRED_TYPHON_TESTS = (
    "typhon-protocol-integration-test",
    "typhon-shortcut-protocol-integration-test",
    "shell-unified-runtime-integration-test",
)


def testcase(name: str, *, skipped: bool = False, failure: bool = False, error: bool = False) -> str:
    children = []
    if skipped:
        children.append("<skipped message=\"intentional\"/>")
    if failure:
        children.append("<failure message=\"failed\"/>")
    if error:
        children.append("<error message=\"errored\"/>")
    return (
        f'<testcase name="{name}" classname="suite">'
        f"{''.join(children)}</testcase>"
    )


def suite(*cases: str) -> str:
    return '<testsuite name="CTest">' + "".join(cases) + "</testsuite>"


class CheckCtestJunitTests(unittest.TestCase):
    def run_validator(self, mode: str, document: str) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as temporary_directory:
            xml_path = Path(temporary_directory) / "ctest.junit.xml"
            xml_path.write_text(document, encoding="utf-8")
            return subprocess.run(
                [sys.executable, str(SCRIPT), mode, str(xml_path)],
                check=False,
                capture_output=True,
                text=True,
            )

    def test_valid_typhon(self) -> None:
        result = self.run_validator(
            "typhon",
            suite(*(testcase(name) for name in REQUIRED_TYPHON_TESTS), testcase("ordinary-test")),
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_typhon_rejects_unexpected_skip(self) -> None:
        result = self.run_validator(
            "typhon",
            suite(
                testcase(REQUIRED_TYPHON_TESTS[0]),
                testcase(REQUIRED_TYPHON_TESTS[1], skipped=True),
                testcase(REQUIRED_TYPHON_TESTS[2]),
            ),
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("skip", result.stderr.lower())

    def test_typhon_rejects_missing_required_test(self) -> None:
        result = self.run_validator(
            "typhon",
            suite(*(testcase(name) for name in REQUIRED_TYPHON_TESTS[:2])),
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(REQUIRED_TYPHON_TESTS[2], result.stderr)

    def test_valid_no_typhon(self) -> None:
        result = self.run_validator(
            "no-typhon",
            suite(
                testcase(REQUIRED_TYPHON_TESTS[0], skipped=True),
                testcase(REQUIRED_TYPHON_TESTS[1], skipped=True),
                testcase(REQUIRED_TYPHON_TESTS[2], skipped=True),
                testcase("ordinary-test"),
            ),
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_no_typhon_rejects_missing_expected_skip(self) -> None:
        result = self.run_validator(
            "no-typhon",
            suite(
                testcase(REQUIRED_TYPHON_TESTS[0], skipped=True),
                testcase(REQUIRED_TYPHON_TESTS[1], skipped=True),
                testcase("ordinary-test"),
            ),
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(REQUIRED_TYPHON_TESTS[2], result.stderr)

    def test_no_typhon_rejects_extra_skip(self) -> None:
        result = self.run_validator(
            "no-typhon",
            suite(
                *(testcase(name, skipped=True) for name in REQUIRED_TYPHON_TESTS),
                testcase("ordinary-test", skipped=True),
            ),
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unexpected", result.stderr.lower())

    def test_rejects_failure(self) -> None:
        result = self.run_validator(
            "typhon",
            suite(*(testcase(name) for name in REQUIRED_TYPHON_TESTS), testcase("bad", failure=True)),
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("failure", result.stderr.lower())

    def test_rejects_error(self) -> None:
        result = self.run_validator(
            "typhon",
            suite(*(testcase(name) for name in REQUIRED_TYPHON_TESTS), testcase("bad", error=True)),
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("error", result.stderr.lower())

    def test_rejects_malformed_xml(self) -> None:
        result = self.run_validator("typhon", "<testsuite>")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("xml", result.stderr.lower())

    def test_rejects_zero_tests(self) -> None:
        result = self.run_validator("typhon", '<testsuite name="CTest" tests="0"/>')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("zero", result.stderr.lower())

    def test_rejects_duplicate_identity(self) -> None:
        result = self.run_validator(
            "typhon",
            suite(
                *(testcase(name) for name in REQUIRED_TYPHON_TESTS),
                testcase("ordinary-test"),
                testcase("ordinary-test"),
            ),
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("duplicate", result.stderr.lower())


if __name__ == "__main__":
    unittest.main()
