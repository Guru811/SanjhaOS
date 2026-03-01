from PIL import Image

img = Image.open("/home/gurshant_singh/cavOS/sanjha_logo.png").convert("RGBA")
img = img.resize((200, 200), Image.LANCZOS)

width, height = img.size
pixels = list(img.getdata())

print("#pragma once")
print(f"#define LOGO_WIDTH  {width}")
print(f"#define LOGO_HEIGHT {height}")
print(f"static const unsigned int sanjha_logo[{width * height}] = {{")

row = []
for (r, g, b, a) in pixels:
    row.append(f"0x{a:02X}{r:02X}{g:02X}{b:02X}")
    if len(row) == 8:
        print("  " + ", ".join(row) + ",")
        row = []
if row:
    print("  " + ", ".join(row))
print("};")
