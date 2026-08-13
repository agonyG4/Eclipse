"""Unit tests for committed-range whitespace validation."""

from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "check-git-whitespace.sh"
ZERO_SHA = "0" * 40


class CheckGitWhitespaceTests(unittest.TestCase):
    def git(self, repository: Path, *arguments: str) -> str:
        result = subprocess.run(
            ["git", *arguments],
            cwd=repository,
            check=True,
            capture_output=True,
            text=True,
        )
        return result.stdout.strip()

    def repository(self) -> Path:
        temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(temporary_directory.cleanup)
        repository = Path(temporary_directory.name)
        self.git(repository, "init", "-q", "-b", "main")
        self.git(repository, "config", "user.name", "Eclipse CI Test")
        self.git(repository, "config", "user.email", "ci-test@example.invalid")
        return repository

    def commit(self, repository: Path, content: str, message: str) -> str:
        (repository / "sample.txt").write_text(content, encoding="utf-8")
        self.git(repository, "add", "sample.txt")
        self.git(repository, "commit", "-q", "-m", message)
        return self.git(repository, "rev-parse", "HEAD")

    def run_checker(self, repository: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
        try:
            return subprocess.run(
                [str(SCRIPT), *arguments],
                cwd=repository,
                check=False,
                capture_output=True,
                text=True,
            )
        except FileNotFoundError as error:
            return subprocess.CompletedProcess(
                [str(SCRIPT), *arguments],
                127,
                "",
                str(error),
            )

    def test_clean_commit_pr_and_push_ranges_pass(self) -> None:
        repository = self.repository()
        base = self.commit(repository, "base\n", "base")
        head = self.commit(repository, "base\nclean\n", "clean")

        for arguments in (("pr", base, head), ("push", base, head), ("commit", head)):
            with self.subTest(arguments=arguments):
                result = self.run_checker(repository, *arguments)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_trailing_whitespace_fails_range_checks(self) -> None:
        repository = self.repository()
        base = self.commit(repository, "base\n", "base")
        bad_head = self.commit(repository, "base\ntrailing  \n", "bad")

        for arguments in (("pr", base, bad_head), ("push", base, bad_head), ("commit", bad_head)):
            with self.subTest(arguments=arguments):
                result = self.run_checker(repository, *arguments)
                self.assertEqual(result.returncode, 1)

    def test_pr_uses_three_dot_merge_base_range(self) -> None:
        repository = self.repository()
        root = self.commit(repository, "base\n", "root")
        self.git(repository, "checkout", "-q", "-b", "base-branch")
        base_head = self.commit(repository, "base\ntrailing  \n", "base-only bad change")
        self.git(repository, "checkout", "-q", "-b", "feature", root)
        feature_head = self.commit(repository, "base\ntrailing  \n", "feature bad change")

        result = self.run_checker(repository, "pr", base_head, feature_head)
        self.assertEqual(result.returncode, 1, result.stderr)

    def test_zero_before_push_checks_head_commit(self) -> None:
        repository = self.repository()
        bad_head = self.commit(repository, "bad  \n", "bad initial commit")

        result = self.run_checker(repository, "push", ZERO_SHA, bad_head)
        self.assertEqual(result.returncode, 1)

    def test_invalid_mode_and_argument_count_return_two(self) -> None:
        repository = self.repository()
        head = self.commit(repository, "clean\n", "clean")

        for arguments in (("unknown", head, head), ("pr", head), ("commit", head, head)):
            with self.subTest(arguments=arguments):
                result = self.run_checker(repository, *arguments)
                self.assertEqual(result.returncode, 2)


if __name__ == "__main__":
    unittest.main()
