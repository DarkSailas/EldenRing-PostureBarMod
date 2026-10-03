import os
import subprocess
import sys
import shutil

DIR = os.path.dirname(os.path.abspath(__file__))
SOURCE_DIR = os.path.join(DIR, "Source")
BUILD_DIR = os.path.join(DIR, "build")
OUT_DLL = os.path.join(DIR, "PostureBarMod.dll")

os.makedirs(BUILD_DIR, exist_ok=True)

cpp_files = []
c_files = []
for dirpath, _, filenames in os.walk(SOURCE_DIR):
    for f in filenames:
        if f.endswith('.cpp'):
            cpp_files.append(os.path.join(dirpath, f))
        elif f.endswith('.c'):
            c_files.append(os.path.join(dirpath, f))

inc_dirs = [
    SOURCE_DIR,
    os.path.join(SOURCE_DIR, "Main"),
    os.path.join(SOURCE_DIR, "ImGui"),
    os.path.join(SOURCE_DIR, "Minhook"),
    os.path.join(SOURCE_DIR, "DirectX"),
    os.path.join(SOURCE_DIR, "Ini"),
    os.path.join(SOURCE_DIR, "Stb"),
]
inc_flags = " ".join([f'-I"{d}"' for d in inc_dirs])

print(f"Building PostureBarMod ({len(cpp_files)} C++, {len(c_files)} C files)...")

errors = 0
obj_files = []

for f in cpp_files:
    rel = os.path.relpath(f, SOURCE_DIR)
    obj_name = rel.replace('\\', '_').replace('/', '_').replace('.cpp', '.o')
    obj_path = os.path.join(BUILD_DIR, obj_name)
    obj_files.append(obj_path)
    cmd = f'g++ -std=c++20 -O2 -c "{f}" -o "{obj_path}" {inc_flags}'
    res = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"FAIL: {rel}\n{res.stderr[:300]}")
        errors += 1
    else:
        print(f"  [CPP] {rel}")

for f in c_files:
    rel = os.path.relpath(f, SOURCE_DIR)
    obj_name = rel.replace('\\', '_').replace('/', '_').replace('.c', '.o')
    obj_path = os.path.join(BUILD_DIR, obj_name)
    obj_files.append(obj_path)
    cmd = f'gcc -O2 -c "{f}" -o "{obj_path}" {inc_flags}'
    res = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"FAIL: {rel}\n{res.stderr[:300]}")
        errors += 1
    else:
        print(f"  [C]   {rel}")

if errors > 0:
    print(f"\nBuild failed with {errors} compilation errors.")
    sys.exit(1)

print("\nLinking PostureBarMod.dll...")
libs = "-ld3d12 -ld3d11 -ldxgi -ld3dcompiler -luser32 -lkernel32 -limm32 -lgdi32 -ldwmapi -ldinput8 -ldxguid"
obj_args = " ".join([f'"{f}"' for f in obj_files])
link_cmd = f'g++ -shared -static -static-libgcc -static-libstdc++ -O2 -o "{OUT_DLL}" {obj_args} {libs}'
res = subprocess.run(link_cmd, shell=True, capture_output=True, text=True)

if res.returncode == 0:
    print(f"SUCCESS: Created {OUT_DLL} ({os.path.getsize(OUT_DLL)} bytes)")
    # Also copy to release/ if present
    rel_dll = os.path.join(DIR, "release", "PostureBarMod.dll")
    if os.path.exists(os.path.dirname(rel_dll)):
        shutil.copy2(OUT_DLL, rel_dll)
        print(f"Updated {rel_dll}")
else:
    print(f"Link failed:\n{res.stderr}")
    sys.exit(1)
