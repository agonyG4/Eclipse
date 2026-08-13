"""Unit tests for Eclipse's constrained GitHub Actions policy checker."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "check-workflow-policy.py"
FULL_SHA = "a" * 40


def workflow(*, body: str = "", permissions: str = "  contents: read") -> str:
    return "\n".join(
        (
            "name: CI",
            "on:",
            "  pull_request:",
            "permissions:",
            permissions,
            "jobs:",
            "  build:",
            "    runs-on: ubuntu-24.04",
            "    steps:",
            "      - uses: owner/action@" + FULL_SHA,
            body,
            "",
        )
    )


class CheckWorkflowPolicyTests(unittest.TestCase):
    def run_checker(self, document: str, *arguments: str) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as temporary_directory:
            workflow_path = Path(temporary_directory) / "ci.yml"
            workflow_path.write_text(document, encoding="utf-8")
            return subprocess.run(
                [sys.executable, str(SCRIPT), str(workflow_path), *arguments],
                check=False,
                capture_output=True,
                text=True,
            )

    def test_accepts_intended_workflow_policy(self) -> None:
        result = self.run_checker(workflow())
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_rejects_active_pull_request_target(self) -> None:
        result = self.run_checker(
            workflow().replace("  pull_request:\n", "    pull_request_target:\n")
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("pull_request_target", result.stderr)

    def test_rejects_inline_pull_request_target(self) -> None:
        result = self.run_checker(
            workflow().replace("on:\n  pull_request:\n", "on: [pull_request_target]\n")
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("pull_request_target", result.stderr)

    def test_comment_with_forbidden_text_is_ignored(self) -> None:
        result = self.run_checker(
            workflow(
                body=(
                    "      # pull_request_target: forbidden\n"
                    "      # permissions: write\n"
                    "      # uses: owner/action@main\n"
                    "      # ${{ secrets.TOKEN }}"
                )
            )
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_rejects_contents_write(self) -> None:
        result = self.run_checker(workflow(permissions="  contents: write"))
        self.assertEqual(result.returncode, 1)
        self.assertIn("contents", result.stderr)

    def test_rejects_write_all(self) -> None:
        result = self.run_checker(workflow(permissions="  write-all"))
        self.assertEqual(result.returncode, 1)
        self.assertIn("permissions", result.stderr)

    def test_rejects_inline_write_all(self) -> None:
        result = self.run_checker(
            workflow().replace("permissions:\n  contents: read", "permissions: write-all")
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("permissions", result.stderr)

    def test_rejects_job_level_permissions(self) -> None:
        result = self.run_checker(
            workflow(body="    permissions:\n      contents: read")
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("job-level", result.stderr)

    def test_rejects_mutable_action_references(self) -> None:
        for reference in ("main", "master", "latest", "v7", "v7.0.1", "abcdef0"):
            with self.subTest(reference=reference):
                result = self.run_checker(
                    workflow(body=f"      - uses: owner/action@{reference}")
                )
                self.assertEqual(result.returncode, 1)
                self.assertIn("immutable", result.stderr)

    def test_accepts_full_40_character_action_sha(self) -> None:
        result = self.run_checker(
            workflow(body=f"      - uses: owner/another-action@{FULL_SHA}")
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_accepts_local_action(self) -> None:
        result = self.run_checker(workflow(body="      - uses: ./tools/local-action"))
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_rejects_secret_reference(self) -> None:
        result = self.run_checker(
            workflow(body="      env:\n        TOKEN: ${{ secrets.TOKEN }}")
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("secret", result.stderr.lower())

    def test_rejects_missing_permissions_block(self) -> None:
        result = self.run_checker(workflow().replace("permissions:\n  contents: read\n", ""))
        self.assertEqual(result.returncode, 1)
        self.assertIn("top-level permissions", result.stderr)

    def test_rejects_cli_usage_error(self) -> None:
        result = subprocess.run(
            [sys.executable, str(SCRIPT)],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 2)


if __name__ == "__main__":
    unittest.main()
