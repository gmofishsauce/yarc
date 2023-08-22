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

    topMiddle() {
        let btTopLen = this.btPoints[1].btX - this.btPoints[0].btX;
        return new Point(this.btPoints[0].btX + btTopLen / 2, this.btPoints[0].btY);
    }

    lowerRight() {
        return this.btPoints[2];
    }

    lowerLeft() {
        return this.btPoints[3];
    }

    bottomMiddle() {
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
    btStart;
    btEnd;

    constructor(ctx, btStart, btEnd) {
        super(ctx, [btStart, btEnd]);
        this.btStart = btStart;
        this.btEnd = btEnd;
    }

    finishDraw() {
        let px = this.btEnd.scale();
        this.ctx.lineTo(px[0], px[1]);
        this.ctx.closePath();
        this.ctx.stroke();
    }
}

class Arrow extends Line {
    saveWidth;
    saveStyle;
    saveFill;

    constructor(ctx, btStart, btEnd) {
        super(ctx, btStart, btEnd);
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
		const size = 7;
        let px = this.btPoints[this.btPoints.length - 1].scale();

        this.ctx.beginPath();
        this.ctx.moveTo(px[0], px[1]); // end pixel of the line

        if (this.btStart.btY == this.btEnd.btY) {
            // horizontal
            if (this.btStart.btX < this.btEnd.btX) {
                this.ctx.lineTo(px[0] - size, px[1] - size);
                this.ctx.lineTo(px[0] - size, px[1] + size);
            } else {
                this.ctx.lineTo(px[0] + size, px[1] - size);
                this.ctx.lineTo(px[0] + size, px[1] + size);
            }
        } else {
            // vertical
            if (this.btStart.btY < this.btEnd.btY) {
                this.ctx.lineTo(px[0] - size, px[1] - size);
                this.ctx.lineTo(px[0] + size, px[1] - size);
            } else {
				this.ctx.lineTo(px[0] - size, px[1] + size);
				this.ctx.lineTo(px[0] + size, px[1] + size);
            }
        }

        this.ctx.fill();
    }

    afterFinishDraw() {
        this.ctx.lineWidth = this.saveWidth;
        this.ctx.strokeStyle = this.saveStyle;
        this.ctx.fillStyle = this.saveFill;
    }
}

// A Control is an HTML control. We just position
// it in 50-pixel box units on the glass pane over
// the canvas.
class Control extends Drawable {
    id; // control id for gebi()
    btUL; // upper left, in box coords

    constructor(ctx, id, ul = new Point(0, 0)) {
        super(ctx);
        this.id = id;
        this.btUL = ul;
    }

	disable(tOrF) {
		gebi(this.id).children[0].disabled = tOrF;
	}

    draw() {
        let px = this.btUL.scale();
        gebi(this.id).style.position = "absolute";
        gebi(this.id).style.left = `${px[0]}px`;
        gebi(this.id).style.top = `${px[1]}px`;
    }

    moveTo(p) {
        this.btUL = p;
    }

    // Set the upper left coordinates of this control
    setUpperLeft(p) {
        this.btUL = p;
    }

    setLowerLeft(p) {
        let r = gebi(this.id).getBoundingClientRect(); // pixel space
        let btClientHeight = r.height / SCALE;
        let result = new Point(p.btX, p.btY - btClientHeight);
        this.btUL = result;
    }

    setLowerRight(p) {
        let r = gebi(this.id).getBoundingClientRect(); // pixel space
        let btClientHeight = r.height / SCALE;
        let btClientWidth = r.width / SCALE;
        let result = new Point(p.btX - btClientWidth, p.btY - btClientHeight);
        this.btUL = result;
    }

    // Center the top of this control over the point argument.
    setCenterTop(p) {
        let r = gebi(this.id).getBoundingClientRect(); // pixel space
        let btClientWidth = r.width / SCALE;
        let result = new Point(p.btX - btClientWidth / 2, p.btY);
        this.btUL = result;
    }

    // Center the bottom of this control over the point argument.
    setCenterBottom(p) {
        let r = gebi(this.id).getBoundingClientRect(); // pixel space
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
        new Line(ctx, new Point(0, row), new Point(COLS, row)).draw();
    }
    for (let col = 1; col < COLS; col++) {
        new Line(ctx, new Point(col, 0), new Point(col, ROWS)).draw();
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

let arrows; // array of fixed arrows

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

	// Fixed arrows
	mainAddrBus = new Arrow(ctx, new Point(0.5, 0.5), new Point(COLS - 0.5, 0.5));
	mainDataBus = new Arrow(ctx, new Point(0.5, ROWS - 0.5), new Point(COLS - 0.5, ROWS - 0.5));
	/* for testing the code to draw manhattan arrowheads. Also add to arrows[].
	aN = new Arrow(ctx, new Point(9, 4), new Point(9, 3));
	aE = new Arrow(ctx, new Point(9, 4), new Point(10, 4));
	aW = new Arrow(ctx, new Point(9, 4), new Point(8, 4));
	aS = new Arrow(ctx, new Point(9, 4), new Point(9, 5));
	*/

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

	areas = [areaRegInMux, areaBank1, areaBank2, areaPort2Hold, areaAlu,
		areaMem, areaIx, areaIr, areaAluOut, areaFlags, areaFlagsMux];

	arrows = [mainAddrBus, mainDataBus];

	src1.setCenterTop(areaBank1.topMiddle());
	src2.setCenterTop(areaBank2.topMiddle());
	dst.setLowerLeft(areaBank1.lowerLeft());
	dst.disable(true);
	dst_wr_en.setCenterBottom(areaBank2.bottomMiddle());
	alu_load_hold.setUpperLeft(areaPort2Hold.upperLeft());
	alu_ctl.setCenterTop(areaAlu.topMiddle());
	acn.setCenterBottom(areaAlu.bottomMiddle());
	reg_in_mux.setCenterTop(areaRegInMux.topMiddle());
	rw.setCenterTop(areaMem.topMiddle());
	m16_en.setCenterBottom(areaMem.bottomMiddle());
	sysdata_src.setCenterTop(new Point(12, 0));
	sysaddr_src.setCenterBottom(new Point(12, 10));
	alu_load_flgs.setCenterTop(areaFlags.topMiddle());
	load_flgs_mux.setCenterTop(areaFlagsMux.topMiddle());
	load_ir.setCenterTop(areaIr.topMiddle());
	ir0_en.setCenterBottom(areaIr.bottomMiddle());
	rcw_cross.setCenterBottom(areaIx.bottomMiddle());
	rcw_ir_uc.setCenterTop(areaIr.bottomMiddle());
	p = areaIr.bottomMiddle();
	p.btY += 1;
	acn_ir_uc.setCenterBottom(p);
	carry_en.setUpperLeft(new Point(7, 5.5));
}

function applyRules(ctx) {
    // Apply a few rules, e.g. can't set K3 fields or ALU op if they come from the instruction.
    //	  rcw_from_instruction = gebi("k0-rcw_ir_uc-1-4").value == 0;
    //	  gebi("k3-src1-3-6").disabled = rcw_from_instruction;
    //	  gebi("k3-src2-7-3").disabled = rcw_from_instruction;
    //	  gebi("k3-dst-7-0").disabled = rcw_from_instruction;
    //	  gebi("k2-alu_op-15-4").disabled = rcw_from_instruction;
    //
    //	  if (gebi("k1-sysdata_src-7-5").value == 7) { // don't clock an undriven bus into anything
    //		if (gebi("k0-ir_clk-1-5").value == 0) {
    //		  alert("warning: clocking undriven sysdata into IR");
    //		}
    //		if (gebi("k0-rw-1-7").value == 0) {
    //		  alert("warning: clocking undriven sysdata into memory");
    //		}
    //		if (gebi("k1-dst_wr_en-1-0").value == 0 && gebi("k1-reg_in_mux-1-4").value == 1) {
    //		  alert("warning: clocking undriven sysdata into a general register");
    //		}
    //	  }
}

function redraw() {
    const canvas = document.getElementById("canvas");
    const ctx = canvas.getContext("2d");
    ctx.strokeStyle = "rgb(0, 0, 0)";
    ctx.lineWidth = 0.5;
    ctx.fillStyle = "rgb(224, 224, 224)";

	// for layout, this is helpful
    drawGrid(ctx);

    if (!init) {
		initialize(ctx);
        init = true;
    }

    for (let i = 0; i < areas.length; i++) {
        areas[i].draw();
    }

    for (let i = 0; i < arrows.length; i++) {
        arrows[i].draw();
    }

    for (let i = 0; i < controls.length; i++) {
        controls[i].draw();
    }

	applyRules(ctx)
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
