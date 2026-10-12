rpc.exports.ping = function (s) {
  return "pong " + s;
};

rpc.exports.add = function (a, b) {
  return a + b;
};

console.log("rpc exports ready");
