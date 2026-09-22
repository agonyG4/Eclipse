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

    pub fn resolve(
        &self,
        theme_id: &str,
        icon_name: &str,
        size: u32,
        scale: u32,
    ) -> Option<PathBuf> {
        if size == 0 || scale == 0 || !safe_icon_name(icon_name) {
            return None;
        }
        let mut visited = HashSet::new();
        self.resolve_theme(theme_id, icon_name, size, scale, 0, &mut visited)
            .or_else(|| {
                if visited.contains("hicolor") {
                    None
                } else {
                    self.resolve_theme("hicolor", icon_name, size, scale, 0, &mut visited)
                }
            })
    }

    fn resolve_theme(
        &self,
        theme_id: &str,
        icon_name: &str,
        size: u32,
        scale: u32,
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
        if let Some(path) =
            resolve_in_theme(&theme.roots, &theme.directories, icon_name, size, scale)
        {
            return Some(path);
        }
        for inherited in &theme.inherits {
            if let Some(path) =
                self.resolve_theme(inherited, icon_name, size, scale, depth + 1, visited)
            {
                return Some(path);
            }
        }
        None
    }
}

fn resolve_in_theme(
    roots: &[PathBuf],
    directories: &[DirectorySpec],
    icon_name: &str,
    size: u32,
    scale: u32,
) -> Option<PathBuf> {
    for directory in directories {
        if !directory_matches_request(directory, size, scale) {
            continue;
        }
        for root in roots {
            let directory_path = root.join(&directory.path);
            for candidate in icon_candidates(&directory_path, icon_name) {
                if candidate.is_file() {
                    return Some(candidate);
                }
            }
        }
    }

    let mut closest: Option<(u32, PathBuf)> = None;
    for directory in directories {
        let distance = directory_score(directory, size, scale);
        for root in roots {
            let directory_path = root.join(&directory.path);
            for candidate in icon_candidates(&directory_path, icon_name) {
                if !candidate.is_file() {
                    continue;
                }
                if closest
                    .as_ref()
                    .is_none_or(|(closest_distance, _)| distance < *closest_distance)
                {
                    closest = Some((distance, candidate));
                }
            }
        }
    }
    closest.map(|(_, path)| path)
}

fn directory_matches_request(directory: &DirectorySpec, size: u32, scale: u32) -> bool {
    directory.scale.max(1) == scale
        && match directory.kind {
            DirectoryType::Fixed => directory.size == size,
            DirectoryType::Scalable => {
                let minimum = directory.min_size.min(directory.max_size);
                let maximum = directory.min_size.max(directory.max_size);
                size >= minimum && size <= maximum
            }
            DirectoryType::Threshold => directory.size.abs_diff(size) <= directory.threshold,
        }
}

fn directory_score(directory: &DirectorySpec, size: u32, scale: u32) -> u32 {
    let directory_scale = directory.scale.max(1);
    let requested_size = size.saturating_mul(scale.max(1));
    let effective_size = directory.size.saturating_mul(directory_scale);
    match directory.kind {
        DirectoryType::Fixed => effective_size.abs_diff(requested_size),
        DirectoryType::Scalable => {
            let minimum = directory.min_size.min(directory.max_size);
            let maximum = directory.min_size.max(directory.max_size);
            let effective_minimum = minimum.saturating_mul(directory_scale);
            let effective_maximum = maximum.saturating_mul(directory_scale);
            if requested_size < effective_minimum {
                effective_minimum - requested_size
            } else if requested_size > effective_maximum {
                requested_size.saturating_sub(effective_maximum)
            } else {
                0
            }
        }
        DirectoryType::Threshold => effective_size
            .abs_diff(requested_size)
            .saturating_sub(directory.threshold.saturating_mul(directory_scale)),
    }
}

fn icon_candidates(directory: &Path, icon_name: &str) -> Vec<PathBuf> {
    ["png", "svg", "xpm"]
        .into_iter()
        .map(|extension| directory.join(format!("{icon_name}.{extension}")))
        .collect()
}

fn safe_icon_name(icon_name: &str) -> bool {
    let path = Path::new(icon_name);
    !icon_name.is_empty()
        && !icon_name.contains('/')
        && !icon_name.contains('\\')
        && !path.is_absolute()
        && matches!(
            path.components().next(),
            Some(std::path::Component::Normal(_))
        )
        && path.components().count() == 1
}

#[cfg(test)]
mod tests {
    use super::super::catalog::{DirectorySpec, DirectoryType, ThemeCatalog};
    use super::{PreviewResolver, directory_matches_request, directory_score, safe_icon_name};
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

    fn scalable_directory(min_size: u32, max_size: u32) -> DirectorySpec {
        DirectorySpec {
            path: String::from("scalable/apps"),
            size: min_size,
            min_size,
            max_size,
            threshold: 0,
            scale: 1,
            kind: DirectoryType::Scalable,
        }
    }

    #[test]
    fn scalable_directory_score_uses_distance_below_minimum() {
        assert_eq!(directory_score(&scalable_directory(24, 96), 12, 1), 12);
    }

    #[test]
    fn scalable_directory_score_is_zero_inside_range() {
        assert_eq!(directory_score(&scalable_directory(24, 96), 48, 1), 0);
    }

    #[test]
    fn scalable_directory_score_uses_distance_above_maximum() {
        assert_eq!(directory_score(&scalable_directory(24, 96), 128, 1), 32);
    }

    #[test]
    fn scalable_directory_score_compares_above_range_distance_to_maximum() {
        let closer_maximum = scalable_directory(8, 90);
        let farther_maximum = scalable_directory(50, 80);

        assert!(
            directory_score(&closer_maximum, 100, 1) < directory_score(&farther_maximum, 100, 1)
        );
    }

    #[test]
    fn resolver_matches_fixed_scalable_threshold_and_scaled_directories() {
        let root = TempDir::new().unwrap();
        write_theme(
            root.path(),
            "preview-theme",
            "[Icon Theme]\nName=Preview\nDirectories=48x48/apps,threshold/apps,scalable/apps\nScaledDirectories=24x24/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n\n[threshold/apps]\nSize=32\nType=Threshold\nThreshold=4\n\n[scalable/apps]\nSize=48\nMinSize=16\nMaxSize=128\nType=Scalable\n\n[24x24/apps]\nSize=24\nScale=2\nType=Fixed\n",
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
                .resolve("preview-theme", "fixed", 48, 1)
                .unwrap()
                .ends_with("fixed.png")
        );
        assert!(
            resolver
                .resolve("preview-theme", "threshold", 35, 1)
                .unwrap()
                .ends_with("threshold.svg")
        );
        assert!(
            resolver
                .resolve("preview-theme", "scalable", 96, 1)
                .unwrap()
                .ends_with("scalable.xpm")
        );
        assert!(
            resolver
                .resolve("preview-theme", "scaled", 48, 1)
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
                .resolve("child", "fallback", 48, 1)
                .unwrap()
                .ends_with("fallback.png")
        );
        assert!(resolver.resolve("child", "missing", 48, 1).is_none());
    }

    #[test]
    fn explicit_hicolor_inheritance_precedes_later_parents() {
        let root = TempDir::new().unwrap();
        write_theme(
            root.path(),
            "child",
            "[Icon Theme]\nName=Child\nInherits=hicolor,OtherTheme\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        write_theme(
            root.path(),
            "hicolor",
            "[Icon Theme]\nName=hicolor\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        write_theme(
            root.path(),
            "OtherTheme",
            "[Icon Theme]\nName=Other Theme\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        write_icon(root.path(), "hicolor", "48x48/apps", "folder.png");
        write_icon(root.path(), "OtherTheme", "48x48/apps", "folder.png");

        let catalog = ThemeCatalog::discover(&[root.path().into()]).unwrap();
        let path = PreviewResolver::new(catalog)
            .resolve("child", "folder", 48, 1)
            .unwrap();
        assert!(path.starts_with(root.path().join("hicolor")));
    }

    #[test]
    fn exact_phase_checks_every_declared_directory_before_closest_fallback() {
        let user = TempDir::new().unwrap();
        let system = TempDir::new().unwrap();
        write_theme(
            user.path(),
            "ordered-theme",
            "[Icon Theme]\nName=Ordered\nDirectories=scalable/apps,48x48/apps\n\n[scalable/apps]\nSize=24\nMinSize=16\nMaxSize=32\nType=Scalable\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        write_theme(
            system.path(),
            "ordered-theme",
            "[Icon Theme]\nName=Ignored Lower Index\nDirectories=48x48/apps\n\n[48x48/apps]\nSize=48\nType=Fixed\n",
        );
        write_icon(user.path(), "ordered-theme", "scalable/apps", "target.png");
        write_icon(system.path(), "ordered-theme", "48x48/apps", "target.png");

        let catalog = ThemeCatalog::discover(&[user.path().into(), system.path().into()]).unwrap();
        let path = PreviewResolver::new(catalog)
            .resolve("ordered-theme", "target", 48, 1)
            .unwrap();
        assert!(path.ends_with("48x48/apps/target.png"));
        assert!(path.starts_with(system.path()));
    }

    #[test]
    fn exact_directory_order_precedes_root_priority() {
        let user = TempDir::new().unwrap();
        let system = TempDir::new().unwrap();
        write_theme(
            user.path(),
            "ordered-theme",
            "[Icon Theme]\nName=Ordered\nDirectories=first/apps,second/apps\n\n[first/apps]\nSize=48\nType=Fixed\n\n[second/apps]\nSize=48\nType=Fixed\n",
        );
        write_theme(
            system.path(),
            "ordered-theme",
            "[Icon Theme]\nName=Ignored Lower Index\nDirectories=first/apps\n\n[first/apps]\nSize=48\nType=Fixed\n",
        );
        write_icon(user.path(), "ordered-theme", "second/apps", "target.png");
        write_icon(system.path(), "ordered-theme", "first/apps", "target.png");

        let catalog = ThemeCatalog::discover(&[user.path().into(), system.path().into()]).unwrap();
        let path = PreviewResolver::new(catalog)
            .resolve("ordered-theme", "target", 48, 1)
            .unwrap();
        assert!(path.ends_with("first/apps/target.png"));
        assert!(path.starts_with(system.path()));
    }

    #[test]
    fn closest_phase_uses_directory_order_then_root_order_as_tie_breakers() {
        let user = TempDir::new().unwrap();
        let system = TempDir::new().unwrap();
        write_theme(
            user.path(),
            "closest-theme",
            "[Icon Theme]\nName=Closest\nDirectories=44/apps,52/apps\n\n[44/apps]\nSize=44\nType=Fixed\n\n[52/apps]\nSize=52\nType=Fixed\n",
        );
        write_theme(
            system.path(),
            "closest-theme",
            "[Icon Theme]\nName=Ignored\nDirectories=44/apps\n\n[44/apps]\nSize=44\nType=Fixed\n",
        );
        write_icon(user.path(), "closest-theme", "52/apps", "target.png");
        write_icon(system.path(), "closest-theme", "44/apps", "target.png");

        let catalog = ThemeCatalog::discover(&[user.path().into(), system.path().into()]).unwrap();
        let path = PreviewResolver::new(catalog)
            .resolve("closest-theme", "target", 48, 1)
            .unwrap();
        assert!(path.ends_with("44/apps/target.png"));
        assert!(path.starts_with(system.path()));
    }

    fn directory(
        kind: DirectoryType,
        size: u32,
        min_size: u32,
        max_size: u32,
        threshold: u32,
        scale: u32,
    ) -> DirectorySpec {
        DirectorySpec {
            path: String::from("apps"),
            size,
            min_size,
            max_size,
            threshold,
            scale,
            kind,
        }
    }

    #[test]
    fn scalable_distance_is_zero_inside_and_distance_to_both_bounds_outside() {
        let scalable = directory(DirectoryType::Scalable, 16, 16, 32, 0, 2);
        assert_eq!(directory_score(&scalable, 32, 1), 0);
        assert_eq!(directory_score(&scalable, 31, 1), 1);
        assert_eq!(directory_score(&scalable, 64, 1), 0);
        assert_eq!(directory_score(&scalable, 65, 1), 1);
    }

    #[test]
    fn threshold_distance_scales_its_size_and_threshold() {
        let threshold = directory(DirectoryType::Threshold, 10, 10, 10, 2, 2);
        assert_eq!(directory_score(&threshold, 16, 1), 0);
        assert_eq!(directory_score(&threshold, 15, 1), 1);
        assert_eq!(directory_score(&threshold, 24, 1), 0);
        assert_eq!(directory_score(&threshold, 25, 1), 1);
    }

    #[test]
    fn fixed_distance_uses_scaled_pixels_without_treating_scale_as_equal() {
        let fixed = directory(DirectoryType::Fixed, 24, 24, 24, 0, 2);
        assert_eq!(directory_score(&fixed, 48, 1), 0);
        assert!(!directory_matches_request(&fixed, 48, 1));
        assert!(directory_matches_request(&fixed, 24, 2));
    }

    #[test]
    fn resolver_prefers_48_at_scale_one_over_24_at_scale_two() {
        let root = TempDir::new().unwrap();
        write_theme(
            root.path(),
            "scale-theme",
            "[Icon Theme]\nName=Scale\nDirectories=24x2/apps,48x1/apps\n\n[24x2/apps]\nSize=24\nScale=2\nType=Fixed\n\n[48x1/apps]\nSize=48\nScale=1\nType=Fixed\n",
        );
        write_icon(root.path(), "scale-theme", "24x2/apps", "target.png");
        write_icon(root.path(), "scale-theme", "48x1/apps", "target.png");

        let catalog = ThemeCatalog::discover(&[root.path().into()]).unwrap();
        let path = PreviewResolver::new(catalog)
            .resolve("scale-theme", "target", 48, 1)
            .unwrap();
        assert!(path.ends_with("48x1/apps/target.png"));
    }

    #[test]
    fn resolver_can_select_twenty_four_at_scale_two() {
        let root = TempDir::new().unwrap();
        write_theme(
            root.path(),
            "scale-theme",
            "[Icon Theme]\nName=Scale\nDirectories=24x2/apps,48x1/apps\n\n[24x2/apps]\nSize=24\nScale=2\nType=Fixed\n\n[48x1/apps]\nSize=48\nScale=1\nType=Fixed\n",
        );
        write_icon(root.path(), "scale-theme", "24x2/apps", "target.png");
        write_icon(root.path(), "scale-theme", "48x1/apps", "target.png");

        let catalog = ThemeCatalog::discover(&[root.path().into()]).unwrap();
        let path = PreviewResolver::new(catalog)
            .resolve("scale-theme", "target", 24, 2)
            .unwrap();
        assert!(path.ends_with("24x2/apps/target.png"));
    }

    #[test]
    fn icon_names_are_single_safe_names_and_extensions_prefer_png_svg_xpm() {
        assert!(safe_icon_name("folder"));
        assert!(!safe_icon_name("../folder"));
        assert!(!safe_icon_name("folder/child"));
        assert!(!safe_icon_name("folder\\child"));

        let root = TempDir::new().unwrap();
        write_theme(
            root.path(),
            "extension-theme",
            "[Icon Theme]\nName=Extensions\nDirectories=48/apps\n\n[48/apps]\nSize=48\nType=Fixed\n",
        );
        for extension in ["svg", "xpm", "png"] {
            write_icon(
                root.path(),
                "extension-theme",
                "48/apps",
                &format!("target.{extension}"),
            );
        }
        let catalog = ThemeCatalog::discover(&[root.path().into()]).unwrap();
        let path = PreviewResolver::new(catalog)
            .resolve("extension-theme", "target", 48, 1)
            .unwrap();
        assert!(path.ends_with("target.png"));
    }
}
