import sys
import os
import subprocess

def before_build(source, target, env):
    html = os.path.join(env['PROJECT_DIR'], 'main', 'gamepad.html')
    inc = os.path.join(env['PROJECT_DIR'], 'main', 'gamepad_html.inc')
    script = os.path.join(env['PROJECT_DIR'], 'romconv', 'html2c.py')
    if os.path.exists(html):
        print(f"[extra_script] Generating {inc} from {html}")
        subprocess.run([sys.executable, script, html, inc], check=True)

def pre_build_action(*args, **kwargs):
    before_build(*args, **kwargs)

Import("env")
env.AddPreAction("buildprog", pre_build_action)
