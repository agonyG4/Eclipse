# Display preview and physical size

User approved implementation directly on main.

Keep the existing Settings layout and wallpaper preview. Preserve monitor and image
items across draft/snapshot updates to prevent flashing. Use the selected draft
mode and transform for preview geometry; changing a selection must never apply a
hardware mode. Fit the complete monitor, bezel and stand inside the available area.
Use preferred/current resolution as the reference so smaller resolutions visibly
shrink instead of being normalized to the same width. Refine the bezel, stand and
connector badge without changing the page's visual language.

Typhon must advertise connector physical millimetres through wl_output geometry,
using zero for unknown dimensions rather than a fictional monitor size. Pixel
resolution changes must not change the reported physical dimensions.

Implementation and verification:
1. Add regression coverage for delegate lifetime, draft aspect/size, portrait and
   narrow layouts, and real Wayland physical size events.
2. Correct preview helpers, sizing, persistent delegates and wallpaper decode size.
3. Pass KMS connector physical size into compositor output publication and update
   existing bindings when metadata changes.
4. Run focused QML integration and compositor protocol tests using the existing
   Aether build trees; inspect the diff and commit only this task's changes.
