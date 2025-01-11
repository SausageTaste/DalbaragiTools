import os

from PIL import Image


def main():
    root_path = r"C:\Users\woos8\Desktop"

    albedo_images = set()
    opacity_images = set()

    for x in os.listdir(root_path):
        x_path = os.path.join(root_path, x)
        if x.endswith("_a.png"):
            albedo_images.add(x_path)
        elif x.endswith("_o.png"):
            opacity_images.add(x_path)

    for albedo_img_path in albedo_images:
        opacity_img_path = albedo_img_path.rstrip("_a.png") + "_o.png"
        if opacity_img_path not in opacity_images:
            raise FileNotFoundError(f"Opacity image not found: {repr(opacity_img_path)}")

        albedo_img = Image.open(albedo_img_path)
        opacity_img = Image.open(opacity_img_path)

        opacity_img = opacity_img.convert('L')
        albedo_img.putalpha(opacity_img)

        # Save the image
        output_path = albedo_img_path.rstrip("_a.png") + "_ao.png"
        albedo_img.save(output_path)
        print(f"Saved {output_path}")


if __name__ == "__main__":
    main()
