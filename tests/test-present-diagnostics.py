#!/usr/bin/env python3
"""Isolated diagnostic tests; writes artifacts only to the requested output dir."""
import argparse,csv,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
headers=root/'vendor/gbarecomp/src/runtime';baseline=a.baseline.resolve()/'vendor/gbarecomp/src/runtime'
def compile_run(name,source,inc,args=(),defines=()):
 cmd=['c++','-std=c++20','-O2',*defines,'-I',str(inc),str(source),'-o',str(out/name)]
 subprocess.run(cmd,check=True);r=subprocess.run([str(out/name),*map(str,args)],check=True,capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);return r.stdout
old=compile_run('camera-baseline',root/'tests/present-diagnostics.cpp',baseline)
disabled=compile_run('camera-disabled',root/'tests/present-diagnostics.cpp',headers)
assert old==disabled,'Disabled diagnostic changed SDL calls, bytes or geometry'
new=compile_run('camera-diagnostic',root/'tests/present-diagnostics.cpp',headers,defines=['-DDIAGNOSTIC_VERSION'])
assert old==new,'SDL call sequence, texture bytes or draw geometry changed'
trace=out/'ring.csv';trace.unlink(missing_ok=True)
compile_run('ring',root/'tests/diagnostic-ring.cpp',headers,[trace])
with trace.open() as f: rows=list(csv.DictReader(f))
assert len(rows)==65536 and int(rows[0]['sequence'])==4 and int(rows[-1]['sequence'])==65539
assert all(None not in r for r in rows)
# Isolate the actual runtime's frame-phase structs. This tests both CSV modes
# without launching another game while the main test driver benchmarks it.
s=(headers/'runtime.cpp').read_text();fragment=s[s.index('    struct FramePhaseSample {'):s.index('    FramePhaseRing frame_phase(')]
source=out/'phase.cpp';source.write_text('''#include <algorithm>\n#include <chrono>\n#include <cstdio>\n#include <cstdlib>\n#include <cstdint>\n#include <string>\n#include <vector>\nunsigned long long overlay_game_thread_compile_ns(){return 0;}\nint main(int argc,char** argv){\n'''+fragment+'''
setenv("GBARECOMP_FRAME_PHASE",argv[1],1);FramePhaseRing ring(argc>2);
ring.record(1,100,110,120,130,140,150,0,0,0,0);
ring.record(2,300,310,320,330,340,350,0,20,25,7);
ring.dump();}\n''')
compile_run('phase',source,headers,[out/'phase-off.csv'])
subprocess.run([str(out/'phase'),str(out/'phase-on.csv'),'on'],check=True,capture_output=True)
with (out/'phase-off.csv').open() as f: legacy=csv.DictReader(f);assert legacy.fieldnames==['frame','guest_us','render_us','present_us','audio_us','pump_us','pacer_us','compile_us']
with (out/'phase-on.csv').open() as f: phases=list(csv.DictReader(f))
assert phases[0]['guest_interval_valid']=='0'
assert phases[1]['guest_interval_valid']=='1' and int(phases[1]['guest_interval_ns'])==150 and int(phases[1]['guest_service_ns'])==20 and int(phases[1]['guest_exclusive_ns'])==130 and int(phases[1]['pacer_service_ns'])==7
result={'camera_sdl_calls_texture_bytes_geometry_equivalent':True,'camera_disabled_equivalent':True,'snap_validity_and_failure_masks':True,'ring_opt_in_no_early_write_wrap_schema':True,'phase_legacy_header_unchanged':True,'phase_exclusive_accounting_and_first_interval_gate':True}
(out/'unit-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
