import unittest
from pathlib import Path


WORKFLOW = Path(__file__).resolve().parents[3] / ".github" / "workflows" / "ci.yml"
QT_ACTION = "jurplel/install-qt-action@48d3ad6db93f3627c8ee7a0454bc6f3744f7e730"


class RustWorkflowDependenciesTest(unittest.TestCase):
    def test_rust_gate_installs_and_verifies_the_pinned_qt_toolchain(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        rust_job = workflow.split("\n  rust:\n", 1)[1].split("\n  qml:\n", 1)[0]

        self.assertIn(QT_ACTION, rust_job)
        self.assertIn("version: ${{ env.QT_VERSION }}", rust_job)
        self.assertIn('test "$(qtpaths --qt-version)" = "$QT_VERSION"', rust_job)
        self.assertIn('qmake_path="$(qtpaths --query QT_HOST_BINS)/qmake"', rust_job)
        self.assertIn('echo "QMAKE=$qmake_path" >> "$GITHUB_ENV"', rust_job)
        self.assertIn("run: tools/ci/run-rust-gate.sh", rust_job)


if __name__ == "__main__":
    unittest.main()
