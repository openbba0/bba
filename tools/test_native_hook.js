

function contains(text, part) {
  return text != null && text.indexOf(part) >= 0;
}

console.log("native-hook libz.so!deflate");

Interceptor.attach("libz.so!deflate", {
  onEnter: function (args) {
    var params;
    if (args[0] == 0) return;

    try {
      var input = args[0].readPointer();
      if ((input.valueOf() & 0xffffffff) == 0) return;
      params = input.readCString();
      if (!contains(params, "x98")) return;
      console.log("deflate in " + params);
    } catch (e) {
      console.log("deflate in err " + e);
    }
  }
});
console.log("deflate attached");
