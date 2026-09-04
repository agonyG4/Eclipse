# Registered QML Components

The `astrea-settings-ui` module registers 37 QML files. The canonical legacy
source column points to the corresponding Astrea source where one exists.

| Path | Public type | Category | Canonical legacy source | Current consumers | Classification |
| --- | --- | --- | --- | --- | --- |
| `qml/Main.qml` | `Main` | shell | native Settings shell source | application root | shell-critical |
| `qml/components/AppIcon.qml` | `AppIcon` | shell | `src/Core/components/AppIcon.qml` | future page primitives | page primitive |
| `qml/components/AvatarImage.qml` | `AvatarImage` | shell | `src/Core/components/AvatarImage.qml` | `Sidebar` | shell-critical |
| `qml/components/Theme.qml` | `Theme` | theme facade | `src/Core/components/Theme.qml` | all visual components | shell-critical |
| `qml/components/controls/Button.qml` | `Button` | controls | `src/Core/components/controls/Button.qml` | future pages | page primitive |
| `qml/components/controls/ButtonCapsule.qml` | `ButtonCapsule` | controls | `src/Core/components/controls/ButtonCapsule.qml` | future pages | page primitive |
| `qml/components/controls/DualButton.qml` | `DualButton` | controls | `src/Core/components/controls/DualButton.qml` | future pages | page primitive |
| `qml/components/controls/FloatingButton.qml` | `FloatingButton` | controls | `src/Core/components/controls/FloatingButton.qml` | future pages | page primitive |
| `qml/components/feedback/DnsPresetChip.qml` | `DnsPresetChip` | feedback | `src/Core/components/feedback/DnsPresetChip.qml` | none currently | compatibility component |
| `qml/components/feedback/DnsStatusCard.qml` | `DnsStatusCard` | feedback | `src/Core/components/feedback/DnsStatusCard.qml` | none currently | compatibility component |
| `qml/components/feedback/ProgressCard.qml` | `ProgressCard` | feedback | `src/Core/components/feedback/ProgressCard.qml` | none currently | compatibility component |
| `qml/components/feedback/SpeedCard.qml` | `SpeedCard` | feedback | `src/Core/components/feedback/SpeedCard.qml` | none currently | compatibility component |
| `qml/components/feedback/StatusDot.qml` | `StatusDot` | feedback | `src/Core/components/feedback/StatusDot.qml` | `DnsStatusCard` fixture | compatibility component |
| `qml/components/form/FormCard.qml` | `FormCard` | form | `src/Core/components/form/FormCard.qml` | `Compositor` | page primitive |
| `qml/components/form/IconListRow.qml` | `IconListRow` | form | `src/Core/components/form/IconListRow.qml` | none currently | page primitive |
| `qml/components/form/ScrollPage.qml` | `ScrollPage` | form | `src/Core/components/form/ScrollPage.qml` | `Compositor` | page primitive |
| `qml/components/form/SearchField.qml` | `SearchField` | form | `src/Core/components/form/SearchField.qml` | none currently | page primitive |
| `qml/components/form/SectionHeader.qml` | `SectionHeader` | form | `src/Core/components/form/SectionHeader.qml` | `Compositor` | page primitive |
| `qml/components/form/SelectButton.qml` | `SelectButton` | form | `src/Core/components/form/SelectButton.qml` | `Compositor` | page primitive |
| `qml/components/form/SettingRow.qml` | `SettingRow` | form | `src/Core/components/form/SettingRow.qml` | `Compositor` | page primitive |
| `qml/components/form/ToggleSwitch.qml` | `ToggleSwitch` | form | `src/Core/components/form/ToggleSwitch.qml` | `Compositor` | page primitive |
| `qml/components/menu/ContextMenu.qml` | `ContextMenu` | menu | `src/Core/components/menu/ContextMenu.qml` | none currently | page primitive |
| `qml/components/menu/ContextMenuAction.qml` | `ContextMenuAction` | menu | `src/Core/components/menu/ContextMenuAction.qml` | `ContextMenu` fixture | page primitive |
| `qml/components/menu/ContextMenuDivider.qml` | `ContextMenuDivider` | menu | `src/Core/components/menu/ContextMenuDivider.qml` | `ContextMenu` fixture | page primitive |
| `qml/components/navigation/NavItem.qml` | `NavItem` | navigation | `src/Core/components/navigation/NavItem.qml` | `Sidebar` | shell-critical |
| `qml/components/navigation/Sidebar.qml` | `Sidebar` | navigation | `src/Core/components/navigation/Sidebar.qml` | `Main` | shell-critical |
| `qml/components/navigation/SidebarFrame.qml` | `SidebarFrame` | navigation | `src/Core/components/navigation/SidebarFrame.qml` | `Sidebar` | shell-critical |
| `qml/components/typography/DisplayLabel.qml` | `DisplayLabel` | typography | `src/Core/components/typography/DisplayLabel.qml` | future pages | page primitive |
| `qml/components/typography/Divider.qml` | `Divider` | typography | `src/Core/components/typography/Divider.qml` | future pages | page primitive |
| `qml/components/typography/TextLabel.qml` | `TextLabel` | typography | `src/Core/components/typography/TextLabel.qml` | future pages | page primitive |
| `qml/pages/system/Compositor.qml` | `Compositor` | page | native Settings preview source | `Main` Loader | shell-critical |
| `qml/pages/appearance/Wallpaper.qml` | `Wallpaper` | page | `src/Apps/Settings/pages/paper/Wallpaper.qml` | `Main` Loader | native Paper-backed route |
| `qml/pages/appearance/Dock.qml` | `Dock` | page | native Settings Dock personalization | `Main` Loader | native shared-config route |
| `qml/theme/Apps.qml` | `Apps` | singleton theme | `src/Core/components/theme/Borealis/Apps.qml` | `Theme` | shell-critical |
| `qml/theme/Shell.qml` | `Shell` | singleton theme | `src/Core/components/theme/Borealis/Shell.qml` | `Theme` | shell-critical |
| `qml/theme/State.qml` | `State` | singleton theme | `src/Core/components/theme/Borealis/State.qml` | `Theme`, `Apps`, `Shell` | shell-critical |
| `qml/theme/Tokens.qml` | `Tokens` | singleton theme | `src/Core/components/theme/Borealis/Tokens.qml` | `Theme` | shell-critical |
