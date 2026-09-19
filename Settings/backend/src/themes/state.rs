use super::catalog::ThemeCatalog;
use std::fmt::{Display, Formatter};

#[derive(Debug, Eq, PartialEq)]
pub enum SelectionError {
    UnknownTheme(String),
}

impl Display for SelectionError {
    fn fmt(&self, formatter: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::UnknownTheme(id) => write!(formatter, "Icon theme is not installed: {id}"),
        }
    }
}

impl std::error::Error for SelectionError {}

pub struct ThemeSelection {
    catalog: ThemeCatalog,
    selected: Option<String>,
}

impl ThemeSelection {
    pub fn new(catalog: ThemeCatalog) -> Self {
        Self {
            catalog,
            selected: None,
        }
    }

    pub fn selected(&self) -> Option<&str> {
        self.selected.as_deref()
    }

    pub fn select(&mut self, theme_id: &str) -> Result<(), SelectionError> {
        let theme_id = theme_id.trim();
        if !self.catalog.contains_visible(theme_id) {
            return Err(SelectionError::UnknownTheme(theme_id.to_owned()));
        }
        self.selected = Some(theme_id.to_owned());
        Ok(())
    }

    pub fn use_system_default(&mut self) {
        self.selected = None;
    }

    pub fn catalog(&self) -> &ThemeCatalog {
        &self.catalog
    }

    pub fn replace_catalog(&mut self, catalog: ThemeCatalog) {
        if self
            .selected
            .as_deref()
            .is_some_and(|id| !catalog.contains_visible(id))
        {
            self.selected = None;
        }
        self.catalog = catalog;
    }
}
