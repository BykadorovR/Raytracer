import gdown

terrain = "https://drive.google.com/drive/folders/1a4JxoMDICd7-y72V-OEyqGy4frCsHnNN?usp=sharing"
gdown.download_folder(url=terrain, output="assets/", quiet=False, use_cookies=False)