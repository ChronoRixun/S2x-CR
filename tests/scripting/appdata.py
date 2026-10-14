"""Exercise the production AppData selector in separate native test processes."""
from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[2]
out=root/'build/tests/appdata'; out.mkdir(parents=True,exist_ok=True)
s=(root/'src/client/game/game.cpp').read_text()
start=s.index('std::filesystem::path get_appdata_path()')
end=s.index('\n\t}',start)+3
function=s[start:end]
code=r'''#include <filesystem>
#include <optional>
#include <string>
#include <stdexcept>
#include <cassert>
#include <iostream>
int mode{}, resolutions{}, frees{};
using PWSTR=wchar_t*;
constexpr int FOLDERID_LocalAppData=1;
#define FAILED(x) ((x)<0)
int SHGetKnownFolderPath(int,int,void*,PWSTR* p) { ++resolutions; static wchar_t path[]=L"C:\\DefaultAppData"; *p=path; return 0; }
void CoTaskMemFree(void*) { ++frees; }
namespace utils { template<class F> struct guard { F f; ~guard(){f();} }; template<class F> auto finally(F f){return guard<F>{f};} }
namespace utils::flags {
std::optional<std::string> get_value(const std::string&) { if(mode==1) return "D:/candidate/own-data"; if(mode==2) return "relative-data"; return {}; }
bool has_flag(const std::string&) { return mode==3; }
}
namespace game { FUNCTION }
int main(int argc,char**argv) {
 mode=argc>1?std::stoi(argv[1]):0;
 if(mode==2 || mode==3) { bool rejected{}; try{game::get_appdata_path();}catch(const std::runtime_error&){rejected=true;} assert(rejected && resolutions==0); }
 else { auto p=game::get_appdata_path(); assert(p.is_absolute()); assert(p==game::get_appdata_path()); if(mode==1){assert(p==std::filesystem::path("D:/candidate/own-data"));assert(resolutions==0 && frees==0);}else{assert(p.filename()=="s2x");assert(resolutions==1 && frees==1);} }
 std::cout<<"PASS AppData case "<<mode<<"\n";
}
'''.replace('FUNCTION',function)
(out/'appdata.cpp').write_text(code)
project=(root/'tests/scripting/storage.vcxproj').read_text().replace('$(ProjectDir)..\\..\\build\\tests\\scripting\\bin\\','$(ProjectDir)bin\\').replace('$(ProjectDir)..\\..\\build\\tests\\scripting\\obj\\','$(ProjectDir)obj\\').replace('storage.cpp','appdata.cpp')
(out/'appdata.vcxproj').write_text(project)
msbuild=r'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
subprocess.run([msbuild,str(out/'appdata.vcxproj'),'/p:Configuration=Release','/p:Platform=x64','/m:1','/v:minimal','/nologo'],env={k.upper():v for k,v in os.environ.items()},check=True)
for mode in range(4): subprocess.run([str(out/'bin/appdata.exe'),str(mode)],check=True)
