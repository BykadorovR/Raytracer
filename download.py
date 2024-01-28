import gdown

stubs = "https://drive.google.com/drive/folders/18V75kGAbsqmTguGSrRenk6zouoe14RHH?usp=sharing"
box = "https://drive.google.com/drive/folders/1FOFbhqplR9viJz7jz0kDTYDd7ufWaXap?usp=sharing"
gdown.download_folder(url=stubs, output="assets/stubs", quiet=False, use_cookies=False)
gdown.download_folder(url=box, output="assets/box", quiet=False, use_cookies=False)