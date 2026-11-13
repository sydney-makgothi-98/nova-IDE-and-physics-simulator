"""Builds the Vibration Lab Suite release: compiles, runs the validation suite, checks the Excel export, and assembles
dist/VibrationLabSuite-<version>-win64.zip (a single self-contained exe plus examples and documents).

    python tools/package.py            (run from anywhere; needs cmake, ninja, g++ on PATH; openpyxl is optional)
"""
import os, shutil, subprocess, sys, zipfile, datetime

VERSION = "1.1"
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BUILD = os.path.join(ROOT, "build")
DIST = os.environ.get("VLAB_DIST") or os.path.join(ROOT, "dist")
PKG = os.path.join(DIST, "VibrationLabSuite")


def run(cmd, **kw):
    print(">", " ".join(cmd) if isinstance(cmd, list) else cmd)
    r = subprocess.run(cmd, **kw)
    if r.returncode not in (0,) and not kw.get("allow_fail"):
        sys.exit(f"command failed ({r.returncode})")
    return r


def main():
    env = dict(os.environ)
    env.pop("ELECTRON_RUN_AS_NODE", None)
    run(["cmake", "-S", ROOT, "-B", BUILD, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_CXX_COMPILER=g++"], env=env)
    run(["cmake", "--build", BUILD], env=env)

    shutil.rmtree(PKG, ignore_errors=True)
    os.makedirs(os.path.join(PKG, "Examples"))
    os.makedirs(os.path.join(PKG, "Tools"))
    os.makedirs(os.path.join(PKG, "Documents"))

    # ---- validation suite: must pass, and its output ships as the validation report
    r = subprocess.run([os.path.join(BUILD, "vlab_tests.exe")], capture_output=True, text=True)
    report = r.stdout
    header = f"Vibration Lab Suite {VERSION} - validation report\nGenerated {datetime.datetime.now():%Y-%m-%d %H:%M}\n" + "=" * 70 + "\n\n"
    open(os.path.join(PKG, "Documents", "validation_report.txt"), "w", encoding="utf-8").write(header + report)
    print(report.strip().splitlines()[-1])
    if r.returncode != 0:
        sys.exit("validation suite FAILED: not packaging")

    # ---- executables (stripped to save space)
    strip = shutil.which("strip")
    for exe, dst in (("vlab_suite.exe", "VibrationLabSuite.exe"), ("vlab_cli.exe", os.path.join("Tools", "vlab_cli.exe")), ("vlab_tests.exe", os.path.join("Tools", "vlab_tests.exe"))):
        target = os.path.join(PKG, dst)
        shutil.copy(os.path.join(BUILD, exe), target)
        if strip:
            subprocess.run([strip, target], check=False)

    # ---- the example gallery as .vlab files
    cli = os.path.join(BUILD, "vlab_cli.exe")
    listing = subprocess.run([cli, "--list"], capture_output=True, text=True).stdout.splitlines()
    names = [l.split(None, 1)[1].strip() for l in listing if l[:3].strip().isdigit()]
    for i, n in enumerate(names, 1):
        safe = "".join(c if c.isalnum() or c in " -_()" else "" for c in n).strip().replace(" ", "_")
        subprocess.run([cli, "--example", str(i), "--save-design", os.path.join(PKG, "Examples", f"{i:02d}_{safe}.vlab")], capture_output=True)

    # ---- Excel export check on a fresh run (structure always; real charts if openpyxl is present)
    tmp = os.path.join(BUILD, "package_check.xlsx")
    subprocess.run([cli, "--example", "7", "--duration", "8", "--out", tmp], capture_output=True)
    try:
        import openpyxl
        wb = openpyxl.load_workbook(tmp)
        charts = sum(len(ws._charts) for ws in wb.worksheets)
        print(f"excel export check: sheets {wb.sheetnames}, charts {charts}")
        if charts < 3:
            sys.exit("excel export check failed: charts missing")
    except ImportError:
        print("openpyxl not installed: skipped the workbook check")

    # ---- documents
    guide = os.path.join(os.path.dirname(ROOT), "docs", "Vibration Lab Suite - User Guide.pdf")     # built by docs/src/build_suite_guide.py
    if os.path.exists(guide):
        shutil.copy(guide, os.path.join(PKG, "Documents", "Vibration Lab Suite - User Guide.pdf"))
    else:
        print("note: the user guide PDF was not found; run docs/src/build_suite_guide.py first")
    shutil.copy(os.path.join(ROOT, "docs", "README.txt"), os.path.join(PKG, "README.txt"))
    lic = []
    for title, path in (("Dear ImGui (MIT)", "third_party/imgui/LICENSE.txt"), ("ImPlot (MIT)", "third_party/implot/LICENSE")):
        p = os.path.join(ROOT, path)
        if os.path.exists(p):
            lic.append("=" * 70 + "\n" + title + "\n" + "=" * 70 + "\n" + open(p, encoding="utf-8", errors="replace").read())
    lic.append("=" * 70 + "\nzlib\n" + "=" * 70 + "\nzlib is (C) 1995-2024 Jean-loup Gailly and Mark Adler. It is provided 'as is', without any express or implied warranty; permission is granted to use it for any purpose, including commercial applications, provided the origin is not misrepresented and altered versions are marked.\n")
    open(os.path.join(PKG, "Documents", "THIRD_PARTY_LICENSES.txt"), "w", encoding="utf-8").write("\n".join(lic))
    # ---- Unreal plugin
    ue = os.path.join(ROOT, "ue5")
    if os.path.isdir(ue):
        shutil.copytree(ue, os.path.join(PKG, "UnrealPlugin"), ignore=shutil.ignore_patterns("__pycache__"))

    # ---- zip
    zpath = os.path.join(DIST, f"VibrationLabSuite-{VERSION}-win64.zip")
    if os.path.exists(zpath):
        os.remove(zpath)
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        for base, _, files in os.walk(PKG):
            for f in files:
                full = os.path.join(base, f)
                z.write(full, os.path.join("VibrationLabSuite", os.path.relpath(full, PKG)))
    exe = os.path.join(PKG, "VibrationLabSuite.exe")
    print(f"\nPACKAGED: {zpath}  ({os.path.getsize(zpath) / 1e6:.1f} MB)")
    print(f"          {exe}  ({os.path.getsize(exe) / 1e6:.1f} MB)")


if __name__ == "__main__":
    main()
