use std::collections::{HashMap, HashSet};

use crate::desktop::entries::DesktopEntry;
use unicode_normalization::UnicodeNormalization;
use unicode_normalization::char::is_combining_mark;

fn strip_accents(text: &str) -> String {
    text.nfd().filter(|&c| !is_combining_mark(c)).collect()
}

fn normalize(value: &str) -> String {
    let text: String = value
        .to_lowercase()
        .chars()
        .filter(|c| !c.is_ascii_control())
        .collect();
    strip_accents(&text)
}

fn search_tokens(value: &str) -> Vec<String> {
    let n = normalize(value);
    n.split(|c: char| c.is_ascii_whitespace() || ". _:/\\-".contains(c))
        .filter(|s| !s.is_empty())
        .map(|s| s.to_string())
        .collect()
}

fn acronym_for_tokens(tokens: &[String]) -> String {
    tokens.iter().filter_map(|t| t.chars().next()).collect()
}

fn aliases_for_name(name: &str) -> Vec<String> {
    let tokens = search_tokens(name);
    let mut aliases = Vec::new();
    if tokens.is_empty() {
        return aliases;
    }
    aliases.push(tokens.concat());
    aliases.push(acronym_for_tokens(&tokens));
    if tokens.len() >= 2 {
        let mut partial: String = tokens[..tokens.len() - 1]
            .iter()
            .filter_map(|t| t.chars().next())
            .collect();
        partial.push_str(&tokens[tokens.len() - 1]);
        aliases.push(partial);
    }
    aliases
}

fn is_subsequence(needle: &str, haystack: &str) -> bool {
    let needle: Vec<char> = needle.chars().collect();
    if needle.is_empty() {
        return true;
    }
    let mut at = 0;
    for ch in haystack.chars() {
        if at < needle.len() && ch == needle[at] {
            at += 1;
        }
    }
    at == needle.len()
}

const NAME_BASE: i32 = 0;
const ALIAS_BASE: i32 = 1;
const METADATA_BASE: i32 = 12;
const IDENTIFIER_BASE: i32 = 24;

fn score_text(value: &str, query: &str, search_terms: &[String], base: i32) -> Option<i32> {
    if value.is_empty() {
        return None;
    }

    let parts: Vec<&str> = value
        .split(|c: char| c.is_ascii_whitespace() || ". _:/\\-".contains(c))
        .filter(|s| !s.is_empty())
        .collect();

    if value == query {
        return Some(base);
    }
    if value.starts_with(query) {
        return Some(base + 2);
    }
    if parts.iter().any(|p| p.starts_with(query)) {
        return Some(base + 5);
    }
    if parts
        .iter()
        .any(|p| query.starts_with(p) && p.len() >= 4 && query.len() - p.len() <= 2)
    {
        return Some(base + 7);
    }
    if search_terms.len() > 1
        && search_terms
            .iter()
            .all(|term| parts.iter().any(|p| p.starts_with(term)))
    {
        return Some(base + 8);
    }
    if search_terms
        .iter()
        .all(|term| value.contains(term.as_str()))
    {
        return Some(base + 14);
    }
    if query.len() >= 3 && is_subsequence(query, value) {
        let length_penalty = (value.len() as i32 - query.len() as i32).max(0) / 4;
        return Some(base + 34 + length_penalty);
    }
    None
}

#[derive(Debug, Clone)]
pub struct ScoredEntry {
    pub entry: DesktopEntry,
    pub score: i32,
    pub usage: i32,
    pub(crate) sort_key: String,
}

pub struct SearchableEntry {
    pub entry: DesktopEntry,
    pub name: String,
    pub name_tokens: Vec<String>,
    pub aliases: Vec<String>,
    pub metadata: String,
    pub identifiers: String,
}

pub fn build_searchable_index(entries: &[DesktopEntry]) -> Vec<SearchableEntry> {
    entries
        .iter()
        .map(|entry| {
            let name = normalize(entry.name.trim());
            let name_tokens = search_tokens(&name);
            let aliases = aliases_for_name(&name);

            let mut metadata_parts: Vec<String> = Vec::new();
            if !entry.generic_name.is_empty() {
                metadata_parts.push(normalize(&entry.generic_name));
            }
            if !entry.comment.is_empty() {
                metadata_parts.push(normalize(&entry.comment));
            }
            if !entry.keywords.is_empty() {
                metadata_parts.push(normalize(&entry.keywords.join(" ")));
            }
            if !entry.categories.is_empty() {
                metadata_parts.push(normalize(&entry.categories.join(" ")));
            }
            let metadata = metadata_parts.join(" ");

            let mut id_parts: Vec<String> = Vec::new();
            if !entry.id.is_empty() {
                id_parts.push(normalize(&entry.id));
            }
            if !entry.desktop_file_name.is_empty() {
                id_parts.push(normalize(&entry.desktop_file_name));
            }
            if !entry.startup_wm_class.is_empty() {
                id_parts.push(normalize(&entry.startup_wm_class));
            }
            if !entry.exec.is_empty() {
                id_parts.push(normalize(&entry.exec));
            }
            let identifiers = id_parts.join(" ");

            SearchableEntry {
                entry: entry.clone(),
                name,
                name_tokens,
                aliases,
                metadata,
                identifiers,
            }
        })
        .collect()
}

fn entry_search_score(se: &SearchableEntry, query: &str, search_terms: &[String]) -> i32 {
    let mut best = score_text(&se.name, query, search_terms, NAME_BASE);

    for alias in &se.aliases {
        if let Some(alias_score) = score_text(alias, query, search_terms, ALIAS_BASE)
            && (best.is_none() || alias_score < best.unwrap())
        {
            best = Some(alias_score);
        }
    }

    if !se.metadata.is_empty()
        && let Some(metadata_score) = score_text(&se.metadata, query, search_terms, METADATA_BASE)
        && (best.is_none() || metadata_score < best.unwrap())
    {
        best = Some(metadata_score);
    }

    if !se.identifiers.is_empty()
        && let Some(identifier_score) =
            score_text(&se.identifiers, query, search_terms, IDENTIFIER_BASE)
        && (best.is_none() || identifier_score < best.unwrap())
    {
        best = Some(identifier_score);
    }

    best.unwrap_or(-1)
}

pub fn search(
    searchable: &[SearchableEntry],
    query: &str,
    usage_counts: &HashMap<String, i32>,
    limit: usize,
) -> Vec<ScoredEntry> {
    let q_raw = normalize(query.trim());
    if q_raw.is_empty() {
        return Vec::new();
    }

    let search_terms = search_tokens(&q_raw);
    let mut items: Vec<ScoredEntry> = Vec::new();
    let mut seen_keys = HashSet::new();

    for se in searchable {
        if se.entry.no_display || se.entry.hidden {
            continue;
        }
        let key = se.entry.dedup_key();
        if seen_keys.contains(&key) {
            continue;
        }

        let score = entry_search_score(se, &q_raw, &search_terms);
        if score < 0 {
            continue;
        }

        let usage = usage_counts.get(&key).copied().unwrap_or(0);
        seen_keys.insert(key);
        items.push(ScoredEntry {
            entry: se.entry.clone(),
            score,
            usage,
            sort_key: se.name.clone(),
        });
    }

    items.sort_by(|a, b| {
        a.score
            .cmp(&b.score)
            .then(b.usage.cmp(&a.usage))
            .then(a.sort_key.cmp(&b.sort_key))
    });

    let has_strong = items.iter().any(|item| item.score < 12);
    if has_strong {
        items.retain(|item| item.score < 12);
    }

    items.truncate(limit);
    items
}

pub fn search_entries(
    entries: &[DesktopEntry],
    query: &str,
    usage_counts: &HashMap<String, i32>,
    limit: usize,
) -> Vec<ScoredEntry> {
    let searchable = build_searchable_index(entries);
    search(&searchable, query, usage_counts, limit)
}
