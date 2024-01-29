import gdown

simple = "https://drive.google.com/drive/folders/1pLKZnjJjbPgHXUCljtAIqXK9smr2a4ZE?usp=sharing"
gdown.download_folder(url=simple, output="assets/", quiet=False, use_cookies=False)
