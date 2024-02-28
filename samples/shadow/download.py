import gdown

shape = "https://drive.google.com/drive/folders/1FOrHziuHYXzvVFmpuMu11r7oLQreagau?usp=sharing"
gdown.download_folder(url=shape, output="assets/", quiet=False, use_cookies=False)
