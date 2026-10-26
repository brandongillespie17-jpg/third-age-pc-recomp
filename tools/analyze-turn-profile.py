#!/usr/bin/env python3
"""Summarize Third Age diagnostic CSV without treating instruction samples as CPU time."""
import argparse
from collections import Counter,defaultdict
import csv,json,math
from pathlib import Path

PHASES=['work_devices_us','render_copy_us','present_us','audio_push_us','input_us','pacer_us','probe_us']
CONTROL={-1:'unavailable',0:'transition',1:'human input gate',2:'AI gate'}
PERIOD_US=280896/16777216*1e6

def quantile(values,q):
    values=sorted(values)
    return values[min(len(values)-1,max(0,math.ceil(q*len(values))-1))] if values else None

def stats(rows):
    return {k:{'median_us':quantile([r[k] for r in rows],.5),'p95_us':quantile([r[k] for r in rows],.95),'max_us':max((r[k] for r in rows),default=0)} for k in PHASES}

def analyze(path):
    with Path(path).open(newline='') as f:
        rows=[{k:int(v) for k,v in row.items()} for row in csv.DictReader(f)]
    if len(rows)<2:raise ValueError('Need at least two recorded frames; close the game normally after playing.')
    required=set(PHASES)|{'frame','cycles','wall_ns','pc_samples','pc_dropped','valid','scene','next_scene','player','control_gate','setup_phase_raw','action','presented','fast_forward'}
    if not required.issubset(rows[0]):raise ValueError('Unsupported diagnostic CSV schema.')
    # The first row has no preceding timestamp and may contain startup counts.
    usable=[r for r in rows[1:] if r['presented'] and not r['fast_forward']]
    groups=defaultdict(list)
    for r in usable:groups[(r['scene'],r['control_gate'],r['setup_phase_raw'])].append(r)
    segments=[]
    for r in rows:
        key=(r['scene'],r['next_scene'],r['player'],r['control_gate'],r['setup_phase_raw'],r['action'])
        if not segments or key!=segments[-1]['key']:
            segments.append({'key':key,'start_frame':r['frame'],'end_frame':r['frame'],'start_cycles':r['cycles'],'end_cycles':r['cycles'],'rows':1})
        else:
            s=segments[-1];s['end_frame']=r['frame'];s['end_cycles']=r['cycles'];s['rows']+=1
    buckets=Counter();samples=sum(r['pc_samples'] for r in usable)
    for r in usable:
        for i in range(4):buckets[r[f'pc{i}']]+=r[f'count{i}']
    gaps=[];resets=0
    for a,b in zip(rows,rows[1:]):
        if b['frame']<a['frame'] or b['cycles']<a['cycles']:resets+=1
        elif b['frame']-a['frame']>1:gaps.append({'after_frame':a['frame'],'frame_step':b['frame']-a['frame'],'fast_forward':bool(b['fast_forward'])})
    active=lambda r:sum(r[k] for k in PHASES if k!='pacer_us')
    # Newer builds identify the completed image separately from the live PPU
    # wrap counter. The pre-present wait must be added to reconstruct call times.
    image_clock=bool(rows[-1].get('completed_image_clock',0))
    present_times=[r['wall_ns']+(r['probe_us']+r['render_copy_us']+r.get('wait_before_present_us',0))*1000 for r in rows if r['presented'] and not r['fast_forward']]
    intervals=[(b-a)/1e6 for a,b in zip(present_times,present_times[1:])]
    latch_steps=[b['vblank']-a['vblank'] for a,b in zip(rows,rows[1:]) if 'vblank' in a and 'vblank' in b and b['vblank']>=a['vblank']]
    presentation={'frame_identity':'completed image' if image_clock else 'live PPU wrap counter',
        'estimated_call_interval_ms':{'median':quantile(intervals,.5),'p95':quantile(intervals,.95),'p99':quantile(intervals,.99),'max':max(intervals,default=0)},
        'repeated_latch_rows':sum(v==0 for v in latch_steps) if latch_steps else None,
        'skipped_latch_count':sum(max(0,v-1) for v in latch_steps) if latch_steps else None}
    summary={'format':'third-age-turn-profile-v1','presentation':presentation,'rows':len(rows),'normal_rows':len(usable),'first_frame':rows[0]['frame'],'last_frame':rows[-1]['frame'],
        'captured_wall_seconds':(rows[-1]['wall_ns']-rows[0]['wall_ns'])/1e9,'nominal_frame_budget_us':PERIOD_US,
        'phases':stats(usable),'over_budget_active_rows':sum(active(r)>PERIOD_US for r in usable),
        'long_input_rows_over_100ms':sum(r['input_us']>100000 for r in usable),'frame_gaps':gaps,'timeline_resets':resets,
        'contexts':[{'scene':k[0],'control':CONTROL.get(k[1],'unknown'),'setup_phase_raw':k[2],'rows':len(v),'phases':stats(v)} for k,v in groups.items()],
        'segments':segments,'pc_samples':samples,'pc_samples_dropped':sum(r['pc_dropped'] for r in usable),
        'top_pc_buckets':[{'address':f'0x{k:08x}','retained_samples':v,'share_of_all_samples_pct':round(100*v/samples,2) if samples else 0} for k,v in buckets.most_common(16) if v],
        'limitations':['Presentation-call intervals are estimated from timestamps, not display scanout. Fast-forward or save-state jumps require separate interpretation.',
        'PC buckets are main-thread instruction-frequency samples, not wall-time percentages; nested IRQ/native/fallback internals are not sampled.',
        'Only the top four 256-byte PC buckets per frame are retained; aggregate bucket counts are lower bounds.',
        'Work/devices includes interpreter execution, device emulation, scanline composition, and intervening loop work.',
        'Input time can include an open paused overlay; long input rows are flagged, not silently removed.',
        'Control is the known input/AI gate, not a universally decoded turn-owner identity. Setup/action fields remain raw.',
        'Up to 32768 latest rows are retained; runtime.log reports older rows dropped. No independent proof of a scripted delay comes from unchanged scene alone.']}
    return summary

def markdown(s):
    lines=['# Third Age turn diagnostics','',f"Captured {s['rows']} rows over {s['captured_wall_seconds']:.2f} wall-clock seconds.",f"Normal-speed rows: {s['normal_rows']}. Nominal guest-frame budget: {s['nominal_frame_budget_us']/1000:.3f} ms.",'',
        '| Phase | Median ms | P95 ms | Maximum ms |','|---|---:|---:|---:|']
    for k,v in s['phases'].items():
        fmt=lambda x:'n/a' if x is None else f'{x/1000:.3f}'
        lines.append(f"| {k} | {fmt(v['median_us'])} | {fmt(v['p95_us'])} | {fmt(v['max_us'])} |")
    lines+=['',f"Active work exceeds the nominal budget in {s['over_budget_active_rows']} rows. This is measured on the machine that made this capture, and is not a benchmark of other hardware.",f"Observed frame gaps: {len(s['frame_gaps'])}; timeline resets: {s['timeline_resets']}; input/pause intervals above 100 ms: {s['long_input_rows_over_100ms']}.",'','## Observed contexts','','| Scene | Control gate | Setup phase (raw) | Rows | Work/devices P95 ms |','|---|---|---:|---:|---:|']
    for c in s['contexts']:lines.append(f"| {c['scene']} | {c['control']} | {c['setup_phase_raw']} | {c['rows']} | {c['phases']['work_devices_us']['p95_us']/1000:.3f} |")
    delivery=s['presentation'];cadence=delivery['estimated_call_interval_ms']
    lines+=['','## Presentation clock','',f"Frame identity: {delivery['frame_identity']}. Repeated latch rows: {delivery['repeated_latch_rows']}; skipped latches: {delivery['skipped_latch_count']}.",f"Estimated present-call interval (ms): median {cadence['median']}, P95 {cadence['p95']}, P99 {cadence['p99']}. These are not display scanout measurements."]
    lines+=['','## Sampled code locations','','These are instruction-frequency samples, not time percentages. Addresses denote 256-byte buckets.','']
    for b in s['top_pc_buckets']:lines.append(f"- {b['address']}: {b['retained_samples']} retained samples ({b['share_of_all_samples_pct']}% of all samples).")
    lines+=['','## Interpretation limits','']+['- '+x for x in s['limitations']]
    return '\n'.join(lines)+'\n'

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('profile',type=Path);p.add_argument('--output',type=Path)
    a=p.parse_args();out=a.output or a.profile.with_suffix('.summary.json');s=analyze(a.profile)
    out.write_text(json.dumps(s,indent=2)+'\n');out.with_suffix('.md').write_text(markdown(s));print(out.with_suffix('.md'))
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError) as e:raise SystemExit(f'Analysis stopped: {e}')
