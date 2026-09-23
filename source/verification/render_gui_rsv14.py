#!/usr/bin/env python3
"""Render schematic RSv14 GUI mockups for layout review (not game screenshots)."""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


out = Path(__file__).resolve().parent / "layouts-rsv14"
out.mkdir(exist_ok=True)
font_path = "C:/Windows/Fonts/segoeui.ttf"
bold_path = "C:/Windows/Fonts/seguisb.ttf"


def font(size, bold=False):
    return ImageFont.truetype(bold_path if bold else font_path, size)


BG = (28, 27, 24)
PANEL = (43, 40, 34)
FIELD = (25, 24, 21)
EDGE = (92, 77, 48)
GOLD = (205, 163, 82)
INK = (232, 224, 207)
MUTED = (165, 157, 141)
BLUE = (57, 130, 178)


def base(title):
    im = Image.new("RGB", (780, 730), BG)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((12, 12, 768, 718), 10, fill=PANEL, outline=EDGE, width=2)
    d.text((30, 25), title, font=font(25, True), fill=GOLD)
    d.text((30, 58), "SCHEMATIC LAYOUT — NOT AN IN-GAME SCREENSHOT", font=font(11), fill=MUTED)
    return im, d


def button(d, box, label, active=False):
    d.rounded_rectangle(box, 5, fill=(78, 64, 39) if active else (55, 51, 43), outline=GOLD if active else EDGE)
    x1, y1, x2, y2 = box
    w = d.textlength(label, font=font(14, True))
    d.text(((x1+x2-w)/2, (y1+y2)/2-9), label, font=font(14, True), fill=INK)


def field(d, box, label, value=""):
    x1, y1, x2, y2 = box
    d.text((x1, y1-20), label, font=font(12, True), fill=MUTED)
    d.rounded_rectangle(box, 4, fill=FIELD, outline=EDGE)
    d.text((x1+9, y1+8), value, font=font(14), fill=INK)


im, d = base("RuneSchema Helpy — Items")
for i, name in enumerate(("ITEMS", "NPCS & AI", "RESOURCES")):
    button(d, (24+i*244, 88, 254+i*244, 125), name, i == 0)
for i, name in enumerate(("SPAWN", "CREATE / CLONE", "FAVORITES")):
    button(d, (24+i*180, 138, 192+i*180, 170), name, i == 0)
for row in range(3):
    for col in range(4):
        x, y = 24+col*184, 188+row*137
        d.rounded_rectangle((x, y, x+172, y+128), 6, fill=FIELD, outline=EDGE)
        d.ellipse((x+6, y+6, x+28, y+28), fill=BLUE); d.text((x+10, y+8), "D", font=font(11, True), fill=INK)
        d.ellipse((x+142, y+6, x+166, y+30), outline=GOLD); d.text((x+149, y+7), "★", font=font(14), fill=GOLD)
        d.rounded_rectangle((x+54, y+12, x+118, y+70), 8, fill=(66, 61, 48), outline=EDGE)
        d.text((x+48, y+78), f"Item {row*4+col+1}", font=font(14, True), fill=INK)
        d.text((x+8, y+106), "Base power: 12", font=font(11), fill=MUTED)
        if col % 2 == 0:d.ellipse((x+142, y+99, x+166, y+123), fill=(117, 77, 42))
d.text((24, 623), "Source badge · identity", font=font(12), fill=MUTED)
d.text((260, 623), "Top-right · favorite", font=font(12), fill=MUTED)
d.text((500, 623), "Lower-right · traits", font=font(12), fill=MUTED)
im.save(out / "01-helpy-item-placards.png")

im, d = base("Create / Clone Item")
for x, label in ((24, "1  SOURCE ITEM"), (270, "2  APPEARANCE"), (520, "3  INVENTORY ICON")):
    d.text((x, 100), label, font=font(13, True), fill=MUTED)
for x in (24, 270):
    d.rounded_rectangle((x, 126, x+228, 270), 6, fill=FIELD, outline=EDGE)
    d.rounded_rectangle((x+78, 142, x+148, 208), 8, fill=(66, 61, 48), outline=EDGE)
d.rounded_rectangle((585, 135, 665, 215), 10, fill=(66, 61, 48), outline=EDGE)
field(d, (24, 340, 500, 378), "4  ITEM DETAILS", "Abyssal Hatchet Clone")
field(d, (520, 340, 744, 378), "POWER LEVEL", "Inherit")
field(d, (24, 416, 744, 457), "FLAVOUR TEXT", "A tuned copy of the loaded source item.")
field(d, (24, 497, 500, 535), "COOKED ICON PATH", "/Game/.../T_Hatchet")
button(d, (520, 497, 630, 535), "[x] Journal", True)
button(d, (642, 497, 756, 535), "[x] Recipe", True)
button(d, (24, 574, 198, 612), "[x] Permanent", True)
button(d, (210, 574, 330, 612), "[x] Give", True)
button(d, (342, 574, 470, 612), "[x] Test save", True)
button(d, (574, 568, 756, 618), "Create item", True)
d.text((24, 650), "Identity → appearance → details → linked outputs → create", font=font(16, True), fill=GOLD)
im.save(out / "02-helpy-clone-flow.png")

for filename, title, recipe in (("03-journal-editor.png", "Journal Entry for This Item", False), ("04-recipe-editor.png", "Recipe for This Item", True)):
    im, d = base(title)
    button(d, (34, 96, 746, 136), "[x] Create and link " + ("recipe" if recipe else "journal entry"), True)
    if recipe:
        field(d, (34, 190, 746, 230), "CRAFTING STATION", "Artisan Workbench")
        field(d, (34, 276, 358, 314), "CATEGORY", "RuneSchema")
        field(d, (384, 276, 498, 314), "OUTPUT", "1")
        button(d, (520, 276, 746, 314), "[ ] Auto-unlock")
        d.text((34, 350), "INGREDIENTS", font=font(15, True), fill=MUTED)
        for i, item in enumerate(("Oak log", "Bronze bar", "Leather strip")):
            y=386+i*54; d.rounded_rectangle((34,y,746,y+44),5,fill=FIELD,outline=EDGE);d.text((88,y+11),item,font=font(14),fill=INK);field(d,(500,y+5,582,y+39),"","2");button(d,(602,y+5,734,y+39),"Remove")
    else:
        field(d, (34, 190, 746, 230), "TITLE", "Abyssal Hatchet")
        field(d, (34, 282, 746, 354), "DESCRIPTION", "A record linked to the newly authored item.")
        field(d, (34, 426, 746, 466), "JOURNAL LOCATION", "Equipment / Axes")
        field(d, (34, 526, 354, 566), "GROUP ID", "abyssal_tools")
        field(d, (386, 526, 746, 566), "GROUP DISPLAY NAME", "Abyssal Tools")
    im.save(out / filename)

im, d = base("UE4SS — Server & Loaders")
for i, name in enumerate(("General", "Server & Loaders", "Authoring & Tools")):
    button(d, (24+i*244, 84, 250+i*244, 124), name, i == 1)
for i, name in enumerate(("Installed Mods", "Loader Controls", "Load Order")):
    button(d, (24, 150+i*48, 184, 188+i*48), name, i == 1)
d.rounded_rectangle((204, 150, 752, 666), 7, fill=FIELD, outline=EDGE)
d.text((226, 174), "Loader controls and authoring map", font=font(20, True), fill=GOLD)
d.text((226, 210), "Players", font=font(17, True), fill=INK)
button(d, (226, 246, 460, 284), "[x] Enable loader", True)
d.text((226, 316), "AUTHORING FLOW", font=font(13, True), fill=MUTED)
d.text((226, 346), "Patch authored rule · Create stable Id · Nameplate.Definition", font=font(14), fill=INK)
d.text((226, 390), "PLAYER COVERAGE", font=font(13, True), fill=MUTED)
for i, line in enumerate(("Target: name, GUID, all players, load slot", "Vitals, movement, capacity and combat attributes", "Appearance, visual effects, archetype and nameplate", "Open Authoring & Tools → Players for the focused editor")):
    d.text((240, 424+i*35), "• "+line, font=font(14), fill=INK)
im.save(out / "05-ue4ss-server-loaders.png")

print(f"Rendered {len(list(out.glob('*.png')))} schematic layouts to {out}")
