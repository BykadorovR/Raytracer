import subprocess
import os
from tempfile import TemporaryFile as Tmp
import re

os.chdir("../build")
error = False
iteration = 0
while error == False:
    with Tmp() as out, Tmp() as err:
        p_cmd = subprocess.Popen("Scene.exe", bufsize=0, stdin=None, stdout=out, stderr=err)
        timed_out = False
        try:
            p_cmd.wait(timeout=5)
        except subprocess.TimeoutExpired:
            timed_out = True
            p_cmd.kill()

        out.seek(0)
        err.seek(0)
        str_out = str(out.read())
        str_err = str(err.read())
        if p_cmd.returncode:
            error = True
            print(f"Application crashed: {p_cmd.returncode}")
        if timed_out:
            print(f"Iteration number {iteration}")
            iteration += 1
            
        if (re.search("Validation Error", str_err)):
            print(str_err)
            error = True