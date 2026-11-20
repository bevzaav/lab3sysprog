import glob
import json
import os
import subprocess
import sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
minic = os.path.join(root, "minic")
tmp = os.path.join(root, "build")
examples = []

for path in sorted(glob.glob(os.path.join(root, "examples", "*.c"))):
    name = os.path.basename(path)[:-2]
    json_path = os.path.join(tmp, name + ".json")
    py_path = os.path.join(tmp, name + ".py")
    subprocess.run([minic, path, "--json", json_path, "--py", py_path], check=True)
    run = subprocess.run([minic, path, "--run"], capture_output=True, text=True)
    examples.append({
        "name": name,
        "source": open(path).read(),
        "ast": json.load(open(json_path)),
        "python": open(py_path).read(),
        "output": run.stdout,
    })

out = os.path.join(root, "web", "data.js")
with open(out, "w") as f:
    f.write("window.MINIC_EXAMPLES = ")
    json.dump(examples, f, ensure_ascii=False)
    f.write(";\n")
print("wrote", out, "with", len(examples), "examples")
if "--open" in sys.argv:
    subprocess.run(["open", os.path.join(root, "web", "index.html")])
