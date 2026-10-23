## Render the Windows icon

The icon converter uses [WSL Container](https://learn.microsoft.com/windows/wsl/wsl-container)
(`wslc.exe`), included with WSL 2.9.3 or later. Check your version with `wsl --version`, and 
use `wsl --update` if needed. Run these commands in PowerShell from the repository root:

```pwsh
wslc.exe build --pull --tag wpg-icon tools/icon
wslc.exe run --rm `
	--mount "type=bind,source=$($PWD.Path)\App\Assets,target=/input,readonly" `
	--mount "type=bind,source=$($PWD.Path)\App,target=/output" `
	wpg-icon /input/Icon.svg /output/WPG.ico
```

This renders `App/Assets/Icon.svg` to `App/WPG.ico`, which the app
already references. The image contains no artwork, so it only needs rebuilding
to pick up tool updates. Pass any input SVG and output ICO path after the image
name; each must lie within a bind-mounted directory. Mount the input read-only,
and mount the output directory to retain the generated file after the container
exits. Each 16, 20, 24, 32, 40, 48, 64, 96, 128, and 256 pixel frame is
rendered independently from the vector at four times its target resolution,
then downsampled with Lanczos filtering. The resulting ICO contains 32-bit RGBA
frames, retaining transparency, the wave, keyhole, and glint without changing the
source artwork. Following the Windows convention, frames up to 128 pixels are
uncompressed bitmaps and the 256 pixel frame is PNG-compressed, which Windows
Vista and later support, including this app's Windows 10 target.
