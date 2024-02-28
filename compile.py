import os
import subprocess
import sys


folder = "shaders"
compiler_path = sys.argv[1]
debug = True

files = [os.path.join(dp, f) for dp, dn, fn in os.walk(os.path.expanduser(folder)) for f in fn]

for f in files:
	file_name = os.path.splitext(f)[0]
	print(file_name)
	extension = os.path.splitext(f)[1][1:]
	extension_new = ""
	if extension == "frag":
		extension_new = "_fragment.spv"
	elif extension == "vert":
		extension_new = "_vertex.spv"
	elif extension == "comp":
		extension_new = "_compute.spv"
	elif extension == "tese":
		extension_new = "_evaluation.spv"
	elif extension == "tesc":
		extension_new = "_control.spv"
	elif extension == "geom":
		extension_new = "_geometry.spv"
	else:
		continue

	debug_key = ""
	if debug:
		debug_key = "-g"
	command_line = f"{compiler_path} -c {debug_key} {f} -o {file_name}{extension_new}"
	#print(command_line)
	subprocess.call(command_line)
