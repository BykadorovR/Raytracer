import gdown

shape = "https://drive.google.com/drive/folders/1FOrHziuHYXzvVFmpuMu11r7oLQreagau?usp=sharing"
sprite = "https://drive.google.com/drive/folders/1DDqN1h_3FEjIYjyaP7ou1lvynHzDp2Wm?usp=sharing"
gdown.download_folder(url=shape, output="assets/", quiet=False, use_cookies=False)
gdown.download_folder(url=sprite, output="assets/", quiet=False, use_cookies=False)