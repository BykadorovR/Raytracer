import gdown

scene = "https://drive.google.com/drive/folders/1wDdL3csI_1olW4U0GHYZ0r2ozGj-lgKs?usp=sharing"
gdown.download_folder(url=scene, output="assets/", quiet=False, use_cookies=False)
