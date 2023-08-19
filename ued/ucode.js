window.onload = (event) => {
  collect();
};

function gebi(s) {
  return document.getElementById(s);
}

function collect() {
  const kmap = new Map();
  const comments = new Map();
  kmap.set("k3", 0xFF); kmap.set("k2", 0xFF); kmap.set("k1", 0xFF); kmap.set("k0", 0xFF);
  comments.set("k3", ""); comments.set("k2", ""); comments.set("k1", ""); comments.set("k0", "");

  Array.from(gebi('controls').elements).forEach((ctl) => {
	// console.log(ctl.id + ": " + ctl.value);
	a = ctl.id.split("-");
	if (a.length == 4 && a[0].startsWith("k")) { // if it's a "k control" (microcode setting):
	  // a[0] is the k-register; a[1] is the field name; a[2] is the AND pattern; a[3] is the shift
	  //console.log(ctl.id + ": " + a[0] + " " + a[1] + " " + a[2] + " " + a[3] + " " + kmap.get(a[0]));
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

/*
const SimplePropertyRetriever = {
  getOwnEnumerables(obj) {
    return this._getPropertyNames(obj, true, false, this._enumerable);
    // Or could use for...in filtered with Object.hasOwn or just this: return Object.keys(obj);
  },
  getOwnNonenumerables(obj) {
    return this._getPropertyNames(obj, true, false, this._notEnumerable);
  },
  getOwnEnumerablesAndNonenumerables(obj) {
    return this._getPropertyNames(
      obj,
      true,
      false,
      this._enumerableAndNotEnumerable,
    );
    // Or just use: return Object.getOwnPropertyNames(obj);
  },
  getPrototypeEnumerables(obj) {
    return this._getPropertyNames(obj, false, true, this._enumerable);
  },
  getPrototypeNonenumerables(obj) {
    return this._getPropertyNames(obj, false, true, this._notEnumerable);
  },
  getPrototypeEnumerablesAndNonenumerables(obj) {
    return this._getPropertyNames(
      obj,
      false,
      true,
      this._enumerableAndNotEnumerable,
    );
  },
  getOwnAndPrototypeEnumerables(obj) {
    return this._getPropertyNames(obj, true, true, this._enumerable);
    // Or could use unfiltered for...in
  },
  getOwnAndPrototypeNonenumerables(obj) {
    return this._getPropertyNames(obj, true, true, this._notEnumerable);
  },
  getOwnAndPrototypeEnumerablesAndNonenumerables(obj) {
    return this._getPropertyNames(
      obj,
      true,
      true,
      this._enumerableAndNotEnumerable,
    );
  },
  // Private static property checker callbacks
  _enumerable(obj, prop) {
    return Object.prototype.propertyIsEnumerable.call(obj, prop);
  },
  _notEnumerable(obj, prop) {
    return !Object.prototype.propertyIsEnumerable.call(obj, prop);
  },
  _enumerableAndNotEnumerable(obj, prop) {
    return true;
  },
  // Inspired by http://stackoverflow.com/a/8024294/271577
  _getPropertyNames(obj, iterateSelf, iteratePrototype, shouldInclude) {
    const props = [];
    do {
      if (iterateSelf) {
        Object.getOwnPropertyNames(obj).forEach((prop) => {
          //if (props.indexOf(prop) === -1 && shouldInclude(obj, prop)) {
          if (props.indexOf(prop) === -1) {
            props.push(prop);
          }
        });
      }
      if (!iteratePrototype) {
        break;
      }
      iterateSelf = true;
      obj = Object.getPrototypeOf(obj);
    } while (obj);
    return props;
  },
};

// Dump properties of source object
// props = SimplePropertyRetriever._getPropertyNames(sourceObject, true, true, true);
// console.log(`props: ${props}`)
//for (const el of props) {
//  console.log(`| ${el} = ${sourceObject[el]}`);
//}
// end dump properties of source

*/
