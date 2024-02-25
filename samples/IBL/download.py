import gdown

IBL = "https://drive.google.com/drive/folders/1lbJ1gLaa6ihK4jtYKwa75TU3_qNFz5pv?usp=sharing"
gdown.download_folder(url=IBL, output="assets/", quiet=False, use_cookies=False)
