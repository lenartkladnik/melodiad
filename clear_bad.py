import requests

playlist = input("Playlist name: ")
path = f".music_data/playlists/{playlist}"

good_tracks = []

with open(path, "r") as f:
    tracks = f.read().split("\n")

for track in tracks:
    if requests.get(f"http://localhost:5400/song?song_id={track}").content == "{}":
        continue

    good_tracks.append(track)

with open(path + "-fixed", "x") as f:
    f.write("\n".join(good_tracks))
