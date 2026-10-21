"""Copy the loading-screen backgrounds from the extracted Wuwa UI resources and generate the bottom shade.

Output: ArtSource/UI/Loading/Textures/*.png, which -run=WuwaLoadingAssetSetup imports into /Game/LGUI/Loading/Textures.
The backgrounds are extracted game assets: keep them local, never commit them (same rule as the HUD sprites).
"""
import shutil
from pathlib import Path

from PIL import Image

SOURCE = Path(r"C:\GamePakExtractor\WutheringWaves-UIResources-3.6\UIResources\Common\Image\BgCg\Loading")
OUT = Path(__file__).resolve().parent / "Textures"

BACKGROUNDS = {
    "T_Loading_Bg_01": "T_BgloadingLahailuo05.png",
    "T_Loading_Bg_02": "T_Bgloading03502.png",
    "T_Loading_Bg_03": "T_Bgloading03506.png",
    "T_Loading_Bg_04": "T_Bgloading03519.png",
    "T_Loading_Bg_05": "T_BgloadingLahailuo09.png",
}


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for name, source in BACKGROUNDS.items():
        shutil.copyfile(SOURCE / source, OUT / f"{name}.png")
        print(f"{name} <- {source}")

    # Bottom shade: black, transparent at the top, about 85% opaque at the bottom (eased), like the reference.
    width, height = 4, 256
    shade = Image.new("RGBA", (width, height))
    for y in range(height):
        t = y / (height - 1)
        alpha = int(255 * 0.85 * (t * t * (3 - 2 * t)))
        for x in range(width):
            shade.putpixel((x, y), (0, 0, 0, alpha))
    shade.save(OUT / "T_Loading_BottomShade.png")
    print("T_Loading_BottomShade generated")


if __name__ == "__main__":
    main()
