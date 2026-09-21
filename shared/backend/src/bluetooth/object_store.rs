use std::collections::BTreeMap;

pub type PropertyMap = BTreeMap<String, PropertyValue>;
pub type InterfaceMap = BTreeMap<String, PropertyMap>;

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum PropertyValue {
    Boolean(bool),
    Integer(i32),
    String(String),
    Unsupported,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct InterfaceRefreshToken {
    pub generation: u64,
    pub object_path: String,
    pub interface_name: String,
    pub revision: u64,
}

#[derive(Default)]
pub struct ObjectStore {
    generation: u64,
    next_revision: u64,
    objects: BTreeMap<String, InterfaceMap>,
    revisions: BTreeMap<(String, String), u64>,
}

impl ObjectStore {
    pub fn objects(&self) -> &BTreeMap<String, InterfaceMap> {
        &self.objects
    }

    pub fn generation(&self) -> u64 {
        self.generation
    }

    pub fn clear_generation(&mut self, generation: u64) {
        self.generation = generation;
        self.objects.clear();
        self.revisions.clear();
    }

    pub fn replace_all(&mut self, generation: u64, objects: BTreeMap<String, InterfaceMap>) {
        self.clear_generation(generation);
        for (path, interfaces) in objects {
            for (interface_name, properties) in interfaces {
                let revision = self.bump_revision();
                self.objects
                    .entry(path.clone())
                    .or_default()
                    .insert(interface_name.clone(), properties);
                self.revisions
                    .insert((path.clone(), interface_name), revision);
            }
        }
    }

    pub fn interfaces_added(&mut self, path: &str, interfaces: InterfaceMap) {
        for (interface_name, properties) in interfaces {
            let revision = self.bump_revision();
            self.objects
                .entry(path.to_owned())
                .or_default()
                .insert(interface_name.clone(), properties);
            self.revisions
                .insert((path.to_owned(), interface_name), revision);
        }
    }

    pub fn interfaces_removed(&mut self, path: &str, interfaces: &[String]) {
        if interfaces.is_empty() {
            self.objects.remove(path);
            self.revisions
                .retain(|(object_path, _), _| object_path != path);
            return;
        }

        if let Some(object) = self.objects.get_mut(path) {
            for interface_name in interfaces {
                object.remove(interface_name);
                self.revisions
                    .remove(&(path.to_owned(), interface_name.clone()));
            }
            if object.is_empty() {
                self.objects.remove(path);
            }
        }
    }

    pub fn properties_changed(
        &mut self,
        path: &str,
        interface_name: &str,
        changed: PropertyMap,
        invalidated: &[String],
    ) -> bool {
        let Some(properties) = self
            .objects
            .get_mut(path)
            .and_then(|interfaces| interfaces.get_mut(interface_name))
        else {
            return false;
        };

        properties.extend(changed);
        for name in invalidated {
            properties.remove(name);
        }
        let revision = self.bump_revision();
        self.revisions
            .insert((path.to_owned(), interface_name.to_owned()), revision);
        true
    }

    pub fn begin_refresh(
        &self,
        object_path: &str,
        interface_name: &str,
    ) -> Option<InterfaceRefreshToken> {
        if !self
            .objects
            .get(object_path)
            .is_some_and(|interfaces| interfaces.contains_key(interface_name))
        {
            return None;
        }
        let revision = self
            .revisions
            .get(&(object_path.to_owned(), interface_name.to_owned()))
            .copied()?;
        Some(InterfaceRefreshToken {
            generation: self.generation,
            object_path: object_path.to_owned(),
            interface_name: interface_name.to_owned(),
            revision,
        })
    }

    pub fn replace_interface_if_revision(
        &mut self,
        token: &InterfaceRefreshToken,
        properties: PropertyMap,
    ) -> bool {
        if token.generation != self.generation {
            return false;
        }
        let key = (token.object_path.clone(), token.interface_name.clone());
        if self.revisions.get(&key) != Some(&token.revision) {
            return false;
        }
        let Some(interface) = self
            .objects
            .get_mut(&token.object_path)
            .and_then(|interfaces| interfaces.get_mut(&token.interface_name))
        else {
            return false;
        };

        *interface = properties;
        let revision = self.bump_revision();
        self.revisions.insert(key, revision);
        true
    }

    pub fn interface_revision(&self, path: &str, interface_name: &str) -> Option<u64> {
        self.revisions
            .get(&(path.to_owned(), interface_name.to_owned()))
            .copied()
    }

    fn bump_revision(&mut self) -> u64 {
        self.next_revision = self.next_revision.saturating_add(1);
        self.next_revision
    }
}
