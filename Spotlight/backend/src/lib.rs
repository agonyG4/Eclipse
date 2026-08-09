#![allow(clippy::missing_safety_doc)]

pub mod config;
pub mod desktop;
pub mod ffi;
pub mod search;
pub mod state;

#[cfg(test)]
mod tests;

pub use desktop::entries as desktop_entries;
pub use ffi::exports::*;
pub use search::ranking;
pub use state::usage;

use std::collections::HashMap;

pub struct AstreaSpotlightBackend {
    astrea_root: String,
    locale: String,
    index: desktop::entries::DesktopEntryIndex,
    external_catalog: Option<Vec<desktop::entries::DesktopEntry>>,
    searchable_entries: Vec<ranking::SearchableEntry>,
    usage_counts: HashMap<String, i32>,
    config: config::store::SpotlightConfig,
}

impl AstreaSpotlightBackend {
    pub fn new(astrea_root: &str, locale: &str) -> Result<Self, String> {
        Self::new_internal(astrea_root, locale, None)
    }

    pub fn new_with_catalog(
        astrea_root: &str,
        locale: &str,
        entries: Vec<desktop::entries::DesktopEntry>,
    ) -> Result<Self, String> {
        Self::new_internal(astrea_root, locale, Some(entries))
    }

    fn new_internal(
        astrea_root: &str,
        locale: &str,
        external_catalog: Option<Vec<desktop::entries::DesktopEntry>>,
    ) -> Result<Self, String> {
        let mut backend = Self {
            astrea_root: astrea_root.to_string(),
            locale: locale.to_string(),
            index: desktop::entries::DesktopEntryIndex::new_with_locale(locale),
            external_catalog,
            searchable_entries: Vec::new(),
            usage_counts: HashMap::new(),
            config: config::store::SpotlightConfig::default(),
        };
        if let Some(entries) = backend.external_catalog.as_ref() {
            backend.searchable_entries = ranking::build_searchable_index(entries);
        } else {
            backend.index.reload();
            backend.searchable_entries = ranking::build_searchable_index(backend.index.entries());
        }
        if let Err(e) = config::store::write_default_config() {
            eprintln!("write_default_config warning: {e}");
        }
        backend.usage_counts = usage::load_usage(astrea_root);
        backend.config = config::store::load_config();
        Ok(backend)
    }

    pub fn set_catalog(&mut self, entries: Vec<desktop::entries::DesktopEntry>) {
        self.external_catalog = Some(entries);
        if let Some(entries) = self.external_catalog.as_ref() {
            self.searchable_entries = ranking::build_searchable_index(entries);
        }
    }

    pub fn reload(&mut self) -> Result<(), String> {
        if let Some(entries) = self.external_catalog.as_ref() {
            self.searchable_entries = ranking::build_searchable_index(entries);
        } else {
            self.index.set_locale(&self.locale);
            self.index.reload();
            self.searchable_entries = ranking::build_searchable_index(self.index.entries());
        }
        self.usage_counts = usage::load_usage(&self.astrea_root);
        self.config = config::store::load_config();
        Ok(())
    }

    pub fn search(&self, query: &str, limit: usize) -> Result<Vec<ranking::ScoredEntry>, String> {
        let usage = &self.usage_counts;
        Ok(ranking::search(
            &self.searchable_entries,
            query,
            usage,
            limit,
        ))
    }

    pub fn search_json(&self, query: &str, limit: usize) -> Result<String, String> {
        let results = self.search(query, limit)?;
        let json_results: Vec<serde_json::Value> = results
            .into_iter()
            .map(|scored| {
                serde_json::json!({
                    "id": scored.entry.id,
                    "name": scored.entry.name,
                    "genericName": scored.entry.generic_name,
                    "comment": scored.entry.comment,
                    "icon": scored.entry.icon,
                    "exec": scored.entry.exec,
                    "desktopFileName": scored.entry.desktop_file_name,
                    "desktopFilePath": scored.entry.path,
                    "startupWmClass": scored.entry.startup_wm_class,
                    "terminal": scored.entry.terminal,
                    "keywords": scored.entry.keywords,
                    "categories": scored.entry.categories,
                    "score": scored.score,
                    "usage": scored.usage,
                })
            })
            .collect();
        serde_json::to_string(&json_results).map_err(|e| format!("serialize results: {e}"))
    }

    pub fn record_launch(&mut self, desktop_id: &str) -> Result<(), String> {
        usage::record_launch(&self.astrea_root, desktop_id)?;
        let count = self.usage_counts.entry(desktop_id.to_string()).or_insert(0);
        *count += 1;
        Ok(())
    }

    pub fn record_activation(&mut self, desktop_id: &str) -> Result<(), String> {
        self.record_launch(desktop_id)
    }
}
