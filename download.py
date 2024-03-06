import subprocess
from zipfile import ZipFile
import os
from pathlib import Path
import shutil
import gdown

stubs = "https://drive.google.com/drive/folders/18V75kGAbsqmTguGSrRenk6zouoe14RHH?usp=sharing"
box = "https://drive.google.com/drive/folders/1FOFbhqplR9viJz7jz0kDTYDd7ufWaXap?usp=sharing"
gdown.download_folder(url=stubs, output="assets/stubs", quiet=False, use_cookies=False)
gdown.download_folder(url=box, output="assets/box", quiet=False, use_cookies=False)

subprocess.run('gdown --id 1rwL_Mt9m0ZcOHpa3xShkGrhexFr4SqA2')

with ZipFile("resources.zip") as file_zip:
	file_zip.extractall("Unpacked")

directory = 'Unpacked'

for source_dir in os.listdir(directory):
	sourse_path = r'{}'.format(directory + "\\" + source_dir)
	print('sourse_path = ' + str(sourse_path))

	dir_in_lower_case = source_dir.lower()
	print(dir_in_lower_case)

	main_path = r'{}'.format(Path(source_dir).parent.absolute())

	target_dir = main_path + "\\samples\\" + dir_in_lower_case + "\\assets"
	print(target_dir)

	if os.path.isdir(target_dir):
		shutil.rmtree(target_dir)
	print('moving from '+ str(sourse_path) + ' to ' + str(target_dir))
	os.rename(sourse_path, target_dir)

os.remove("resources.zip")
os.rmdir("Unpacked")