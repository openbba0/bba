///<reference path='/home/dennydell/node_modules/@types/frida-gum/index.d.ts' />

function contains(text, part) {
  return text != null && text.indexOf(part) >= 0;
}

console.log("native-hook libz.so!deflate");

var addr = Module.getExportByName("libz.so", "deflate")
console.log("addr", addr);
var module = Process.getModuleByName("libz.so")
console.log("base addr", module.base, module.name);
console.log("ptr test:", ptr(module.base).readPointer())

var addr_deflate = module.getExportByName("deflate")

Interceptor.attach(addr_deflate.add(8), {
  onEnter: function (args) {
    console.log("Come in offset hook");
}});

// Interceptor.attach("libz.so!deflate", {
//   onEnter: function (args) {
//     var params;
//     if (args[0] == 0) return;

//     try {
//       var input = args[0].readPointer();
//       if ((input.valueOf() & 0xffffffff) == 0) return;
//       params = input.readCString();
//       //if (!contains(params, "\"x98\"")) return;
//       console.log("deflate in " + params);
//     } catch (e) {
//       console.log("deflate in err " + e);
//     }
//   }
// });
console.log("deflate attached");
