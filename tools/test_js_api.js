var R = [];
var liveListener = null;
var live = {
  hits: 0,
  tid: 0,
  depth: -1,
  ra: 0,
  a0: "",
  leave: "",
  stalker: 0,
  stalkerType: "",
  stalkerErr: "",
  bt: 0,
  btPc: 0,
  btErr: ""
};

function check(name, ok, detail) {
  var line = (ok ? "PASS " : "FAIL ") + name;
  if (detail !== undefined && detail !== "") {
    line += " " + detail;
  }
  R.push(line);
  console.log(line);
}

function throws(name, fn) {
  try {
    fn();
    check(name, false, "no throw");
  } catch (e) {
    check(name, true, String(e));
  }
}

function near(a, b) {
  return a > b - 0.001 && a < b + 0.001;
}

try {
  var mem = Memory.alloc(256);
  check("Memory.alloc", mem && !mem.isNull(), mem.toString());

  check("NULL.isNull", NULL.isNull() === true);
  check("NULL.equals", NULL.equals(ptr(0)) && NULL.equals(0));
  check("ptr string", ptr("0x20").toInt32() === 32, ptr("0x20").toString());
  check("NativePointer", NativePointer(32).toInt32() === 32);
  check("toString 10", ptr(255).toString(10) === "255");
  check("toString 16", ptr(255).toString(16) === "0xff");
  check("valueOf", ptr(7).valueOf() === 7);
  check("equals", ptr(3).equals(ptr(3)) && ptr(3).equals(3) && !ptr(3).equals(4));
  check("compare", ptr(2).compare(1) === 1 && ptr(1).compare(2) === -1 && ptr(3).compare(3) === 0);
  check("isNull", ptr(0).isNull() && !ptr(1).isNull());
  check("add/sub", ptr(100).add(5).sub(3).toInt32() === 102);
  check("and/or/xor", ptr(0xff).and(0x0f).toInt32() === 0x0f && ptr(0x1).or(0x2).toInt32() === 3 && ptr(0x5).xor(0x1).toInt32() === 4);
  check("shl/shr", ptr(1).shl(4).toInt32() === 16 && ptr(16).shr(2).toInt32() === 4);
  check("not", ptr(0).not().equals(ptr("0xffffffffffffffff")));

  mem.writeU8(0xff);
  check("read/writeU8", mem.readU8() === 255);
  check("readS8", mem.readS8() === -1);
  mem.writeU16(0x1234);
  check("read/writeU16", mem.readU16() === 0x1234);
  mem.writeU16(-2);
  check("readS16", mem.readS16() === -2);
  mem.writeU32(0x89abcdef);
  check("read/writeU32", mem.readU32() === 0x89abcdef);
  mem.writeU32(-3);
  check("readS32", mem.readS32() === -3);
  mem.writeU64(0x100000001n);
  check("read/writeU64", mem.readU64() === 0x100000001n, String(mem.readU64()));
  mem.writeU64(-5);
  check("readS64", mem.readS64() === -5, String(mem.readS64()));
  mem.writeFloat(1.5);
  check("read/writeFloat", near(mem.readFloat(), 1.5), String(mem.readFloat()));
  mem.writeDouble(2.25);
  check("read/writeDouble", near(mem.readDouble(), 2.25), String(mem.readDouble()));
  mem.writeUtf8String("hello");
  check("read/writeUtf8String", mem.readUtf8String() === "hello");
  check("readCString", mem.readCString() === "hello");
  var bytes = new ArrayBuffer(4);
  var view = new Uint8Array(bytes);
  view[0] = 1;
  view[1] = 2;
  view[2] = 3;
  view[3] = 4;
  mem.writeByteArray(bytes);
  var back = new Uint8Array(mem.readByteArray(4));
  check("read/writeByteArray", back[0] === 1 && back[1] === 2 && back[2] === 3 && back[3] === 4);
  mem.writePointer(ptr(0x42));
  check("read/writePointer", mem.readPointer().toInt32() === 0x42);

  var slot = mem.add(64);
  Memory.writeU32(slot, 0x11223344);
  check("Memory.read/writeU32", Memory.readU32(slot) === 0x11223344);
  Memory.writeUtf8String(slot, "mem");
  check("Memory.read/writeUtf8String", Memory.readUtf8String(slot) === "mem");
  Memory.writePointer(slot, ptr(9));
  check("Memory.read/writePointer", Memory.readPointer(slot).toInt32() === 9);
  Memory.writeFloat(slot, 3.5);
  check("Memory.read/writeFloat", near(Memory.readFloat(slot), 3.5));
  Memory.writeDouble(slot, 4.5);
  check("Memory.read/writeDouble", near(Memory.readDouble(slot), 4.5));

  var strlen = Module.findExportByName(null, "strlen");
  var strlenLibc = Module.getExportByName("libc.so", "strlen");
  check("Module.findExportByName null", strlen !== null && !strlen.isNull(), strlen.toString());
  check("Module.findExportByName libc", strlenLibc !== null && strlenLibc.equals(strlen));
  check("Module.findExportByName missing", Module.findExportByName("libc.so", "no_such_spindle_sym") === null);
  throws("Module.getExportByName missing", function () {
    Module.getExportByName("libc.so", "no_such_spindle_sym");
  });
  var tid = Process.getCurrentThreadId();
  check("Process.getCurrentThreadId", tid > 0, String(tid));
  var bt = Thread.backtrace();
  check("Thread.backtrace", bt.length > 0 && bt[0] && !bt[0].isNull(), String(bt.length));
  var fuzzyBt = Thread.backtrace(null, Backtracer.FUZZY);
  check("Thread.backtrace fuzzy", fuzzyBt.length > 0 && !fuzzyBt[0].isNull(), String(fuzzyBt.length));
  throws("Thread.backtrace bad context", function () {
    Thread.backtrace(ptr(1));
  });

  check("console.log", console.log("console.log") === undefined);
  check("console.warn", console.warn("console.warn") === undefined);
  check("console.error", console.error("console.error") === undefined);
  check("console.debug", console.debug("console.debug") === undefined);
  check("rpc.unlock", typeof rpc.unlock === "function");

  throws("Interceptor.attach 0", function () {
    Interceptor.attach(ptr(0), { onEnter: function () {} });
  });
  throws("Interceptor.attach no callback", function () {
    Interceptor.attach(strlen, {});
  });
  var probe = Interceptor.attach(strlen, function () {});
  check("Interceptor.attach function", probe && typeof probe.detach === "function");
  probe.detach();
  var named = Interceptor.attach("libc.so!strlen", { onEnter: function () {} });
  check("Interceptor.attach lib!sym", named && typeof named.detach === "function");
  named.detach();
  check("Interceptor.flush", Interceptor.flush() === undefined);
  check("Interceptor.detachAll", Interceptor.detachAll() === undefined);

  throws("Stalker.transform", function () {
    Stalker.follow(Process.getCurrentThreadId(), { transform: function () {} });
  });
  check("Stalker.exclude", Stalker.exclude(ptr(0x1000), 0x1000) === undefined);
  check("Stalker.exclude object", Stalker.exclude({ base: ptr(0x2000), size: 16 }) === undefined);
  var asm = Stalker.disasm(strlen);
  check("Stalker.disasm", typeof asm === "string" && asm.length > 0, asm);
  throws("Stalker.parse", function () {
    Stalker.parse(0);
  });
  check("Stalker.unfollow idle", Stalker.unfollow() === undefined);
  check("Stalker.flush idle", Stalker.flush() === undefined);

  liveListener = Interceptor.attach(strlen, {
    onEnter: function (args) {
      if (live.hits > 0) {
        return;
      }
      live.tid = this.threadId;
      live.depth = this.depth;
      live.ra = this.returnAddress.isNull() ? 0 : 1;
      live.a0 = args[0].toString();
      args[0] = args[0];
      try {
        var frames = Thread.backtrace(this.context, Backtracer.ACCURATE);
        live.bt = frames.length;
        live.btPc = this.context && this.context.pc && !this.context.pc.isNull() ? 1 : 0;
      } catch (e) {
        live.btErr = String(e);
      }
      try {
        Stalker.follow(this.threadId, {
          events: { compile: true, call: true },
          onReceive: function (events) {
            var list = Stalker.parse(events);
            live.stalker += list.length;
            if (list.length > 0 && !live.stalkerType) {
              live.stalkerType = list[0].type;
            }
          },
          onCallSummary: function (summary) {
            live.summary = summary ? 1 : 0;
          }
        });
      } catch (e) {
        live.stalkerErr = String(e);
      }
    },
    onLeave: function (retval) {
      if (live.hits > 0) {
        return;
      }
      var v = retval[0];
      retval.replace(v);
      live.leave = retval[0].toString();
      Stalker.unfollow();
      Stalker.flush();
      live.hits = 1;
      console.log("LIVE strlen tid=" + live.tid + " depth=" + live.depth + " ra=" + live.ra + " leave=" + live.leave + " stalker=" + live.stalker + " " + live.stalkerType);
    }
  });
} catch (e) {
  check("script", false, String(e));
  console.log("SCRIPT EX " + e);
}

function liveLines() {
  var lines = [];
  lines.push((live.hits > 0 ? "PASS" : "FAIL") + " Interceptor.onEnter/onLeave hits " + live.hits);
  lines.push((live.tid > 0 ? "PASS" : "FAIL") + " Interceptor.this.threadId " + live.tid);
  lines.push((live.depth >= 0 ? "PASS" : "FAIL") + " Interceptor.this.depth " + live.depth);
  lines.push((live.ra === 1 ? "PASS" : "FAIL") + " Interceptor.this.returnAddress");
  lines.push((live.bt > 0 && live.btPc === 1 ? "PASS" : "FAIL") + " Thread.backtrace(this.context) " + live.bt);
  if (live.btErr) {
    lines.push("FAIL Thread.backtrace " + live.btErr);
  }
  lines.push((live.a0 !== "" ? "PASS" : "FAIL") + " Interceptor.args writeback " + live.a0);
  lines.push((live.leave !== "" ? "PASS" : "FAIL") + " retval.replace " + live.leave);
  lines.push((live.stalker > 0 ? "PASS" : "FAIL") + " Stalker.follow/parse/flush " + live.stalker + " " + live.stalkerType);
  if (live.stalkerErr) {
    lines.push("FAIL Stalker.live " + live.stalkerErr);
  }
  return lines;
}

rpc.exports.report = function () {
  if (liveListener) {
    liveListener.detach();
    liveListener = null;
  }
  Stalker.unfollow();
  return R.concat(liveLines()).join("\n");
};

rpc.exports.doUnlock = function () {
  var r = rpc.unlock();
  return r === undefined ? "undefined" : String(r);
};

rpc.exports.javaProbe = function () {
  var lines = [];
  function note(name, ok, detail) {
    var line = (ok ? "PASS " : "FAIL ") + name;
    if (detail !== undefined && detail !== "") {
      line += " " + detail;
    }
    lines.push(line);
    console.log(line);
  }
  function expectThrow(name, fn) {
    try {
      fn();
      note(name, false, "no throw");
    } catch (e) {
      note(name, true, String(e));
    }
  }
  try {
    expectThrow("Java.original", function () {
      Java.original();
    });
    expectThrow("Java.hook argc", function () {
      Java.hook("java.lang.String");
    });
    expectThrow("Java.unhook argc", function () {
      Java.unhook("java.lang.String");
    });
    note("Java.str null", Java.str(null) === "null");
    var Str = Java.use("java.lang.String");
    note("Java.use", Str && Str.$className === "java.lang.String" && typeof Str.$new === "function");
    var text = Str.$new();
    note("Java.$new", text === "" || (text && typeof text.toString === "function"), String(text));
    note("Java.str", String(text) === "" || text.toString() === "");
    Java.hook("java.lang.Thread", "activeCount", "()I", {
      onEnter: function () {},
      onLeave: function (retval) {
        return retval;
      }
    });
    Java.unhook("java.lang.Thread", "activeCount", "()I");
    note("Java.hook/unhook", true);
  } catch (e) {
    note("Java", false, String(e));
  }
  return lines.join("\n");
};

console.log("SYNC DONE " + R.length);
