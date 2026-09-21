fn main() {
    cxx_qt_build::CxxQtBuilder::new()
        .qt_module("Core")
        .files(["src/bluetooth/bridge.rs"])
        .build();
}
