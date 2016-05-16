var nano = require('nanomsg'),
	cbor = require('cbor');

// Define the port number to use
var REPLY_URL = 'tcp://*:5555',
	PUBLISH_DEVS_URL = 'tcp://*:6666',
	PUBLISH_CLDS_URL = 'tcp://*:6677',
	SURVEY_DEVS_URL = 'tcp://*:7777',
	SURVEY_CLDS_URL = 'tcp://*:7788';

//==========================================================
// GLOBAL DATA STRUCTURES
// Shhh..! I am using global variables! Need to figure an
// elegant alternative. We will get through this prototype
// with this ugliness!
//==========================================================

var devTable = {},
	cloudTable = {},
	funcRegistry = {};
	portsArray = [];


//==========================================================
// INITIALIZATION SECTION
// Initialize some of the global structures here.
//==========================================================

function setupSystem() {
	// Initialize all available ports
	for (i = 8001; i < 8500; i++)
		portsArray.push(i);

	// TODO: What else needs to be initialized?
}


//==========================================================
// REPLY PROCESSING SECTION
// This is meant to process the request-reply messages
//==========================================================

var rep = nano.socket('rep');
rep.bind(REPLY_URL);

setupSystem();

rep.on('data', function(buf) {
	console.log("Buffer size " + Buffer.byteLength(buf));
	cbor.decodeFirst(buf, function(error, msg) {
		// if error != null .. it seems nanomsg is
		// receiving more bytes than what the C sent..
		// TODO: investigate this pr

		try {
			adminProcessor(rep, msg, function(obj) {
				var encode = cbor.encode(msg);
				rep.send(encode);
				console.log(msg);
			});
		} catch(err) {
			console.log("errror..", err);
		}
	});
});


function adminProcessor(sock, msg, callback) {

	switch (msg["cmd"]) {
		case "REGISTER":
			console.log("REGISTERing...");
			if (msg["opt"] === "DEVICE") {
				var devidrcd = msg["args"][0];
				if (devidrcd !== undefined && devTable[devidrcd] !== undefined) {
				 	var devid = getAlternateID(msg, devidrcd);
				 	var opt = "ALT";
				} else {
				 	var opt = "ORI"
					devid = devidrcd;
				}
				var port = getAPort();
				console.log("Calling start req service.." + port);
				startReqService(port, function(err) {
				 	if (err === null)
				 		insertDevTableEntry(msg, devid, port);
				 	else
				 		opt = "ERR";
					msg = makeNewMsg(msg, "REGISTERED", opt, devid, port);
				 	console.log(msg);
				 	callback(msg);
				});
			}
		break;
		default:
			console.log("Command unknown..." + msg["cmd"]);
		break;
	}

}


function startReqService(port, callback) {

	var sock = nano.socket('rep');
	var url = "tcp://127.0.0.1:" + port;
	sock.bind(url);

	sock.on('data', function(buf) {
		console.log("In new service.. Buffer size " + Buffer.byteLength(buf));
		cbor.decodeFirst(buf, function(error, msg) {
			console.log(msg);

		});
	});

	console.log("Started the service...at:" + url);
	callback(null);
}

function getAlternateID(msg, devid)
{
	var aname = msg["actname"],
		dname = msg["actid"];

	var indx = 1;

	do {
		var tempid = aname + "|" + dname +"(" +  indx + ")";
		indx++;
	} while (devTable[tempid] !== undefined);

	console.log("ALt id " + tempid);
	return tempid;
}

function insertDevTableEntry(msg, devid, port) {

	var entry = {"app_name": msg["actname"], "device_name": msg["actid"], "port": port};
	console.log("Entry ... for devid", devid);
	console.log(entry);
	devTable[devid] = entry;
}

function makeNewMsg(msg, cmd, opt, devid, port) {
	msg["cmd"] = cmd;
	msg["opt"] = opt;
	msg["args"] = [ port, devid];

	return msg;
}

function getAPort()
{
	return portsArray.shift();
}


function releasePort(port)
{
	portsArray.push(port);
}
