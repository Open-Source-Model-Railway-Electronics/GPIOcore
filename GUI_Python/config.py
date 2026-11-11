import json

def save_config(data, filename="config.json"):
    with open(filename, "w") as f:
        json.dump(data, f)

def load_config(filename="config.json"):
    with open(filename, "r") as f:
        return json.load(f)
