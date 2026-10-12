// spindle Java.hook smoke test
console.log("java-hook test start");

function printStack(tag) {
  try {
    var th = Java._new("java.lang.Throwable", "()V", []);
    var text = Java._invoke(
      "android.util.Log",
      "getStackTraceString",
      "(Ljava/lang/Throwable;)Ljava/lang/String;",
      null,
      [th],
      1
    );
    var lines = String(text).split("\n");
    var i;
    console.log(tag + " stack");
    for (i = 0; i < lines.length && i < 40; i++) {
      if (lines[i]) {
        console.log(lines[i]);
      }
    }
  } catch (e) {
    console.log(tag + " stack err " + e);
  }
}

Java.hook("android.app.Activity", "onCreate", "(Landroid/os/Bundle;)V", {
  onEnter: function (args) {
    console.log("Activity.onCreate enter this=" + Java.str(args[0]));
    printStack("onCreate");
  },
  onLeave: function (retval) {
    console.log("Activity.onCreate leave");
  }
});

console.log("java-hook attached — open any Activity to fire onCreate");
