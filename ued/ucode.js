window.onload = (event) => {
    collect();
};

function gebi(s) {
    return document.getElementById(s);
}

// It's possible to initialize constants from within the
// onload function so they are available early, but it's
// messy, so we punt it.

const WIDTH = 1350; // parseInt(gebi("canvas").width);
const HEIGHT = 500; // parseInt(gebi("canvas").height);
const SCALE = 50; // most things done in 50 pixel boxes
const ROWS = HEIGHT / SCALE;
const COLS = WIDTH / SCALE;

// Point in 50-pixel boxes
class Point {
    btX;
    btY;

    constructor(btX, btY) {
        this.btX = btX;
        this.btY = btY;
    }

    scale() { // returns 2-tuple of pixel position
        return [SCALE * this.btX, SCALE * this.btY];
    }
}

class Drawable {
    ctx; // rendering context for canvas

    constructor(ctx) {
        this.ctx = ctx;
    }

    draw() {
        console.log("draw(): not overridden");
    }
}

// Closed polygons and lines including multilines are all "Paths".
// They include an array of points. Their implementation of Draw()
// is broken into a shareable begin() and a finish() that must be
// overridden to produce either a stroked line or filled figure.
class Path extends Drawable {
    btPoints; // array of Point

    constructor(ctx, btPoints) {
        super(ctx);
        this.btPoints = btPoints;
    }

    // Draw most of a path, but don't close it.
    // Subclass must override finishDraw()
    beginDraw() {
        this.ctx.beginPath();
        let px = this.btPoints[0].scale();
        this.ctx.moveTo(px[0], px[1]);
        for (let i = 1; i < this.btPoints.length; i++) {
            let px = this.btPoints[i].scale();
            this.ctx.lineTo(px[0], px[1]);
        }
    }

    finishDraw() {
        console.log("finishDraw(): not overridden");
    }

	// Optional override
    beforeBeginDraw() {}

	// Optional override
    afterFinishDraw() {}

    draw() { // final
        this.beforeBeginDraw();
        this.beginDraw();
        this.finishDraw();
        this.afterFinishDraw();
    }
}

// Four sided polygons. Allows for rectangle and
// trapezoid subclasses. Abstract - does not override finishDraw().
class Quad extends Path {
    constructor(ctx, corners) {
        super(ctx, corners);
    }

    upperLeft() {
        return this.btPoints[0];
    }

	upperRight() {
        return this.btPoints[1];
	}

    topCenter() {
        let btTopLen = this.btPoints[1].btX - this.btPoints[0].btX;
        return new Point(this.btPoints[0].btX + btTopLen / 2, this.btPoints[0].btY);
    }

    lowerRight() {
        return this.btPoints[2];
    }

    lowerLeft() {
        return this.btPoints[3];
    }

    bottomCenter() {
        let btTopLen = this.btPoints[1].btX - this.btPoints[0].btX;
        return new Point(this.btPoints[0].btX + btTopLen / 2, this.btPoints[2].btY);
    }
}

class Rect extends Quad {
    btUL;
    btSz;

    // Compute the corner points of a rectangle. Static
    // because it's in the calling sequence of the ctor.
    static rectPoints(ul, sz) {
        let result = [new Point(ul.btX, ul.btY),
            new Point(ul.btX + sz.btX, ul.btY),
            new Point(ul.btX + sz.btX, ul.btY + sz.btY),
            new Point(ul.btX, ul.btY + sz.btY)
        ];
        return result;
    }

    constructor(ctx, ul, sz) {
        super(ctx, Rect.rectPoints(ul, sz));
        this.btUL = ul;
        this.btSz = sz;
    }

    // Filled rect - finish draw with lineTo and fill().
    finishDraw() {
        let px = this.btUL.scale();
        this.ctx.lineTo(px[0], px[1]);
        this.ctx.fill();

        // put a box around it
        let ss = this.ctx.strokeStyle;
        let lw = this.ctx.lineWidth;
        this.ctx.strokeStyle = "rgb(0, 0, 192)";
        this.ctx.lineWidth = 1.5;
        this.ctx.beginPath();
        px = this.btPoints[0].scale();
        this.ctx.moveTo(px[0], px[1]);
        for (let i = 1; i < this.btPoints.length; i++) {
            px = this.btPoints[i].scale();
            this.ctx.lineTo(px[0], px[1]);
        }
        this.ctx.closePath();
        this.ctx.stroke();
        this.ctx.strokeStyle = ss;
        this.ctx.lineWidth = lw;
    }
}

// This is a gross implementation inheritance cheat. We
// make Trapezoid a funny kind of Rectangle. They all
// narrow toward the bottom, which is a silly software
// restriction but a good block diagram practice.
class Trap extends Rect {
    constructor(ctx, ul, sz) {
        super(ctx, ul, sz);
        this.btPoints[2].btX -= 0.5;
        this.btPoints[3].btX += 0.5;
        this.btUL = ul;
        this.btSz = sz;
    }
}

class Line extends Path {
    constructor(ctx, btPath) {
        super(ctx, btPath);
    }

	get btStart() {
		return this.btPoints[0];
	}

	get btPrev() { // may return btStart()
		return this.btPoints[this.btPoints.length - 2];
	}

	get btEnd() {
		return this.btPoints[this.btPoints.length - 1];
	}

    finishDraw() {
        this.ctx.stroke();
    }
}

class Arrow extends Line {
	name;
	visible;
    saveWidth;
    saveStyle;
    saveFill;

    constructor(ctx, name, visible, btPath) {
        super(ctx, btPath);
		this.name = name;
		this.visible = visible;
    }

	draw() {
		if (this.visible) {
			super.draw();
		}
	}

	setVisible(aBoolean) {
		this.visible = aBoolean;
	}

    beforeBeginDraw() {
        this.saveWidth = this.ctx.lineWidth;
        this.saveColor = this.ctx.strokeStyle;
        this.saveFill = this.ctx.fillStyle;
        this.ctx.lineWidth = 3.5;
        this.ctx.strokeStyle = "rgb(0, 192, 0)";
        this.ctx.fillStyle = "rgb(0, 192, 0)";
    }

    finishDraw() {
        super.finishDraw();

        // Arrowhead - computations in pixels
		const size = 5;
        let px = this.btPoints[this.btPoints.length - 1].scale(); // endpoint of last segment

        this.ctx.beginPath();
        this.ctx.moveTo(px[0], px[1]);

        if (this.btPrev.btY == this.btEnd.btY) {
            // horizontal
            if (this.btPrev.btX < this.btEnd.btX) {
                this.ctx.lineTo(px[0] - size, px[1] - size);
                this.ctx.lineTo(px[0] - size, px[1] + size);
            } else {
                this.ctx.lineTo(px[0] + size, px[1] - size);
                this.ctx.lineTo(px[0] + size, px[1] + size);
            }
        } else {
            // vertical
            if (this.btPrev.btY < this.btEnd.btY) {
                this.ctx.lineTo(px[0] - size, px[1] - size);
                this.ctx.lineTo(px[0] + size, px[1] - size);
            } else {
				this.ctx.lineTo(px[0] - size, px[1] + size);
				this.ctx.lineTo(px[0] + size, px[1] + size);
            }
        }

		this.ctx.closePath();
        this.ctx.stroke();
    }

    afterFinishDraw() {
        this.ctx.lineWidth = this.saveWidth;
        this.ctx.strokeStyle = this.saveStyle;
        this.ctx.fillStyle = this.saveFill;
    }
}

// A Control is an HTML select control in a label.
// We just position it in 50-pixel box units on the
// glass pane over the canvas and provide a few
// accessors for the embedded selector.
class Control extends Drawable {
    id; // control id for gebi()
    btUL; // upper left, in box coords
	label; // gebi(this.id)
	ctl; // select control child of label

    constructor(ctx, id, ul = new Point(0, 0)) {
        super(ctx);
        this.id = id;
        this.btUL = ul;
		this.label = gebi(id);
		this.ctl = gebi(id).children[0];
    }

	// I just couldn't hack having a "disabled" property,
	// even though that's how HTML does it. Move along.
	enable(aBoolean) {
		this.ctl.disabled = !aBoolean;
	}

	enabled() {
		return !this.ctl.disabled;
	}

	value() {
		//console.log(`${this.ctl.options.selectedIndex}`);
		return this.ctl.options.selectedIndex;
	}

	valueName() { // e.g. "alu_add" when acn[0] is chosen
		//console.log(`${this.ctl.options[this.value()].text}`);
		return this.ctl.options[this.value()].text;
	}

	// If the argument is true, enable our selector. If not,
	// first set our selector to its maximum value and then
	// disable it. Thus the disabled state of all controls
	// would be {max}{max} ... == 0b111... == 0xFF... == noop.
	setActive(aBool) {
		if (!aBool) {
			this.enable(true);
			this.ctl.selectedIndex = this.ctl.options.length - 1;
		}
		this.enable(aBool)
	}

	// Set our select control to the index of the argument name
	setByName(name) {
		//console.log(`${this.ctl.length} ${this.ctl.options[0]}`);
		for (let i = 0; i < this.ctl.length; ++i) {
			if (this.ctl.options[i].text == name) {
				this.ctl.selectedIndex = i;
			}
		}
	}

    draw() {
        let px = this.btUL.scale();
        this.label.style.position = "absolute";
        this.label.style.left = `${px[0]}px`;
        this.label.style.top = `${px[1]}px`;
    }

    moveTo(p) {
        this.btUL = p;
    }

    // Set the upper left coordinates of this control
    setUpperLeft(p) {
        this.moveTo(p);
    }

    setLowerLeft(p) {
        let r = this.label.getBoundingClientRect(); // pixel space
        let btClientHeight = r.height / SCALE;
        let result = new Point(p.btX, p.btY - btClientHeight);
        this.btUL = result;
    }

    setLowerRight(p) {
        let r = this.label.getBoundingClientRect(); // pixel space
        let btClientHeight = r.height / SCALE;
        let btClientWidth = r.width / SCALE;
        let result = new Point(p.btX - btClientWidth, p.btY - btClientHeight);
        this.btUL = result;
    }

    // Center the top of this control over the point argument.
    setCenterTop(p) {
        let r = this.label.getBoundingClientRect(); // pixel space
        let btClientWidth = r.width / SCALE;
        let result = new Point(p.btX - btClientWidth / 2, p.btY);
        this.btUL = result;
    }

    // Center the bottom of this control over the point argument.
    setCenterBottom(p) {
        let r = this.label.getBoundingClientRect(); // pixel space
        let btClientWidth = r.width / SCALE;
        let btClientHeight = r.height / SCALE;
        let result = new Point(p.btX - btClientWidth / 2, p.btY - btClientHeight);
        this.btUL = result;
    }
}

// Temporary function to draw a box (50 pixel) grid on the canvas.
// For use during design, to be removed when design is complete.
function drawGrid(ctx) {
    for (let row = 1; row < ROWS; row++) {
        new Line(ctx, [new Point(0, row), new Point(COLS, row)]).draw();
    }
    for (let col = 1; col < COLS; col++) {
        new Line(ctx, [new Point(col, 0), new Point(col, ROWS)]).draw();
    }
}

let init = false;

let controls; // array of controls
let src1, src2, dst, alu_ctl, acn, alu_load_hold,
	alu_load_flgs, sysdata_src, reg_in_mux, rcw_cross,
	sysaddr_src, dst_wr_en, rw, m16_en, load_ir,
	rcw_ir_uc, carry_en, load_flgs_mux, acn_ir_uc, ir0_en;

let areas; // array of areas
let areaRegInMux, areaBank1, areaBank2, areaPort2Hold, areaAlu,
    areaMem, areaIx, areaIr, areaAluOut, areaFlags, areaFlagsMux;

let arrows; // array of arrows

function initialize(ctx) {

	// Quads
	areaRegInMux = new Trap(ctx, new Point(1, 1), new Point(6, 0.5));
	areaBank1 = new Rect(ctx, new Point(1, 2), new Point(3, 2));
	areaBank2 = new Rect(ctx, new Point(4, 2), new Point(3, 2));
	areaPort2Hold = new Rect(ctx, new Point(4, 4.5), new Point(3, 0.5));
	areaAlu = new Trap(ctx, new Point(1, 5.5), new Point(6, 1));
	areaMem = new Rect(ctx, new Point(12, 2), new Point(4, 5));
	areaIx = new Rect(ctx, new Point(17, 2), new Point(4, 5));
	areaIr = new Rect(ctx, new Point(23, 5), new Point(3, 1));
	areaFlagsMux = new Trap(ctx, new Point(6.5, 7), new Point(4.5, 0.5));
	areaFlags = new Rect(ctx, new Point(6.5, 8), new Point(4.5, 0.5));
	areaAluOut = new Rect(ctx, new Point(2, 7), new Point(4, 0.5));

	areas = [areaRegInMux, areaBank1, areaBank2, areaPort2Hold, areaAlu,
		areaMem, areaIx, areaIr, areaAluOut, areaFlags, areaFlagsMux];

	// Arrows
	mainDataBus = new Arrow(ctx, "sysaddr", true,
		[new Point(0.5, 0.5), new Point(COLS - 0.5, 0.5)]);
	mainAddrBus = new Arrow(ctx, "sysdata", true,
		[new Point(0.5, ROWS - 0.5), new Point(COLS - 0.5, ROWS - 0.5)]);
	mainMemoryAddr = new Arrow(ctx, "mainMemoryAddr", true,
		[new Point(areaMem.bottomCenter().btX, mainAddrBus.btPoints[0].btY), areaMem.bottomCenter()]);
	busToMux = new Arrow(ctx, "busToMux", false,
		[new Point(areaRegInMux.topCenter().btX, areaRegInMux.topCenter().btY - 0.5),
		areaRegInMux.topCenter()]);
	muxToReg = new Arrow(ctx, "muxToReg", false,
		[areaRegInMux.bottomCenter(), areaBank2.upperLeft()]);
	aluToReg = new Arrow(ctx, "aluToReg", false,
		[new Point(3, 7.5), new Point(3, 8), new Point(0.5, 8), new Point(0.5, 1), new Point(1, 1)])
	aluToAddr = new Arrow(ctx, "aluToAddr", false,
		[areaAluOut.bottomCenter(),
		new Point(areaAluOut.bottomCenter().btX, areaAluOut.bottomCenter().btY + 2)])
	aluAinput = new Arrow(ctx, "aluAinput", false,
		[areaBank1.bottomCenter(),
		 new Point(areaBank1.bottomCenter().btX, areaBank1.bottomCenter().btY + 1.5)]);
	regToAluHold = new Arrow(ctx, "regToAluHold", false,
		[areaBank2.bottomCenter(), areaPort2Hold.topCenter()]);
	busToAluHold = new Arrow(ctx, "busToAluHold", false,
		[new Point(9, 0.5), new Point(9, 4.5), areaPort2Hold.upperRight()]);
	holdToAluBinput = new Arrow(ctx, "holdToAluBinput", false,
		[areaPort2Hold.bottomCenter(),
		new Point(areaPort2Hold.bottomCenter().btX, areaPort2Hold.bottomCenter().btY + 0.5)]);
	aluOut = new Arrow(ctx, "aluOut", false,
		[areaAlu.bottomCenter(), areaAluOut.topCenter()]);
	aluOutFlags = new Arrow(ctx, "aluOutFlags", false,
		[areaAlu.lowerRight(), new Point(areaFlagsMux.topCenter().btX, areaAlu.lowerRight().btY),
		 areaFlagsMux.topCenter()]);
	busToFlags = new Arrow(ctx, "busToFlags", false,
		[new Point(areaFlagsMux.upperRight().btX, mainDataBus.btPoints[0].btY), areaFlagsMux.upperRight()]);
	muxToFlags = new Arrow(ctx, "muxToFlags", false,
		[areaFlagsMux.bottomCenter(), areaFlags.topCenter()]);
	flagsToBus = new Arrow(ctx, "flagsToBus", false,
		[areaFlags.bottomCenter(),
		 new Point(areaFlags.bottomCenter().btX, areaFlags.bottomCenter().btY + 0.5),
		 new Point(areaFlags.lowerRight().btX + 0.5, areaFlags.lowerRight().btY + 0.5),
		 new Point(areaFlags.lowerRight().btX + 0.5, mainDataBus.btPoints[0].btY)]);
	regToBus = new Arrow(ctx, "regToBus", false,
		[new Point(areaBank2.upperRight().btX, areaBank2.upperRight().btY + 1),
		 new Point(areaBank2.upperRight().btX + 1, areaBank2.upperRight().btY + 1),
		 new Point(areaBank2.upperRight().btX + 1, mainDataBus.btPoints[0].btY)]);
	memToBus = new Arrow(ctx, "memToBus", false,
		[areaMem.topCenter(), new Point(areaMem.topCenter().btX, mainDataBus.btPoints[0].btY)]);
	busToMem = new Arrow(ctx, "busToMem", false,
		[new Point(areaMem.topCenter().btX, mainDataBus.btPoints[0].btY), areaMem.topCenter()]);
	regToAddr = new Arrow(ctx, "regToAddr", false,
		[new Point(areaBank1.lowerLeft().btX + 0.5, areaBank1.lowerLeft().btY),
		 new Point(areaBank1.lowerLeft().btX + 0.5, mainAddrBus.btPoints[0].btY)]);
	ixToAddr = new Arrow(ctx, "ixToAddr", false,
		[areaIx.bottomCenter(), new Point(areaIx.bottomCenter().btX, mainAddrBus.btPoints[0].btY)]);
	addrToData = new Arrow(ctx, "addrToData", false,
		[new Point(areaIx.lowerRight().btX + 1, mainAddrBus.btPoints[0].btY),
		 new Point(areaIx.lowerRight().btX + 1, mainDataBus.btPoints[0].btY)]);
	dataToIr = new Arrow(ctx, "dataToIr", false,
		[new Point(areaIr.topCenter().btX, mainDataBus.btPoints[0].btY), areaIr.topCenter()]);

	arrows = [mainAddrBus, mainDataBus, mainMemoryAddr, busToMux, aluToReg, aluToAddr,
			  aluAinput, muxToReg, regToAluHold, busToAluHold, holdToAluBinput, aluOut,
			  aluOutFlags, busToFlags, muxToFlags, flagsToBus, regToBus, memToBus,
			  busToMem, regToAddr, ixToAddr, addrToData, dataToIr ];

	// Controls. Their variable names and IDs match the microcode
	// definitions and documentation, so makes sense in context.
	src1 = new Control(ctx, "src1");
	src2 = new Control(ctx, "src2");
	dst = new Control(ctx, "dst");
	alu_ctl = new Control(ctx, "alu_ctl");
	acn = new Control(ctx, "acn");
	alu_load_hold = new Control(ctx, "alu_load_hold");
	alu_load_flgs = new Control(ctx, "alu_load_flgs");
	sysdata_src = new Control(ctx, "sysdata_src");
	reg_in_mux = new Control(ctx, "reg_in_mux");
	rcw_cross = new Control(ctx, "rcw_cross");
	sysaddr_src = new Control(ctx, "sysaddr_src");
	dst_wr_en = new Control(ctx, "dst_wr_en");
	rw = new Control(ctx, "rw");
	m16_en = new Control(ctx, "m16_en");
	load_ir = new Control(ctx, "load_ir");
	rcw_ir_uc = new Control(ctx, "rcw_ir_uc");
	carry_en = new Control(ctx, "carry_en");
	load_flgs_mux = new Control(ctx, "load_flgs_mux");
	acn_ir_uc = new Control(ctx, "acn_ir_uc");
	ir0_en = new Control(ctx, "ir0_en");
	carry_en = new Control(ctx, "carry_en");

	controls = [src1, src2, dst, alu_ctl, acn, alu_load_hold,
		alu_load_flgs, sysdata_src, reg_in_mux, rcw_cross,
		sysaddr_src, dst_wr_en, rw, m16_en, load_ir,
		rcw_ir_uc, carry_en, load_flgs_mux, acn_ir_uc,
		ir0_en];

	src1.setCenterTop(areaBank1.topCenter());
	src2.setCenterTop(areaBank2.topCenter());
	dst.setCenterTop(new Point(areaBank1.topCenter().btX, areaBank1.topCenter().btY + 1));
	dst_wr_en.setCenterTop(new Point(areaBank2.topCenter().btX, areaBank2.topCenter().btY + 1));
	alu_load_hold.setUpperLeft(areaPort2Hold.upperLeft());
	alu_ctl.setCenterTop(areaAlu.topCenter());
	acn.setCenterBottom(areaAlu.bottomCenter());
	reg_in_mux.setCenterTop(areaRegInMux.topCenter());
	rw.setCenterTop(areaMem.topCenter());
	m16_en.setCenterBottom(areaMem.bottomCenter());
	sysdata_src.setCenterTop(new Point(12, 0));
	sysaddr_src.setCenterBottom(new Point(12, 10));
	alu_load_flgs.setCenterTop(areaFlags.topCenter());
	load_flgs_mux.setCenterTop(areaFlagsMux.topCenter());
	load_ir.setCenterTop(areaIr.topCenter());
	ir0_en.setCenterBottom(areaIr.bottomCenter());
	rcw_cross.setCenterBottom(areaIx.bottomCenter());
	rcw_ir_uc.setCenterTop(areaIr.bottomCenter());
	p = areaIr.bottomCenter();
	p.btY += 1;
	acn_ir_uc.setCenterBottom(p);
	carry_en.setUpperLeft(new Point(7, 5.5));
}

// data bus selector codes 5 and 6 are "tbd". Code 7 is "undriven".
function dataBusActive() {
	return sysdata_src.value() <= 4;
}

function applyRules(ctx) {
	// If RCW fields come from the instruction, don't allow them to be set in microcode.
	src1.setActive(rcw_ir_uc.valueName() == "rcw_from_uc");
	src2.setActive(rcw_ir_uc.valueName() == "rcw_from_uc");
	acn.setActive(acn_ir_uc.valueName() == "acn_from_uc");

	// Similarly for dst, plus dst_wr_en must be set to enable it
	dst.setActive(dst_wr_en.valueName() == "yes" && rcw_ir_uc.valueName() == "rcw_from_uc");

	// If register write isn't happening, don't allow input path to be set
	reg_in_mux.setActive(dst_wr_en.valueName() == "yes");

	// If flags write isn't enabled, don't allow input path to be set
	load_flgs_mux.setActive(alu_load_flgs.valueName() == "yes");

	// If not an ALU operation, don't allow carry in to be suppressed
	carry_en.setActive(alu_ctl.valueName() == "alu_phi1" || alu_ctl.valueName() == "alu_phi2");

	// If memory is supposed to drive the bus, or nothing is driving the bus,
	// don't allow memory to be written. Sysdata_src 5, 6, 7 are "undriven".
	rw.setActive(sysdata_src.valueName() != "bus_mem" && dataBusActive());

	// If we're not reading or writing memory, don't allow memory width selection
	m16_en.setActive(rw.valueName() == "write" || sysdata_src.valueName() == "bus_mem");

	// If nothing is driving the bus, don't allow it to be clocked into IR
	load_ir.setActive(dataBusActive());

	// Only allow IR bit 0 to be forced low when it's heading to the bus
	ir0_en.setActive(sysdata_src.valueName() == "bus_ir");

	// Arrows. Make visible arrows that show actual data flows.
	busToMux.setVisible(reg_in_mux.enabled() && reg_in_mux.valueName() == "from_bus");
	muxToReg.setVisible(dst_wr_en.valueName() == "yes");
	aluToReg.setVisible(reg_in_mux.enabled() && reg_in_mux.valueName() == "from_alu");
	aluToAddr.setVisible(sysaddr_src.valueName() == "addr_alu");
	aluAinput.setVisible(alu_ctl.valueName() == "alu_phi1");
	busToAluHold.setVisible(alu_load_hold.valueName() == "yes" && alu_ctl.valueName() == "alu_in");
	regToAluHold.setVisible(alu_load_hold.valueName() == "yes" && alu_ctl.valueName() != "alu_in");
	holdToAluBinput.setVisible(alu_ctl.valueName() == "alu_phi1");
	aluOut.setVisible(alu_ctl.valueName() == "alu_phi2");
	aluOutFlags.setVisible(alu_load_flgs.valueName() == "yes" && load_flgs_mux.valueName() == "from_alu");
	busToFlags.setVisible(alu_load_flgs.valueName() == "yes"  && load_flgs_mux.valueName() == "from_bus");
	muxToFlags.setVisible(alu_load_flgs.valueName() == "yes");
	flagsToBus.setVisible(sysdata_src.valueName() == "bus_f");
	regToBus.setVisible(sysdata_src.valueName() == "bus_gr");
	memToBus.setVisible(sysdata_src.valueName() == "bus_mem");
	addrToData.setVisible(sysdata_src.valueName() == "bus_addr");
	busToMem.setVisible(rw.valueName() == "write");
	regToAddr.setVisible(sysaddr_src.valueName() == "addr_gr" || sysaddr_src.valueName() == "addr_op");
	ixToAddr.setVisible(sysaddr_src.valueName() == "addr_ix");
	dataToIr.setVisible(load_ir.valueName() == "yes");
}

function redraw() {
    const canvas = document.getElementById("canvas");
    const ctx = canvas.getContext("2d");
	ctx.clearRect(0, 0, canvas.width, canvas.height);

    ctx.strokeStyle = "rgb(0, 0, 0)";
    ctx.lineWidth = 0.5;
    ctx.fillStyle = "rgb(224, 224, 224)";

	// for layout, this is helpful - draws a 1-box (SCALE pixel) grid
    // drawGrid(ctx);

    if (!init) {
		initialize(ctx);
        init = true;
    }

	applyRules(ctx)

    for (let i = 0; i < areas.length; i++) {
        areas[i].draw();
    }

    for (let i = 0; i < arrows.length; i++) {
        arrows[i].draw();
    }

    for (let i = 0; i < controls.length; i++) {
        controls[i].draw();
    }
}

// This is called at onload() time and every time something changes
function collect() {
    const kmap = new Map();
    const comments = new Map();
    kmap.set("k3", 0xFF);
    kmap.set("k2", 0xFF);
    kmap.set("k1", 0xFF);
    kmap.set("k0", 0xFF);
    comments.set("k3", "");
    comments.set("k2", "");
    comments.set("k1", "");
    comments.set("k0", "");

    Array.from(gebi('controls').elements).forEach((ctl) => {
        a = ctl.id.split("-");
        if (a.length == 4 && a[0].startsWith("k")) { // if it's a "k control" (microcode setting):
            // a[0] is the k-register; a[1] is the field name; a[2] is the AND pattern; a[3] is the shift
            kmap.set(a[0], kmap.get(a[0]) & ~(a[2] << a[3]));
            kmap.set(a[0], kmap.get(a[0]) | ((ctl.value & a[2]) << a[3]));
            comments.set(a[0], comments.get(a[0]) + " " + a[1] + "=" + ctl.options[ctl.selectedIndex].text);
        }
    });

    microcode.innerText =
        "WriteK(" +
        "0x" + kmap.get("k3").toString(16) +
        ", 0x" + kmap.get("k2").toString(16) +
        ", 0x" + kmap.get("k1").toString(16) +
        ", 0x" + kmap.get("k0").toString(16) +
        ");" + "\n\n" +
        ".set K3_FIX" + comments.get("k3") + "\n" +
        ".set K2_FIX" + comments.get("k2") + "\n" +
        ".set K1_FIX" + comments.get("k1") + "\n" +
        ".set K0_FIX" + comments.get("k0")

    redraw();

}
