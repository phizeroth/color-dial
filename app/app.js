var Firebase = require("firebase");
var express = require("express");

// Create HTTP Server
var app = express();
var server = require("http").createServer(app);
// Attach Socket.io server 
var io = require("socket.io")(server);
// Indicate port 5000 as host
var port = process.env.PORT || 5000;

// Create a new firebase reference
var config = {
  apiKey: "AIzaSyDNEKm6Ob1AEzCTcobqbp68joSc0i-tjKk",
  authDomain: "firepot-phiz.firebaseapp.com",
  databaseURL: "https://firepot-phiz.firebaseio.com",
  projectId: "firepot-phiz",
  storageBucket: "firepot-phiz.appspot.com",
  messagingSenderId: "530141388106"
};

Firebase.initializeApp(config)

var firebaseRef = Firebase.database().ref("rgb");

server.listen(port, function() {
  console.log("Server listening on port %d", port);
});

// Routing to static files
app.use("/rgb/", express.static(__dirname + "/public"));
console.log(__dirname);

// Socket server listens on connection event
io.on("connection", function(socket) {
  console.log("Connected and ready!");
  
  // firebase reference listens on value change, 
  // and return the data snapshot as an object
  firebaseRef.on("value", function(snapshot) {
    var colorChange = snapshot.val();
    
    // Print the data object's values
    console.log("snapshot R: " + colorChange.r);
    console.log("snapshot G: " + colorChange.g);
    console.log("snapshot B: " + colorChange.b);
  });
});


