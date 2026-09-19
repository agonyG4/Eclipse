fn main() {
    cxx_qt_build::CxxQtBuilder::new()
        .qt_module("Network")
        .files(["src/animation/qobject.rs", "src/themes/qobject.rs"])
        .build();
}
