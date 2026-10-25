# Original Cockpit Authoring Checkpoints

This folder preserves the original mesh experiments and visual captures. The
current playable integration lives on `codex/vr-refresh-oct13`; these are source
references, not a replacement for that branch's authored cockpit.

GitHub does not accept new Git LFS uploads in this public fork. All five Blender
files, including backup versions and the pre-decimation model, are stored
losslessly in `blender-sources.7z.*`. Volumes are at most 48 MiB each.

From the repository root, extract with 7-Zip into an empty directory:

```powershell
7z x artifacts/first-person-kart-screens/blender-sources.7z.001 -oextracted-cockpit
```

The archive retains repository-relative paths. `blender-sources-sha256.json`
records original filenames, byte counts and SHA256 hashes for verification.
Existing local `.blend` and `.blend1` files remain untouched and ignored.
