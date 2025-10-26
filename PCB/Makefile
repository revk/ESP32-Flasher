all: png stl */README.md

stl:    Flasher/Flasher[TBC].stl

png:	$(patsubst %.kicad_pcb,%.png,$(wildcard */*.kicad_pcb))

PCBCase/case: PCBCase/case.c
	make -C PCBCase

PCBCase/clean: PCBCase/clean.c
	make -C PCBCase

%.stl: %.scad
	echo "Making $@"
	/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD $< -o $@
	echo "Made $@"

Flasher/FlasherT.scad: Flasher/Flasher.kicad_pcb PCBCase/case
	PCBCase/case -o $@ $< --edge=2 --bottom=3 --top=10 --pcb=2 --two-file

%.png:	%.kicad_pcb PCBCase/clean PCBCase/render
	PCBCase/render $<

%/README.md: %/*.kicad_pcb %/*.stl PCBCase/readme
	PCBCase/readme $@
