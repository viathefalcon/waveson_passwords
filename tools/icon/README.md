## Render the Windows icon

The icon converter uses [WSL Container](https://learn.microsoft.com/windows/wsl/wsl-container)
(`wslc.exe`), included with WSL 2.9.3 or later. Check your version with `wsl --version`, and 
use `wsl --update` if needed. Run these commands in PowerShell from the repository root:

```pwsh
wslc.exe build --pull --tag wpg-icon tools/icon
wslc.exe run --rm `
	--mount "type=bind,source=$($PWD.Path)\App\Assets,target=/input,readonly" `
	--mount "type=bind,source=$($PWD.Path)\App,target=/output" `
	wpg-icon /input/Icon.svg /input/IconSmall.svg /output/WPG.ico
```

This renders `App/Assets/Icon.svg` and `App/Assets/IconSmall.svg` to
`App/WPG.ico`, which the app already references. Pass the main SVG, small
SVG, and output ICO path after the image name; each must lie within a
bind-mounted directory. Mount the inputs read-only, and mount the output
directory to retain the generated file after the container exits.