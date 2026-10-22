//! Settings gaming configuration, platform adapters and native game launching.

pub mod command;
pub mod config;
pub mod launch;
pub mod performance;
pub mod store;

pub type Result<T> = std::result::Result<T, String>;
