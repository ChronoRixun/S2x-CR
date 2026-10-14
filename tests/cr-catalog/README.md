# CR native catalog/Orders tests

See [PARITY.md](PARITY.md) for restoration counts, definitions, policy differences, commands and test limitations.

Build `claims.vcxproj` with Visual Studio 2022 C++ tools and initialized dependencies. The harness is standalone and uses no game process or real profile. Run the EXE from the candidate repository root so its resource fixtures resolve. Output: `build/tests/cr-catalog/bin/claims.exe`; object files: `build/tests/cr-catalog/obj`.

```powershell
msbuild tests/cr-catalog/claims.vcxproj /m /v:minimal /p:Configuration=Release /p:Platform=x64
./build/tests/cr-catalog/bin/claims.exe
python tests/cr-catalog/catalog.py
```
