# whisli bean enclosure

**[Polska wersja](README.pl.md)**

**33 × 66 × 10.4 mm.** Flat enough to fit under an amplifier, and the magnets let you stick it to steel, such as the side of a device. The top and sides are one solid piece. The seam and the fingernail slots are on the underside only. The receiver's lens sticks out of the front and USB-C is at the back.

## Files

| File | What it is |
|---|---|
| `print/korpus.stl`, `print/dno.stl` | the shell and the bottom plate, already oriented for printing |
| `print/whisli-bean-obudowa.step` | the assembled model with mock electronics and magnets |
| `preview/*.png` | previews and a cross-section; `gora.png` shows the layout, `napis.png` the lettering |

## How it works

- **Plate:** four clips, two per side. Each is a 1.1 × 25.5 mm beam fixed at both ends, with a 4 mm hook in the middle, raised 1.2 mm above the solid plate. It bends within the print layers, which is the strong direction. A 0.3 mm deflection works out to about 0.7% strain. The beam prints as a bridge between posts, with no supports.
- **Opening:** there's a fingernail slot on both long sides, on the underside. The nail goes in from below and catches a groove in the plate edge. Pull-off force is estimated at about 16 N with an even pull.
- **Board:** held by a light snap. Each side has a thin beam in the shell (0.8 × 18 mm) with a 0.25 mm tooth. The tooth's slope works as a wedge and presses the board into the blocks in the shell, whatever the actual PCB thickness. A solid block surrounds each front corner, and a small rib on it takes up play along the board. Removal force is estimated at about 6 N. Once closed, rails on the plate also support the board.
- **Receiver:** the body sits in a channel in the thick front wall, and the lens comes out through a round hole. A solid 3 mm block on the plate holds it from behind, and its slope pushes the receiver forward as you close the case.
- **Magnets:** two 20 × 10 × 2 mm magnets in 22 × 12 × 1 mm seats (1 mm of margin per side), between the board and the receiver. There's 0.6 mm of plate under them, to hold as strongly as possible on steel.
- **Taking it off metal:** slide the case along the surface. The magnets hold harder than the clips, so a straight pull can separate the shell from the plate.
- **Shape:** the USB socket and the lens are at half height. The side walls are 3.5 mm thick, and the plate under the board is 1.2 mm.
- **LEDs:** a 1 × 4 mm window from the inside, leaving 0.4 mm of plastic on the outside. A collar limits light bleeding between them.
- **Lettering:** "whis.li" in the middle of the top, Helvetica Neue, 19.5 mm long, engraved 0.4 mm deep, with round dots. It reads with USB on the left and the lens on the right. The letters are slightly spaced, the outer layer of the engraving is 0.06 mm wider, and the block is optically centred.
- **Antenna:** about 10.4 mm of free space between the end of the board and the magnets.

## Assembly

No headers. Use thin wires (about 30 AWG, silicone), about 55 mm long, soldered directly to the holes. I recommend running them from the underside of the board, because then the case closes nicely.

| TSOP34438 | ESP32-C3 Super Mini |
|---|---|
| 1 OUT | GPIO4 |
| 2 GND | G |
| 3 VS | 3.3 |

1. Stick the magnets into the seats on the plate. The easiest way is to buy them with 3M adhesive (Amazon and similar sites). Without it, use epoxy or cyanoacrylate and let it cure fully. If you can choose, put them with opposite poles down: they attract instead of repelling, and usually hold better on steel.
2. Trim the receiver's legs to about 3 mm and bend them backwards.
3. Turn the shell upside down. Slide the receiver into the channel until the lens comes out of the hole, with its legs towards the 5V, G, 3.3 and GPIO4 pins.
4. Insert the board components down, at a slight angle: first the USB socket into its opening, then press the front until it clicks. To remove it, do the reverse: lever up the front edge in the middle, between the blocks.
5. Route the wires along the wall on the side of those pins (seen from above with USB towards you: the left side), over the beams.
6. Press the plate in until the clips click.

## Printing

- **White or light material,** because the LEDs need to shine through. ASA is recommended, or matte PETG. Don't use PLA near a warm amplifier, because it softens at around 55–60 °C.
- **The STLs are already placed with the outside face on the bed, so don't rotate them in the slicer.**
- No supports. The clip beams print as bridges, so check afterwards that they haven't stuck to the plate. Layer height 0.2 mm.
- The windows and lettering are the first two layers of the shell, so the first layer has to be even, with elephant foot compensation on (about 0.1 mm). Thin letters can close up if the first layer is squished too much.
- **For the best lettering:** a smooth plate, "Bottom surface pattern" set to "Monotonic line", a slow first layer (about 30 mm/s), first layer flow no higher than 100%, and a degreased plate.
- Put the seam at the back, on the USB side. A smooth plate gives a smooth top.

## Measure before printing

The receiver's dimensions come from its datasheet. The board's come from a dimensioned drawing, and Super Minis vary between manufacturers.

| Dimension | What to measure | Assumed |
|---|---|---|
| USB overhang | how far the socket sticks out past the board edge; **most important: the socket must end flush with the wall** | 1.4 mm |
| PCB and socket | PCB thickness, socket height | 1.2 mm, 3.2 mm |
| Board | width and length | 18 mm, 22.5 mm |
| LEDs | LED centre: X from the board's centre with USB pointing away from you, Y from the USB edge | PWR (−6; 5.3), IO8 (4.5; 11.2) |

## Checks after printing

1. There's a gap under the plate's beams along their whole length, and the plate goes in without force.
2. The plate clicks, holds, and comes off with a fingernail from below. The hook sticks out 0.65 mm.
3. The board clicks and holds firmly. The tooth sticks out 0.25 mm.
4. The USB-C plug goes all the way in.
5. The LEDs shine through and line up with the windows.
6. The Wi-Fi signal is still fine with the case closed, with the magnets, and on metal.
7. The case slides off metal without the plate coming off.

## Design notes

- **Bottom plate rather than a lid:** the seam is on the underside, so the top and sides are uninterrupted. The fingernail slots are there too.
- **Layout:** the magnets don't fit beside or under the board without raising the whole case, so it's longer and flatter instead. They're as far from the antenna as the 1:2 proportion allows.
- **No holes for BOOT and RESET:** resetting the password (BOOT for 10 s) means taking the plate off.
