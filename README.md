Open `VSCode` using `x64 Native Tools Command Prompt` and switch to `pwsh`
```powershell
cmd> pwsh
PS> code -n .
```
### Create a `beagle.lib` file
- Create the `beagle.def` file (just look at it)
- Use the `lib` tool   
    ```powershell
    PS> lib /def:beagle.def /out:beagle.lib /machine:x64
    ```
### Create a `build` folder
```
pwsh> New-Item -ItemType Directory -Name build
```
### Create a `tasks.json` file with arguments:
```json
"args": [
    "/Zi", "/EHsc", "/nologo",
    "/I.", "beagle.lib",                            // Specify include dir and link beagle.lib 
    "/Fo${workspaceFolder}\\build\\",               // object-files
    "/Fd${workspaceFolder}\\build\\capture.pdb",    // debug symbols
    "/Fe${workspaceFolder}\\build\\capture.exe",    // output binary
    "${file}"
],
```
Build by pressing `CTRL` + `SHIFT` + `B` when the main CPP file is active.