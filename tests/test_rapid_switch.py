#!/usr/bin/env python3
import json
import subprocess
import time
import sys
import os

YABAI_BIN = "/opt/homebrew/bin/yabai"
LOG_FILE = "/tmp/yabai_main.out.log"

def run_cmd(cmd):
    p = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return p.stdout.strip(), p.stderr.strip(), p.returncode

def get_spaces():
    out, _, code = run_cmd(f"{YABAI_BIN} -m query --spaces")
    if code != 0:
        return []
    return json.loads(out)

def get_active_space():
    out, _, code = run_cmd(f"{YABAI_BIN} -m query --spaces --space")
    if code != 0:
        return None
    return json.loads(out)

def get_windows():
    out, _, code = run_cmd(f"{YABAI_BIN} -m query --windows")
    if code != 0:
        return []
    return json.loads(out)

def get_frontmost_app():
    out, _, code = run_cmd(f"{YABAI_BIN} -m query --windows --window")
    if code == 0 and out:
        try:
            return json.loads(out).get('app', '')
        except Exception:
            pass
    return ""

def activate_app(app_name):
    run_cmd(f'open -a "{app_name}"')

def read_log_slice(start_pos):
    if not os.path.exists(LOG_FILE):
        return "", start_pos
    with open(LOG_FILE, "r", encoding="utf-8", errors="replace") as f:
        f.seek(start_pos)
        data = f.read()
        new_pos = f.tell()
    return data, new_pos

def main():
    print("=" * 60)
    print("  YABAI RAPID SPACE SWITCHING DIAGNOSTIC TEST")
    print("=" * 60)

    # 1. Inspect current layout
    windows = get_windows()
    spaces = get_spaces()
    current_space = get_active_space()

    print(f"Total spaces: {len(spaces)}")
    for s in spaces:
        print(f"  Space index {s['index']} (id={s['id']}): visible={s['is-visible']}, focus={s['has-focus']}")

    # Find candidate apps across different spaces
    app_by_space = {}
    for w in windows:
        app = w['app']
        sp = w['space']
        # filter out System Settings or dialogs
        if w.get('role') == 'AXWindow' and not w.get('is-minimized') and not w.get('is-sticky'):
            if sp not in app_by_space:
                app_by_space[sp] = app

    print("\nApplications detected on spaces:")
    for sp, app in sorted(app_by_space.items()):
        print(f"  Space {sp}: {app}")

    if len(app_by_space) < 2:
        print("\nERROR: Need at least 2 non-sticky apps on different spaces to test switching.")
        return

    if len(sys.argv) >= 3:
        app1 = sys.argv[1]
        app2 = sys.argv[2]
        sp1 = next((w['space'] for w in windows if w['app'].lower() == app1.lower()), 1)
        sp2 = next((w['space'] for w in windows if w['app'].lower() == app2.lower()), 4)
    else:
        # Pick two apps on different spaces
        sorted_spaces = sorted(app_by_space.keys())
        sp1 = sorted_spaces[0]
        sp2 = sorted_spaces[-1]
        app1 = app_by_space[sp1]
        app2 = app_by_space[sp2]

    print(f"\nTest Pair: [{app1} on Space {sp1}] <---> [{app2} on Space {sp2}]")

    # Start log tracking
    log_pos = os.path.getsize(LOG_FILE) if os.path.exists(LOG_FILE) else 0

    delays = [0.8, 0.4, 0.2, 0.1]
    iterations_per_delay = 3

    total_tests = 0
    first_press_success = 0
    failures = []

    print("\nStarting rapid switching test series...")
    print("-" * 60)

    for delay in delays:
        print(f"\n--- Testing with switch interval: {int(delay * 1000)}ms ---")
        for i in range(iterations_per_delay):
            target_app = app2 if (i % 2 == 0) else app1
            target_sp = sp2 if (i % 2 == 0) else sp1

            total_tests += 1
            test_start = time.time()
            log_start_pos = log_pos

            # Activate target app
            activate_app(target_app)

            # Poll space and front app for up to 1.2 seconds
            switched_space = False
            front_app_switched = False
            space_switch_time = None
            front_switch_time = None

            deadline = time.time() + 1.2
            while time.time() < deadline:
                cur_sp_info = get_active_space()
                cur_sp_idx = cur_sp_info['index'] if cur_sp_info else None
                front_app = get_frontmost_app()

                now = time.time()
                elapsed_ms = int((now - test_start) * 1000)

                if not front_app_switched and front_app.lower() == target_app.lower():
                    front_app_switched = True
                    front_switch_time = elapsed_ms

                if not switched_space and cur_sp_idx == target_sp:
                    switched_space = True
                    space_switch_time = elapsed_ms
                    break

                time.sleep(0.015)

            # Read log slice
            log_slice, log_pos = read_log_slice(log_start_pos)
            log_lines = [l for l in log_slice.strip().split("\n") if l and not l.startswith("EVENT_HANDLER_MOUSE")]

            status = "PASS" if switched_space else "FAIL (Space NOT switched on 1st attempt)"
            if switched_space:
                first_press_success += 1
                print(f"  Attempt {total_tests} -> {target_app} (Space {target_sp}): {status} in {space_switch_time}ms (front_app in {front_switch_time}ms)")
            else:
                print(f"  Attempt {total_tests} -> {target_app} (Space {target_sp}): {status}!")
                print(f"    Frontmost App: {front_app} (front switched: {front_app_switched})")
                print(f"    Current Space: {get_active_space()['index']} (Expected: {target_sp})")
                print(f"    Yabai Log during attempt:")
                for l in log_lines[-10:]:
                    print(f"      {l}")
                failures.append({
                    "attempt": total_tests,
                    "target_app": target_app,
                    "target_sp": target_sp,
                    "actual_sp": get_active_space()['index'],
                    "front_app": front_app,
                    "logs": log_lines
                })

                # Test recovery: how many extra presses needed?
                extra_presses = 0
                for retry in range(1, 4):
                    extra_presses += 1
                    time.sleep(0.3)
                    activate_app(target_app)
                    time.sleep(0.3)
                    cur_sp = get_active_space()['index']
                    if cur_sp == target_sp:
                        print(f"    -> Switched space after {extra_presses} extra activation(s)!")
                        break

            time.sleep(delay)

    print("\n" + "=" * 60)
    print("  TEST SUMMARY & STATISTICS")
    print("=" * 60)
    print(f"Total switch attempts: {total_tests}")
    print(f"First-press successes: {first_press_success} ({int(first_press_success / total_tests * 100)}%)")
    print(f"First-press failures:  {len(failures)} ({int(len(failures) / total_tests * 100)}%)")

    if failures:
        print("\nFAILURE ANALYSIS:")
        for f in failures:
            print(f"  * Attempt #{f['attempt']} to {f['target_app']} (Space {f['target_sp']}):")
            print(f"    Stayed on Space {f['actual_sp']} while front was {f['front_app']}")
            for l in f['logs']:
                if "gesture" in l.lower() or "focus" in l.lower() or "space" in l.lower() or "skip" in l.lower():
                    print(f"    log: {l}")
    else:
        print("\nAll switches succeeded on 1st attempt.")

if __name__ == "__main__":
    main()
