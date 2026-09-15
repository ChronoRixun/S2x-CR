"""Run the existing local HQ harness plus round-one tests outside build/."""
from pathlib import Path
import re
import shutil
import subprocess
import sys

repo = Path(__file__).resolve().parents[1]
relay = (repo / "src/client/component/hidden_challenge_relay.cpp").read_text()
relay = relay[relay.index("\t\tvoid relay_test_command"):relay.index("\t\tvoid clear_pending_forwards")]
assert relay.index("params.size() < 2") < relay.index('Dvar_FindMalleableVar("sv_cheats")') < relay.index("party_xuids()")
assert "params.size() > 4" in relay and "target slot %zu" in relay
assert "local XUID %" not in relay and "party member XUID %" not in relay and "XUID %llu" not in relay
print("PASS: relay source checks: validate arguments before gate/party access; no XUID formatting", flush=True)
output = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else repo.parent / "S2x-round1-harness"
if output == repo or repo in output.parents:
    raise SystemExit("Use an output directory outside the repository")
harness = output / "hq-tests"
harness.mkdir(parents=True, exist_ok=True)
original = repo / "build/research/hq-tests"
shutil.copytree(original / "shim", harness / "shim", dirs_exist_ok=True)
tests = (original / "tests.cpp").read_text()
for relative in set(re.findall(r'"(\.\./[^"\n]+)"', tests)):
    relative = relative.removeprefix("../").removeprefix("../")
    source = repo / "build/research" / relative
    destination = output / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)
extension = (repo / "tools/hq-economy-round1-tests.hpp").as_posix()
tests = tests.replace("int main() {", f'#include "{extension}"\nint main() {{')
tests = tests.replace(' std::ofstream("players2/user/hq_economy.json") << "corrupt";',
                      ' round1_receipt_tests();\n round1_log_and_capacity_tests();\n round1_replay_tests();\n'
                      ' std::ofstream("players2/user/hq_economy.json") << "corrupt";')
# A distinct run directory also makes repeated invocations safe if Windows reuses a PID.
tests = tests.replace('std::to_string(GetCurrentProcessId())',
                      'std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64())')
(harness / "tests.cpp").write_text(tests)
(harness / "shim/component/console/console.hpp").write_text('''#pragma once
#include <cstdio>
namespace console {
inline std::vector<std::string> errors, warnings, infos;
template<class... T> void capture(std::vector<std::string>& out, const char* fmt, T... args) {
 char text[4096]{}; std::snprintf(text, sizeof(text), fmt, args...); out.emplace_back(text);
}
template<class... T> void error(const char* fmt, T... args) { capture(errors, fmt, args...); }
template<class... T> void warn(const char* fmt, T... args) { capture(warnings, fmt, args...); }
template<class... T> void info(const char* fmt, T... args) { capture(infos, fmt, args...); }
template<class... T> void debug(const char*, T...) {}
}
''')
project = (original / "hq-tests.vcxproj").read_text()
project = project.replace('$(ProjectDir)..\\..\\..\\', str(repo) + '\\')
project = project.replace('../../../src/', (repo / "src").as_posix() + '/')
(harness / "hq-tests.vcxproj").write_text(project)
scheduler = (repo / "src/client/component/scheduler.cpp").read_text()
pipeline = scheduler[scheduler.index("namespace scheduler"):scheduler.index("\t\tvolatile bool kill")]
start = scheduler.index("\tvoid schedule(")
submission = scheduler[start:scheduler.index("\tclass component final", start)]
(harness / "scheduler-under-test.inc").write_text(
    pipeline + '\t\ttask_pipeline pipelines[pipeline::count];\n\t}\n' + submission + '}\n')
start = project.index('<ItemGroup><ClCompile Include="tests.cpp"')
end = project.index('</ItemGroup>', start) + len('</ItemGroup>')
scheduler_project = (project[:start] + '<ItemGroup><ClCompile Include="' +
                     (repo / 'tools/scheduler-round1-tests.cpp').as_posix() + '"/></ItemGroup>' + project[end:])
scheduler_project = scheduler_project.replace('$(ProjectDir)shim;', '$(ProjectDir);$(ProjectDir)shim;')
scheduler_project = scheduler_project.replace('$(ProjectDir)obj\\', '$(ProjectDir)scheduler-obj\\')
(harness / "scheduler-tests.vcxproj").write_text(scheduler_project)

bash = "C:/Program Files/Git/bin/bash.exe"
msbuild = "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"
for name in ("scheduler-tests", "hq-tests"):
    command = f'"{msbuild}" "{(harness / (name + ".vcxproj")).as_posix()}" -m -v:minimal -nologo -p:Configuration=Release -p:Platform=x64'
    with (output / (name + "-build.log")).open("w") as log:
        subprocess.run([bash, "-lc", command], cwd=repo, stdout=log, stderr=subprocess.STDOUT, check=True)
    with (output / (name + "-run.log")).open("w") as log:
        subprocess.run([str(harness / "bin" / (name + ".exe"))], cwd=harness,
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    print((output / (name + "-run.log")).read_text(), end="")
