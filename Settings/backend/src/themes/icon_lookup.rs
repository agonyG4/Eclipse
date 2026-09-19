use super::catalog::{DirectorySpec, DirectoryType, ThemeCatalog};
use std::collections::HashSet;
use std::path::{Path, PathBuf};

const MAX_INHERIT_DEPTH: usize = 32;
const MAX_VISITED_THEMES: usize = 64;

pub struct PreviewResolver {
    catalog: ThemeCatalog,
}

impl PreviewResolver {
    pub fn new(catalog: ThemeCatalog) -> Self {
        Self { catalog }
    }

    pub fn resolve(&self, theme_id: &str, icon_name: &str, logical_size: u32) -> Option<PathBuf> {
        if logical_size == 0 || !safe_icon_name(icon_name) {
            return None;
        }
        let mut visited = HashSet::new();
        self.resolve_theme(theme_id, icon_name, logical_size, 0, &mut visited)
    }

    fn resolve_theme(
        &self,
        theme_id: &str,
        icon_name: &str,
        logical_size: u32,
        depth: usize,
        visited: &mut HashSet<String>,
    ) -> Option<PathBuf> {
        if depth >= MAX_INHERIT_DEPTH || visited.len() >= MAX_VISITED_THEMES {
            return None;
        }
        if !visited.insert(theme_id.to_owned()) {
            return None;
        }
        let theme = self.catalog.theme(theme_id)?;
        for root in &theme.roots {
            if let Some(path) = resolve_in_root(root, &theme.directories, icon_name, logical_size) {
                return Some(path);
            }
        }
        for inherited in &theme.inherits {
            if let Some(path) =
                self.resolve_theme(inherited, icon_name, logical_size, depth + 1, visited)
            {
                return Some(path);
            }
        }
        if theme_id != "hicolor" {
            return self.resolve_theme("hicolor", icon_name, logical_size, depth + 1, visited);
        }
        None
    }
}

fn resolve_in_root(
    root: &Path,
    directories: &[DirectorySpec],
    icon_name: &str,
    logical_size: u32,
) -> Option<PathBuf> {
    let mut candidates = directories
        .iter()
        .enumerate()
        .map(|(index, directory)| (directory_score(directory, logical_size), index, directory))
        .collect::<Vec<_>>();
    candidates.sort_by_key(|(score, index, _)| (*score, *index));
    for (_, _, directory) in candidates {
        let directory_path = root.join(&directory.path);
        for candidate in icon_candidates(&directory_path, icon_name) {
            if candidate.is_file() {
                return Some(candidate);
            }
        }
    }
    None
}

fn directory_score(directory: &DirectorySpec, size: u32) -> u32 {
    let scale = directory.scale.max(1);
    let effective_size = directory.size.saturating_mul(scale);
    let effective_min = directory.min_size.saturating_mul(scale);
    let effective_max = directory.max_size.saturating_mul(scale);
    match directory.kind {
        DirectoryType::Fixed => {
            if effective_size == size {
                0
            } else {
                1000 + effective_size.abs_diff(size)
            }
        }
        DirectoryType::Scalable => {
            if size >= effective_min && size <= effective_max {
                0
            } else {
                1000 + size.abs_diff(effective_min.min(effective_max))
            }
        }
        DirectoryType::Threshold => {
            let distance = effective_size.abs_diff(size);
            if distance <= directory.threshold {
                distance
            } else {
                1000 + distance
            }
        }
    }
}

fn icon_candidates(directory: &Path, icon_name: &str) -> Vec<PathBuf> {
    let path = directory.join(icon_name);
    if path.extension().is_some() {
        return vec![path];
    }
    ["png", "svg", "xpm"]
        .into_iter()
        .map(|extension| directory.join(format!("{icon_name}.{extension}")))
        .collect()
}

fn safe_icon_name(icon_name: &str) -> bool {
    let path = Path::new(icon_name);
    !icon_name.is_empty()
        && !path.is_absolute()
        && path
            .components()
            .all(|component| !matches!(component, std::path::Component::ParentDir))
}

#[cfg(test)]
mod tests {
    use super::super::catalog::ThemeCatalog;
    use super::PreviewResolver;
    use std::fs;
    use std::path::Path;
    use tempfile::TempDir;

    fn write_theme(root: &Path, id: &str, metadata: &str) {
        let theme = root.join(id);
        fs::create_dir_all(&theme).unwrap();
        fs::write(theme.join("index.theme"), metadata).unwrap();
    }

    fn write_icon(root: &Path, theme: &str, directory: &str, name: &str) {
        let path = root.join(theme).join(directory);
        fs::create_dir_all(&path).unwrap();
        fs::write(path.join(name), b"preview").unwrap();
    }

    #[test]
    fn resolver_matches_fixed_scalable_threshold_and_scaled_directories() {
        let root = TempDir::new().unwrap();
        write_theme(
            root.path(),
            "preview-theme",
            "[Icon Theme]\nName=Preview\nDirectories=48x48/apps,threshold/apps,scalable/apps\nScaledDirectories=24x24/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n\n[threshold/apps]\nSize=32\nType=Threshold\nThreshold=4\n\n[scalable/apps]\nMinSize=16\nMaxSize=128\nType=Scalable\n\n[24x24/apps]\nSize=24\nScale=2\nType=Fixed\n",
        );
        write_theme(
            root.path(),
            "hicolor",
            "[Icon Theme]\nName=hicolor\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        write_icon(root.path(), "preview-theme", "48x48/apps", "fixed.png");
        write_icon(
            root.path(),
            "preview-theme",
            "threshold/apps",
            "threshold.svg",
        );
        write_icon(
            root.path(),
            "preview-theme",
            "scalable/apps",
            "scalable.xpm",
        );
        write_icon(root.path(), "preview-theme", "24x24/apps", "scaled.png");

        let catalog = ThemeCatalog::discover(&[root.path().into()]).unwrap();
        let resolver = PreviewResolver::new(catalog);
        assert!(
            resolver
                .resolve("preview-theme", "fixed", 48)
                .unwrap()
                .ends_with("fixed.png")
        );
        assert!(
            resolver
                .resolve("preview-theme", "threshold", 35)
                .unwrap()
                .ends_with("threshold.svg")
        );
        assert!(
            resolver
                .resolve("preview-theme", "scalable", 96)
                .unwrap()
                .ends_with("scalable.xpm")
        );
        assert!(
            resolver
                .resolve("preview-theme", "scaled", 48)
                .unwrap()
                .ends_with("scaled.png")
        );
    }

    #[test]
    fn resolver_follows_inheritance_and_hicolor_without_looping_on_cycles() {
        let root = TempDir::new().unwrap();
        write_theme(
            root.path(),
            "child",
            "[Icon Theme]\nName=Child\nInherits=parent\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        write_theme(
            root.path(),
            "parent",
            "[Icon Theme]\nName=Parent\nInherits=child,hicolor\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        write_theme(
            root.path(),
            "hicolor",
            "[Icon Theme]\nName=hicolor\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        write_icon(root.path(), "hicolor", "48x48/apps", "fallback.png");

        let catalog = ThemeCatalog::discover(&[root.path().into()]).unwrap();
        let resolver = PreviewResolver::new(catalog);
        assert!(
            resolver
                .resolve("child", "fallback", 48)
                .unwrap()
                .ends_with("fallback.png")
        );
        assert!(resolver.resolve("child", "missing", 48).is_none());
    }
}
