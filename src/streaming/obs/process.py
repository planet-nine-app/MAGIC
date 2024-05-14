import subprocess

print("Does this do anything")

python_bin = "/Users/zachbabb/Work/python/bin/python3.12"
script_file = "/Users/zachbabb/Work/planet-nine/MAGIC/src/streaming/obs/demo.py"

subprocess.Popen([python_bin, script_file])
