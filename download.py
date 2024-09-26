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
	source_path = r'{}'.format(directory + "\\" + source_dir)

	dir_in_lower_case = source_dir.lower()
	print(dir_in_lower_case)

	main_path = r'{}'.format(Path(source_dir).parent.absolute())

	target_dir = main_path + "\\samples\\" + dir_in_lower_case + "\\assets"

	if os.path.isdir(target_dir):
		shutil.rmtree(target_dir)
	print('moving from '+ str(source_path) + ' to ' + str(target_dir))
	os.rename(source_path, target_dir)

# Android uses the same resources as Scene sample
files_to_copy = ['assets/', 'samples/sprite/assets/', 'samples/IBL/assets/', 'samples/terrain/assets', 'samples/particles/assets', 'samples/scene/assets/Skybox', 'samples/models/BrainStem']
def copytree(src, dst, symlinks=False, ignore=None):
    for item in os.listdir(src):
        s = os.path.join(src, item)
        d = os.path.join(dst, item)
        if os.path.isdir(s):
            if not os.path.exists(d):
                shutil.copytree(s, d, symlinks, ignore)
        else:
            shutil.copy2(s, d)

assets_folder = 'samples/android/app/src/main/assets'
if not os.path.exists(assets_folder):
        os.makedirs(assets_folder)
for item in files_to_copy:
	copytree(item, assets_folder)

print("Neccessary files have been copied to Android sample folder")

os.remove("resources.zip")
os.rmdir("Unpacked")