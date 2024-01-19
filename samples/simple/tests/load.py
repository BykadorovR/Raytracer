import subprocess
import os

os.chdir("../build")
error = False
iteration = 0
while error == False:
	try:		
		output = subprocess.check_output("Simple.exe", timeout=5)
	except subprocess.TimeoutExpired:
		continue
	except subprocess.CalledProcessError:
		error = True
		print("Application crashed")
	finally:
		print(f"iteration number {iteration}")
		iteration += 1

