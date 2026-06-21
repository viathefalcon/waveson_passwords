Waveson Password Generator
--------------------------

Waveson Password Generator is a Windows app which combines hardware-based sources of randomness with user-specified character sets to generate passwords of varying length. It builds, in part, on my earlier work with [RDRAND](https://github.com/viathefalcon/rdrand_msvc_2010/), which this project incorporates as a [submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules).

If you're just looking for the app, built binaries can be downloaded from [here](https://viathefalcon.net/waveson/wpg/), or it can be built and installed as follows:

## Build

```
msbuild waveson_passwords.sln /p:Configuration=Release /p:Platform=ARM64
```

## Local, Unsigned Installation
Windows 11 only: build, as above, and then:
```pwsh
cd .\Appx\
.\ARM64.bat
```

..to generate the MSIX package. Then, as an Administrator:
```pwsh
Add-AppxPackage -Path .\WPG.ARM64.msix -AllowUnsigned
```
