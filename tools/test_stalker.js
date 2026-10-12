var started = false;
var finished = false;
console.log("stalker strlen");
Interceptor.attach("libz.so!deflate", {
  onEnter: function (args) {
    if (started) {
      return;
    }
    started = true;
    Stalker.follow(0, {
      events: { exec: true },
      onReceive: function (events) {
        var list = Stalker.parse(events);
        var i;
        var n = list.length;
        if (n > 64) {
          n = 64;
        }
        for (i = 0; i < n; i++) {
          console.log(list[i].location.toString(16) + "  " + Stalker.disasm(list[i].location));
        }
        if (list.length > n) {
          console.log("exec truncated " + list.length);
        }
      }
    });
  },
  onLeave: function () {
    if (!started || finished) {
      return;
    }
    finished = true;
    Stalker.unfollow(0);
    Stalker.flush();
    console.log("stalker done");
  }
});
console.log("stalker attached");
