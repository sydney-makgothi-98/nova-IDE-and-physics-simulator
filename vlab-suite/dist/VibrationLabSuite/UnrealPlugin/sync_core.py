"""Copies the engine core (core/include/vlab, core/src) into the plugin so the plugin builds the same, validated code as the desktop app.
Run this after every change to core/:   python ue5/sync_core.py"""
import os, shutil
here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(here)
dst = os.path.join(here, "VLab", "Source", "VLabRuntime", "ThirdParty")
for sub in ("include", "src"):
    shutil.rmtree(os.path.join(dst, sub), ignore_errors=True)
shutil.copytree(os.path.join(root, "core", "include"), os.path.join(dst, "include"))
os.makedirs(os.path.join(dst, "src"), exist_ok=True)
for f in os.listdir(os.path.join(root, "core", "src")):
    shutil.copy(os.path.join(root, "core", "src", f), os.path.join(dst, "src", f))
print("core synced into", dst)
